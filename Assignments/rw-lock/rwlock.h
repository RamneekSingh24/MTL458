#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

struct read_write_lock
{ 
	sem_t rLock, resourceLock;
	int num_readers;

	// Needed for writer-pref lock
	sem_t wLock, queueLock; // can't used same lock inplace of rLock and wLock, Deadlock will occur!
	int num_writers;
};

void InitalizeReadWriteLock(struct read_write_lock * rw);
void ReaderLock(struct read_write_lock * rw);
void ReaderUnlock(struct read_write_lock * rw);
void WriterLock(struct read_write_lock * rw);
void WriterUnlock(struct read_write_lock * rw);
