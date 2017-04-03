//==============================================================================
// 2017 Dmitry.Frantsev@gmail.com
//==============================================================================
#ifdef WIN32
	#include <windows.h>
	#include <io.h>
#else
	#include <signal.h>
	#include <sys/mman.h>
	#include <unistd.h>
	#include <pthread.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "SharedQueue.h"

#ifndef LEVEL1_DCACHE_LINESIZE
#define LEVEL1_DCACHE_LINESIZE	128
#endif

#ifdef __GNUC__
	#define readBarrier()		__sync_synchronize()
	#define writeBarrier()		__sync_synchronize()
#else
	#include <intrin.h>
	#define readBarrier()		_ReadBarrier()
	#define writeBarrier()		_WriteBarrier()
#endif

#define MAX_QUEUE_NAME			64
#define MQI_MAGIC				0x12345678UL


#define ALIGN(i)				(((i) + LEVEL1_DCACHE_LINESIZE-1) & ~(LEVEL1_DCACHE_LINESIZE-1))
#define ALIGN_PTR(p)			(((uintptr_t)(p) + LEVEL1_DCACHE_LINESIZE-1) & ~(LEVEL1_DCACHE_LINESIZE-1))

//==============================================================================
// TSharedMemHeader
//==============================================================================
struct TSharedMemHeader
{
	union
	{
		struct
		{
		volatile unsigned long	m_magic;		// Magic number
		volatile size_t			m_used;			//
		size_t					m_maxElmCount;	// Maximum number of the elements in the queue.
		size_t					m_maxElmSize;	// Maximum size of an element.
		};
		char					pad1[LEVEL1_DCACHE_LINESIZE];
	};
	union
	{
		volatile size_t			m_head;
		char					pad2[LEVEL1_DCACHE_LINESIZE];
	};
	union
	{
		volatile size_t			m_tail;
		char					pad3[LEVEL1_DCACHE_LINESIZE];
	};
};


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// TSPSCSharedQueuePrivate
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class TSPSCSharedQueuePrivate
{
protected:
	void				*m_shared;					// Base pointer to shared memory.
	TSharedMemHeader	*m_hdr;						// Aligned pointer to header in shared memory.
	char				*m_data;					// Aligned pointer to slots.
	size_t				m_slotSize;					// Precalculated slot (header + element) size.
	size_t				m_memSize;					// Precalculated total memory size.
	char				m_name[MAX_QUEUE_NAME+2];	// Name of the queue.
#ifdef WIN32
	HANDLE				m_fmap;						// file mapping object
#endif
public:
	TSPSCSharedQueuePrivate();
	~TSPSCSharedQueuePrivate();
	inline bool			open(const char* name, size_t elmSize, size_t elmCount, bool create);
	inline bool			close();
	inline bool			push(size_t elmSize, const char* elm);
	inline size_t		pop(char* elm);
protected:
	inline void			reset();
};
//==============================================================================
TSPSCSharedQueuePrivate::TSPSCSharedQueuePrivate()
{
	reset();
}

//==============================================================================
TSPSCSharedQueuePrivate::~TSPSCSharedQueuePrivate()
{
	close();
}

//==============================================================================
void TSPSCSharedQueuePrivate::reset()
{
	m_shared = NULL;
	m_hdr = NULL;
	m_data = NULL;
	m_slotSize = 0;
	m_memSize = 0;
	m_name[0] = 0;
#ifdef WIN32
	m_fmap = 0;
#endif
}

//==============================================================================
inline bool TSPSCSharedQueuePrivate::open(const char* queueName, size_t elmSize, size_t elmCount, bool create)
{
	if (m_shared)
		return true;

	if (!queueName || queueName[0]==0 || elmCount==0 || elmSize==0)
		return false;

	size_t	len = strlen(queueName);
	if (len >= MAX_QUEUE_NAME)
		return false;

	m_slotSize = ALIGN(elmSize+sizeof(size_t));
	m_memSize = ALIGN(sizeof(struct TSharedMemHeader)) + m_slotSize *(elmCount+1) + LEVEL1_DCACHE_LINESIZE;
	if (queueName[0]!='/')
	{
		m_name[0] = '/';
		m_name[1] = 0;
		++len;
	}
	strcat(m_name, queueName);

#ifdef WIN32
	wchar_t	nameBuffer[256];
	// Open or create a new shared memory region.
	lstrcpyW(nameBuffer, TEXT("Global\\Mem"));
	len = lstrlenW(nameBuffer);
	mbstowcs(nameBuffer+len, m_name+1, sizeof(nameBuffer)-len);
	m_fmap = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, m_memSize, nameBuffer);
	if (!m_fmap)
	{
		close();
		return false;
	}

	m_shared = MapViewOfFile(m_fmap, FILE_MAP_ALL_ACCESS, 0, 0, m_memSize);
	if (!m_shared)
	{
		close();
		return false;
	}
#else
	int fmap;
	if (create)
	{
		shm_unlink(m_name);
		fmap = shm_open(m_name, O_CREAT | O_RDWR | O_EXCL, (S_IRUSR | S_IWUSR) );
		if ( fmap == -1 )
		{
			close();
			return false;
		}
		if (ftruncate( fmap, m_memSize )==-1)
		{
			close();
			return false;
		}
	}
	else
	{
		fmap = shm_open(m_name, O_RDWR, (S_IRUSR | S_IWUSR) );
		if ( fmap == -1 )
		{
			close();
			return false;
		}
	}
	m_shared = mmap(NULL, m_memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fmap, 0 );
	::close(fmap);

	if ( m_shared == MAP_FAILED )
	{
		m_shared = NULL;
		close();
		return false;
	}
#endif
	// Make an aligned pointer to shared memory header.
	m_hdr = (TSharedMemHeader*)ALIGN_PTR(m_shared);
	// First time creation?
	if (m_hdr->m_magic != MQI_MAGIC || create)
	{
		m_hdr->m_magic = MQI_MAGIC;
		m_hdr->m_used = 1;
		m_hdr->m_maxElmCount = elmCount + 1;
		m_hdr->m_maxElmSize = elmSize;
		m_hdr->m_head = 0;
		m_hdr->m_tail = 0;
	}
	else
	{
		if (m_hdr->m_maxElmCount!=elmCount + 1 ||
			m_hdr->m_maxElmSize!=elmSize)
		{
			close();
			return false;
		}
		m_hdr->m_used++;
	}
	m_data = (char*)m_hdr + ALIGN(sizeof(TSharedMemHeader));

	return true;
}

//==============================================================================
inline bool TSPSCSharedQueuePrivate::close()
{
	size_t	used;
	if (m_hdr && m_hdr->m_magic == MQI_MAGIC)
	{
		m_hdr->m_used--;
		used = m_hdr->m_used;
	}
#ifdef WIN32
	if (m_shared)
		UnmapViewOfFile(m_shared);
	if (m_fmap)
	{
		CloseHandle(m_fmap);
		m_fmap = NULL;
	}
#else
	if (m_shared)
		munmap(m_shared, m_memSize);
	// Close and remove shared memory if there is no one.
	if (!used && m_name[0])
		shm_unlink(m_name);
#endif
	reset();

	return true;
}

//==============================================================================
inline bool TSPSCSharedQueuePrivate::push(size_t elmSize, const char* elm)
{
	if (!m_shared || m_hdr->m_magic!=MQI_MAGIC || elmSize>m_hdr->m_maxElmSize)
		return false;
	readBarrier();
	size_t	tail = m_hdr->m_tail;
	size_t	nextTail = (tail+1) % m_hdr->m_maxElmCount;
	// Is there space?
	if (nextTail==m_hdr->m_head)
		return false;
	// Calcalate the slot address.
	char	*ptr = m_data + m_slotSize * tail;
	// Put data in the slot.
	memmove(ptr, &elmSize, sizeof(size_t));
	ptr += sizeof(size_t);
	memmove(ptr, elm, elmSize);
	writeBarrier();
	// Move the tail.
	m_hdr->m_tail = nextTail;
	return true;
}

//==============================================================================
inline size_t TSPSCSharedQueuePrivate::pop(char* elm)
{
	if (!m_shared || m_hdr->m_magic!=MQI_MAGIC)
		return 0;
	// Is there any data.
	readBarrier();
	size_t	head = m_hdr->m_head;
	if (head==m_hdr->m_tail)
		return 0;
	// Calcalate the slot address.
	char	*ptr = m_data + m_slotSize * head;
	size_t	len;
	// Get data size.
	memmove(&len, ptr, sizeof(size_t));
	ptr += sizeof(size_t);
	if (len>m_hdr->m_maxElmSize)
		return 0;
	// Get data.
	memmove(elm, ptr, len);
	// Move the head of the queue.
	m_hdr->m_head = (head + 1) % m_hdr->m_maxElmCount;
	writeBarrier();
	return len;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// TSPSCSharedQueue
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
TSPSCSharedQueue::TSPSCSharedQueue()
{
	m_pQueue = new TSPSCSharedQueuePrivate();
}

//==============================================================================
TSPSCSharedQueue::~TSPSCSharedQueue()
{
	delete m_pQueue;
}

//==============================================================================
bool TSPSCSharedQueue::open(const char* name, size_t elmSize, size_t elmCount, bool create)
{
	return m_pQueue->open(name, elmSize, elmCount, create);
}

//==============================================================================
bool TSPSCSharedQueue::close()
{
	return m_pQueue->close();
}

//==============================================================================
bool TSPSCSharedQueue::push(size_t elmSize, const char* elm)
{
	return m_pQueue->push(elmSize, elm);
}

//==============================================================================
size_t TSPSCSharedQueue::pop(char* elm)
{
	return m_pQueue->pop(elm);
}
