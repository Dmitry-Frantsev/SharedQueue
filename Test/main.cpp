#ifdef WIN32
	#include <windows.h>
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "../Shared/Core/SharedQueue.h"

#define MQ_MAXMSG	1000
#define MSG_SIZE	1500
#define QUEUE_NAME	"queue"

#define CLK2S(clk)	(double(clk)/CLOCKS_PER_SEC)

void sleepMicro(unsigned long usecs)
{
#ifdef WIN32
	DWORD msec = (DWORD)(usecs/1000);
	Sleep(msec);
#else
	struct timespec tm;
	tm.tv_sec = (unsigned long long)usecs/1000000;
	tm.tv_nsec = (unsigned long long)usecs * 1000 - (unsigned long long)tm.tv_sec * 1000000000UL;
	clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, NULL);
#endif
}

//==============================================================================
static int sendFunc()
{
	clock_t				prevTS, currTS;
	char				msg[MSG_SIZE] = {};
	unsigned int		prev = 0;
	unsigned int		counter = 0;
	unsigned int		*pCounter = (unsigned int*)msg;
	unsigned int		*pEndCounter = (unsigned int*)(msg + sizeof(msg) - sizeof(unsigned int));
	TSPSCSharedQueue	sendQueue;

	if (!sendQueue.open(QUEUE_NAME, MSG_SIZE, MQ_MAXMSG))
	{
		perror("SENDER. Unable to open send queue.");
		return -1;
	}

	prevTS = clock();
	while(1)
	{
		bool res = sendQueue.push(sizeof(msg), msg);
		if (!res)
		{
			sleepMicro(0);
			continue;
		}

		*pCounter = counter;
		*pEndCounter = counter;
		++counter;
		if (counter%1000000==0)
		{
			double p;
			currTS = clock();
			p = CLK2S(currTS - prevTS);
			if (p==0)
				p = 0.00000000001;
			prevTS = currTS;
			printf("%g [msg/s]\n", (counter-prev)/p);
			prev = counter;
		}

	}
	return -1;
}

//==============================================================================
static int recvFunc()
{
	char				msg[MSG_SIZE] = {};
	unsigned int		prev = 0;
	unsigned int		*pCounter = (unsigned int*)msg;
	unsigned int		*pEndCounter = (unsigned int*)(msg + sizeof(msg) - sizeof(unsigned int));
	TSPSCSharedQueue	recvQueue;

	if (!recvQueue.open(QUEUE_NAME, MSG_SIZE, MQ_MAXMSG, true))
	{
		perror("RECEIVER. Unable to open recv queue.");
		return -1;
	}

	while(1)
	{
		size_t res = recvQueue.pop(msg);
		if (res<=0)
		{
			sleepMicro(0);
			continue;
		}

		if (*pCounter!=*pEndCounter)
		{
			printf("%u!=%u\n", *pCounter, *pEndCounter);
		}

		if (prev+1!=*pCounter)
		{
			printf("SEQ. %u %u\n", *pCounter, prev);
		}
		prev = *pCounter;
		if (*pCounter%100000==0)
		{
			printf("%u\n", *pCounter);
		}
	}

	return -1;
}

//==============================================================================
int main(int argc, char *argv[])
{
	printf("SharedQueueTest -send|-recv\n");
	if (argc>1 && strcmp(argv[1], "-send")==0)
	{
		return sendFunc();
	}
	if (argc>1 && strcmp(argv[1], "-recv")==0)
	{
		return recvFunc();
	}

	return -1;
}

