#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>

#define KB 1024
#define MIN_BLOCK_SIZE 8 // min block size that can be requested
#define MAGIC 1234567

typedef struct {
	int size;
	int magic;
} header_t;

typedef struct __node_t {
	int size;
	struct __node_t *next;
} node_t;

#define HEADER_SIZE sizeof(header_t)
#define NODE_SIZE sizeof(node_t)

int my_init(void);
void* my_alloc(int size);
void my_free(void* addr);
void my_clean(void);
void my_heapinfo(void);
void my_clean(void);



// Global Variables :- head of the free list pointer 
//                     heapStart pointer for my_clean() call
//                     an int pointer for my_heapinfo()

node_t *head;
int* allocatedCount;
void* heapStart;


int my_init(void) 
{
	void* ptr = mmap(NULL, 4*KB, PROT_READ|PROT_WRITE, 
		MAP_ANON|MAP_PRIVATE, -1, 0); // System call requesting 4KB memory in heap from OS with read 
									  // and write priviliges

	if(ptr == MAP_FAILED){
        return -1;
    }
	// We keep an integer at start of memory to count allocated blocks
	allocatedCount = ptr;
	*allocatedCount = 0;


	// We save the start of heap
	heapStart = ptr;

	// Free-List starts at head 
	head = ptr + sizeof(int);
	
	// MAX-SIZE
	head->size = 4*KB - sizeof(node_t)-sizeof(int); // remaining size after allocating space for head and one int
	head->size += NODE_SIZE - HEADER_SIZE; // actual size that can be given is more as HEADER_SIZE < NODE_SIZE
	head->next = NULL;
	return 0;
}



void* my_alloc(int size)
{
	
	if(size % 8 != 0 || size <=0 || head == NULL) return NULL; // Invalid Cases

	node_t* ptr = head;
	node_t* prev = NULL;

	int f = -1;


	// Let X be size of the block wish to use, We have the following two cases:
	// Note that minimum size of a block is 8 and we can only allocate if size <= X
	// (assuming X >= size)
	// Case 1 : remaining size = X-size < HEADER_SIZE + 8 (minimum: size of a block) in this case we allocate
	//          the remaining size with the given size, as we cannot split, this causes internal fragmentation.
	// Case 2 : otherwise, we split the block into two and use one for allocation and keep other as free.


	while(ptr != NULL) {

		if(ptr->size >= size) {

			if(ptr->size - size < HEADER_SIZE + MIN_BLOCK_SIZE) f = 1; // Case 1
			else f = 0;	// Case 2

			break;

		}

		prev = ptr;
		ptr = ptr->next;
	}

	// ptr is the node in free list that is to be allocated, 
	// prev is the node behind it in the list.

	if(f == -1) {
		//printf("Not enough space\n");
		return NULL;
	} // Not enough contingous space found for the given request.

	else if(f == 0) {     

		// in this case the found block is to be  split into one free block and one allocated block
		// so two headers are needed finally, one for each block.
		// we already have one header, so we need to create only one extra header.

		int rem_size = ptr->size - size - sizeof(node_t); // remaining free space after splitting
		node_t* next_node = ptr->next;

		header_t* alloc_block = ptr;
		alloc_block->size = size;
		alloc_block->magic = MAGIC;

		node_t* free_block = (void*)alloc_block + HEADER_SIZE + size;
		free_block->size = rem_size;
		free_block->size += NODE_SIZE - HEADER_SIZE; // adjusting for difference in header and node sizes.
		free_block->next = next_node;

		if(prev == NULL) head = free_block;
		else prev->next = free_block;

		(*allocatedCount)++;
		alloc_block = alloc_block + 1;
		return (void*) alloc_block;


	}

	else{

		// in this case the found block is used up fully, i.e. it is not to be split
		// we only need one header node for the allocated block
		// which we already have, so no need to create extra

		// We have the following situation
		//  -- () --> () --> () --> prev --> ptr --> ptr->next --> () --> () -->
		// We need to remove ptr from the list, so we assign prev->next = ptr->next

		if(prev == NULL) {   
			// in case ptr = head, prev = NULL
			head = ptr->next;
			
		}

		else {
			prev->next = ptr->next;
		}

		int blockSZ = ptr->size;
		header_t* alloc_block = ptr;
		alloc_block->size = blockSZ;
		alloc_block->magic = MAGIC;
		(*allocatedCount)++; // increasing count of allocated blocks

		return (void*) (alloc_block+1); // return after incrementing pointer to the
										// actual start of allocated mem(after header).

	}



}

void mergeIf(node_t* node1, node_t* node2) 
{	

	// *Merges nodes node1 and node2 in free list if they are consecutive in the given order.
	// We merge as follows:
	// node1 = node1 + node2

	//int add = node1->size + sizeof(node_t);
	int add = node1->size + HEADER_SIZE;

	if((void*) node2  == (void*)node1 + add) {  // condition to check 

		node1->size += node2->size + HEADER_SIZE;
		node1->next = node2->next;
	}
}


void my_free(void *addr) 
{	
;
	if(addr == NULL) return;

	header_t* ptr = (header_t*) addr - 1;    // points towards header of the given block, assuming it is a valid block


	if(ptr->magic != MAGIC) {

		// if the given address is not allocated currently - it is already freed or
		//                                                   addr is an invalid location
		//printf("requested address is not allocated\n");  
		return;			   								
	}
	ptr->magic = 696969;

	// otherwise we free the given addr and add it back to the free list
	int size = ptr->size;
	(*allocatedCount)--; 

	if(head == NULL) {	// if head is NULL i.e. list is empty, we put the block at head
		head = (node_t*)ptr;
		head->size = size;
		head->next = NULL;
		return;
	}


	// we maintain the free list so that the nodes are arranges in increasing order of their adresses
	// so we need to find the appropiate place to add the given block 
	// we take the following situation: ptr is to be added b/w prev and curr
	// ()--> prev --> ptr(to be added) --> curr --> () -->

	node_t* prev = NULL;
	node_t* curr = head;

	//int itrt = 0;

	while(curr != NULL) {

		int add = curr->size + HEADER_SIZE;
		void* end = (void*)curr + add;

		if(end > (void*) ptr) {   // we compare the addresses in virtual memory			
				  				  // to find the correct place
			break;   // we stop when the end of current block > ptr(the block to be freed)
		}
		prev = curr;
		curr = curr->next;
	}

	// currently we have, --> prev --> curr -->
	// we need to add ptr in b/w:-
	// --> prev --> ptr --> curr --> 

	if(prev == NULL) {	// in this case ptr is to be added at start of list
		head = (node_t*)ptr;
		head->size = size;
		head->next = curr;
		mergeIf(head,curr);// we merge ptr and curr if they are consecutive

	}

	else{
		node_t* free_block = ptr;
		free_block->size = size;
		free_block->next = curr;
		prev->next = free_block;
		if(curr != NULL) mergeIf(free_block,curr);  // we merge free_block and curr if they are conseutive
		mergeIf(prev,free_block);
	}
}

void my_heapinfo(void) 
{	
	printf("=== Heap Info ================\n");

	if(head == NULL) return;

	node_t* ptr = head;
	int FREE_SIZE = 0;

	int minSZ = 40000;
	int maxSZ = 0;

	//int NUM_BLOCKS = 0;
	while(ptr != NULL) {
		int FREE_SZ = ptr->size - NODE_SIZE + HEADER_SIZE; // absolute free space, it can be 0(in case of 8 B block)
		if(ptr->size > maxSZ) maxSZ = FREE_SZ;
		if(ptr->size < minSZ) minSZ = FREE_SZ;
		FREE_SIZE += FREE_SZ; // absolute free space, 
														//not the free space that can be allocated
		ptr = ptr->next;
	}

	int MAX_SIZE = 4*KB - sizeof(int);

	printf("Max Size: %d\n", MAX_SIZE);

//Prints maximum size of the heap(in bytes) which is (4KB - any data structure/variable used to maintain heap info Q(e)).


	printf("Current Size: %d\n", MAX_SIZE - FREE_SIZE);

//Prints size occupied(in bytes)(including header)


 	printf("Free Memory: %d\n", FREE_SIZE);

//Prints unoccupied space(in bytes)


 	printf("Blocks allocated: %d\n", *allocatedCount);

//Prints number of different memory blocks allocated (Note this is not the number of nodes in free list)

 	if(minSZ == 40000) minSZ = 0;
 	printf("Smallest available chunk: %d\n", minSZ);

//Prints size of smallest available chunk in the heap(in bytes)


 	printf("Largest available chunk: %d\n", maxSZ);

 	//printf("Number of blocks: %d\n", NUM_BLOCKS);
//Prints size of the largest available chunk in the heap(in bytes)
 	printf("==============================\n");

}

void my_clean(){
    int temp = munmap(heapStart, 4096);
    if(temp != 0){
        //perror("unmap");
        return;
    }
}


