//==============================================================================
// 2017 Dmitry.Frantsev@gmail.com
//==============================================================================
#ifndef SHAREDQUEUE_H_INCLUDED
#define SHAREDQUEUE_H_INCLUDED


class TSPSCSharedQueuePrivate;
class TSPSCSharedQueue
{
protected:
	TSPSCSharedQueuePrivate	*m_pQueue;
	TSPSCSharedQueue(const TSPSCSharedQueuePrivate&);
	TSPSCSharedQueue& operator=(const TSPSCSharedQueue&);
public:
					TSPSCSharedQueue();
					~TSPSCSharedQueue();
	bool			open(const char* name, size_t elmSize, size_t elmCount, bool create = false);
	bool			close();
	bool			push(size_t elmSize, const char* elm);
	size_t			pop(char* elm);
};

#endif // SHAREDQUEUE_H_INCLUDED
