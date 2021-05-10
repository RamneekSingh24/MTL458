#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock * rw)
{
	int rc = sem_init(&rw->rLock, 0, 1);
	rc += sem_init(&rw->wLock, 0, 1);
	rc += sem_init(&rw->resourceLock, 0, 1);
	rc += sem_init(&rw->queueLock, 0, 1);

	if(rc != 0) {
		printf("Initalization Failed, Please Retry\n");
		exit(1);
	}

	rw->num_readers = 0;
}

void ReaderLock(struct read_write_lock * rw)
{	
	sem_wait(&rw->queueLock);    // Aquire the lock for getting the resource lock
								 // This is needed to to stop readers in case a writer 
								 // is waiting to aquire the lock.

	sem_wait(&rw->rLock);		 // Lock needed so that readers access the shared data one at a time.
	if(1 == ++rw->num_readers) {
		sem_wait(&rw->resourceLock);  // If it was the first reader, then the resource will be locked(for writers)
	}
	sem_post(&rw->rLock);

	sem_post(&rw->queueLock);
}

void ReaderUnlock(struct read_write_lock * rw)
{
	sem_wait(&rw->rLock);			 // General lock needed so the readers access the shared data one at a time.
	if(0 == --rw->num_readers) {
		sem_post(&rw->resourceLock); // The last reader will remove the lock from resource, 
	}								 // now writers are allowed to write.
	sem_post(&rw->rLock);
}

void WriterLock(struct read_write_lock * rw)
{
	sem_wait(&rw->wLock);		    // General lock needed so the writers access the shared data one at a time.
	if(1 == ++rw->num_writers) {
		sem_wait(&rw->queueLock);   // The first writer to request the lock will block the upcoming readers
	}								// from aquiring the lock using queuelock.
	sem_post(&rw->wLock);

	sem_wait(&rw->resourceLock);	// Writers will wait here for the resource lock.

	/*
		Another possible implementation is that each writer aquires the queuelock and gives it up
		as soon as it aquires the resource lock. However such implementation will not 
		be writer baised in same way as the other version is baised towards readers. A stream of writers, will
		not starve the readers as after the first writer gives up the queuelock any reader/writer
		waiting in the queue could be by up and aquire the lock.
		Current implementation prevents this by keeping track of number of writers and only allowing
		the last writer to leave the queue lock.Here the writers hold the queue lock much the same way
		the readers hold the resource lock only allowing writers access once all the readers are finished.
	*/
}

void WriterUnlock(struct read_write_lock * rw)
{
	sem_post(&rw->resourceLock);	// Writer will give away the resource lock.

	sem_wait(&rw->wLock);		
	if(0 == --rw->num_writers) {
		sem_post(&rw->queueLock);	// The last writer will leave the queue lock allowing readers
	}								// to aquire the resource lock.
	sem_post(&rw->wLock);

}
