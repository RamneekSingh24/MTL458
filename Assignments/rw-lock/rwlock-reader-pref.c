#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock * rw)
{	
	int rc =sem_init(&rw->rLock, 0, 1);
	rc += sem_init(&rw->resourceLock, 0, 1);

	if(rc != 0) {
		printf("Initalization Failed, Please Retry\n");
		exit(1);
	}

	rw->num_readers = 0;
}

void ReaderLock(struct read_write_lock * rw)
{
	sem_wait(&rw->rLock);
	if(1 == ++rw->num_readers) {
		sem_wait(&rw->resourceLock);
	}
	sem_post(&rw->rLock);
}

void ReaderUnlock(struct read_write_lock * rw)
{
	sem_wait(&rw->rLock);
	if(0 == --rw->num_readers) {
		sem_post(&rw->resourceLock);
	}
	sem_post(&rw->rLock);
}

void WriterLock(struct read_write_lock * rw)
{
	sem_wait(&rw->resourceLock);
}

void WriterUnlock(struct read_write_lock * rw)
{
	sem_post(&rw->resourceLock);
}
