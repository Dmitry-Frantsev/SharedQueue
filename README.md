# SharedQueue
## Description
Simple fast crossplatform shared queue for IPC.  
This is single producer signle consumer queue.
This source code doesn't require any new modern compiler features like Atomic, CAS etc.  
This class can be compiled by old compilers like gcc 4.4.
Tested on:
- Windows 7 32/64 bit, gcc 4.4
- QNX 6.5, gcc 4.4
- Ubutnu 32 bit, gcc 4.4
- ARM linux, gcc 4.4
- Russian Elbrus e2k, lcc 1.19.21 (gcc 4.4 compatible)  
## How to test
- Run SharedQueueTest -recv
- Run SharedQueueTest -send
## Performance
Queue size is 1000 elements. Element size is 1500 bytes.
- 5500000 msg/sec on Windows 7 64 bit, CPU Intel Core i5 3 GHz
- 5200000 msg/sec on QNX 6.5 (using VMWarePlayer), CPU Intel Core i5 3 GHz
- 4800000 msg/sec on Ubuntu (using VMWarePlayer), CPU Intel Core i5 3 GHz
