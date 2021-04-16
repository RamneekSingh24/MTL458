#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>


#define PAGE_SZ 4 // Page size in KB
#define PN_SHIFT 12 // 4 KB = 2^12 B
#define VM_WIDTH 32 // 32 bit VM address space

char strats[5][6] = {
	"OPT",
	"FIFO",
	"CLOCK",
	"LRU",
	"RANDOM"
};


int NUM_MISSES;
int NUM_DROPS;
int NUM_WRITES;



int NUM_FRAMES;
int verbose_MODE;              
int replacement_stratergy;

int* memory_accesses_locations;
char* memory_accesses_modes;

int16_t* page_table;
int* inverted_page_table;
int free_frame_idx;
int num_free_frames;


int NUM_REQUESTS;
int global_timer;

int queue_idx_FIFO;
int clock_idx;
int* last_used_time;
int** reference_times;




void init(int argc, char* argv[]);
void take_input(char* inputFile);
void init_page_table(void);
void init_frames(void);
void init_OPT(void);
int	evict(int REQUESTED_PN);
void service_mem_access(int addr, char mode);





void init(int argc, char* argv[]) 
{	


	// argv[0], input_file, num_frames, rp_strat, -verbose(flag)

	if(argc < 4) {
		printf("Please provide correct arguments\n");
		exit(1);
	}

	NUM_FRAMES = atoi(argv[2]);
	if(NUM_FRAMES > 1000 || NUM_FRAMES <= 0) {
		printf("Invalid amount of frames\n");
		exit(1);
	}

	verbose_MODE = 0;
	if(argc > 4 && strcmp(argv[4], "-verbose") == 0) {
		verbose_MODE = 1;
	}



	replacement_stratergy = -1;
	for(int i = 0; i < 5; i++) {
		if(strcmp(strats[i], argv[3]) == 0) {
			replacement_stratergy = i;
			break;
		}
	}
	if(replacement_stratergy == -1) {
		printf("Invalid input\n");
		exit(1);
	}


	take_input(argv[1]);

	init_page_table();

	init_frames();

	NUM_WRITES = 0, NUM_MISSES = 0, NUM_DROPS = 0, global_timer = 0;

	//printf("%d\n", replacement_stratergy);

	switch(replacement_stratergy){


		case 0: 
			// OPT
			init_OPT();
			break;
		

		case 1: 
			// FIFO
			queue_idx_FIFO = 0;
			break;
		

		case 2: 
			// CLOCK
			clock_idx = 0;
			break;
		

		case 3: 
			// LRU
			last_used_time = malloc(NUM_FRAMES * sizeof(int));

			for(int i = 0; i < NUM_FRAMES; i++) {
				last_used_time[i] = -1; // time 0 for first access
			}
			break;
		

		case 4: 

			srand(5635);
			break;
		

	}

}

void init_OPT(void) 
{

	int NUM_PAGES = 1 << (VM_WIDTH - PN_SHIFT);

	reference_times = malloc(sizeof(int*) * NUM_PAGES);

	int* num_references = malloc(sizeof(int) * NUM_PAGES);

	for(int i = 0; i < NUM_PAGES; i++) {
		num_references[i] = 0;
	}

	for(int i = 0; i < NUM_REQUESTS; i++) {
		num_references[memory_accesses_locations[i] >> PN_SHIFT]++;
	}

	for(int i = 0; i < NUM_PAGES; i++) {
		reference_times[i] = malloc(sizeof(int) * (num_references[i] + 1));
	}

	for(int i = 0; i < NUM_PAGES; i++) {
		num_references[i] = 0;
	}

	for(int i = 0; i < NUM_REQUESTS; i++) {
		int PN = memory_accesses_locations[i] >> PN_SHIFT;
		reference_times[PN][num_references[PN]++] = i;
	}

	for(int i = 0; i < NUM_PAGES; i++) {
		reference_times[i][num_references[i]] = -1;
	}

	free(num_references);



}

void take_input(char* INPUT_FILE) 
{	

	// Code for reading line taken from manual page
	// https://linux.die.net/man/3/getline

    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(INPUT_FILE, "r");

    if (fp == NULL)
        exit(EXIT_FAILURE);


    int VECTOR_SIZE = 20;
    int idx = 0;

    memory_accesses_locations = malloc((VECTOR_SIZE + 1) * sizeof(int));
    memory_accesses_modes = malloc((VECTOR_SIZE + 1)* sizeof(char));

    NUM_REQUESTS = 0;

    while ((read = getline(&line, &len, fp)) != -1) {


        char* token = strtok(line, " \t");

        if(!token) {
        	printf("Invalid Input\n");
        	exit(1);
        }

        int loc = (int) strtol(token, NULL, 0);


        token = strtok(NULL, " \t");

        if(!token) {
        	printf("Invalid Input\n");
        	exit(1);
        }

        char mode = (char)*token;

        if(!(mode == 'R' || mode == 'W')) {
        	printf("Invalid Access Mode: %c", mode);
        	exit(1);
        }

        ++NUM_REQUESTS;


        if(idx == VECTOR_SIZE) {

        	VECTOR_SIZE *= 2;
        	memory_accesses_locations = realloc(memory_accesses_locations, (VECTOR_SIZE+1) * sizeof(int));
    		memory_accesses_modes = realloc(memory_accesses_modes, (VECTOR_SIZE+1) * sizeof(char));
        }

        memory_accesses_locations[idx] = loc;
        memory_accesses_modes[idx++] = mode;

    }
	
	memory_accesses_modes[idx] = NULL;
	memory_accesses_locations[idx] = NULL;
	
    fclose(fp);



}


void init_page_table(void) 
{
	// Number of Entries = 2^20 
	// MAX_NUM_OF_FRAMES = 1000
	// So MAX 10 bits needed for locating frame
	// 1 present bit
	// Total 11 bits
	// We use int16_t as an entry 
	// [10 bit PFN ,present_bit,valid_bit]

	int NUM_ENTRIES = 1 << (VM_WIDTH - PN_SHIFT); 

	page_table = malloc(NUM_ENTRIES * sizeof(int16_t));

	for(int i = 0; i < NUM_ENTRIES; i++) {
		page_table[i] = (int16_t)0;
	}



}

void init_frames()
{	

	// [20 bit PN,clock set bit,dity bit]
	inverted_page_table = malloc(NUM_FRAMES * sizeof(int));
	for(int i = 0; i < NUM_FRAMES; i++) {
		inverted_page_table[i] = 0;
	}
	free_frame_idx = 0;
	num_free_frames = NUM_FRAMES;
}



void service_mem_access(int addr, char mode)
{	

	int PN = addr >> PN_SHIFT;


	if(replacement_stratergy == 0) {
		reference_times[PN] = (int*)reference_times[PN] + 1;
	}



	int PTE = (int) page_table[PN];

	int PFN = PTE >> 1;

	int present = PTE & 1;

	if(!present) {
		++NUM_MISSES;
		int free_frame = evict(PN); // evict and reutrn the freed frame or return a free frame if already present
		inverted_page_table[free_frame] = 4 * PN + 0 + 0; // clock bit = 1 , dirty bit = 0
		page_table[PN] = (int16_t) (2 * free_frame + 1);
		PFN = free_frame;
	}
	else if(replacement_stratergy == 2) {
		inverted_page_table[PFN] |= 2;
	}


	if(replacement_stratergy == 3) {
		last_used_time[PFN] = global_timer++;
	}


	if(mode == 'W') {
		inverted_page_table[PFN] |= 1; // set dirty bit = 1
	}

}


int evict(int REQUESTED_PN)
{
	// evict and reutrn the freed frame or return a free frame if already pressent

	if(num_free_frames > 0) {
		num_free_frames--;
		return free_frame_idx++;
	}


	int free_frame;

	switch(replacement_stratergy) {
		case 0: {
			// OPT
			free_frame = 0;
			int MAX_RFRT = -1;

			for(int i = 0; i < NUM_FRAMES; i++) {

				int PN_CURR = inverted_page_table[i] >> 2;

				int rfr_t = reference_times[PN_CURR][0];

				if(MAX_RFRT < rfr_t) {
					MAX_RFRT = rfr_t;
					free_frame = i;
				}
				if(rfr_t < 0) {
					free_frame = i; // MAY CHANGE , CURRENTLY RETURNING FRAME WITH LEAST FRAME_Number
					break;
				}
			}
			//printf("Evicted Frame No: %d\n", free_frame);
			break;
		}

		case 1: {
			// FIFO
			free_frame = queue_idx_FIFO++;

			if(queue_idx_FIFO >= NUM_FRAMES)
				queue_idx_FIFO = 0;

			break;
		}

		case 3: {
			// LRU

			int LRU_IDX = 0;
			for(int i = 0; i < NUM_FRAMES; i++) {
				if(last_used_time[i] < last_used_time[LRU_IDX]) {
					LRU_IDX = i;
				}
			}

			free_frame = LRU_IDX;
			break;
		}

		case 2: {
			// clock

			int IVT_PTE = inverted_page_table[clock_idx];

			//int dirty_bit = IVT_PTE & 1;
			int clock_bit = IVT_PTE & 2;

			while(clock_bit) {


				inverted_page_table[clock_idx] ^= 2; // Flip the clock BIT
				
				clock_idx++;

				if(clock_idx >= NUM_FRAMES) {
					clock_idx = 0;
				}
				
				IVT_PTE = inverted_page_table[clock_idx];
				//dity_bit = IVT_PTE & 1;
				clock_bit = IVT_PTE & 2;
			}

			//printf("clock_idx: %d\n", clock_idx);
			free_frame = clock_idx;

			clock_idx++;
			if(clock_idx >= NUM_FRAMES) {
				clock_idx = 0;
			}
			
			break;

		}

		case 4: {
				// RANDOM
		
			free_frame = rand() % NUM_FRAMES;


			//printf("frame NO: %d\n", free_frame);
			break;
		}

	}

	int EVICTED_FRAME_IVT_ENTRY = inverted_page_table[free_frame];
	int dirty_bit = EVICTED_FRAME_IVT_ENTRY & 1;
	int EVICTED_PN = EVICTED_FRAME_IVT_ENTRY >> 2;
	page_table[EVICTED_PN] = 0;

	if(dirty_bit) {
		++NUM_WRITES;
	}
	else{
		++NUM_DROPS;
	}

	if(verbose_MODE) {

		if(!dirty_bit) {
			printf("Page 0x%x was read from disk, page 0x%x was dropped (it was not dirty).\n", REQUESTED_PN, EVICTED_PN);
		}

		else{
			printf("Page 0x%x was read from disk, page 0x%x was written to the disk.\n", REQUESTED_PN, EVICTED_PN);
		}
		
		

	}


	return free_frame;

}




int main(int argc, char* argv[]) 
{

	//freopen("output.txt", "w", stdout);

	init(argc, argv);


	for(int i = 0; i < NUM_REQUESTS; i++) {
		//printf("%d %c\n", memory_accesses_locations[i], memory_accesses_modes[i]);
		service_mem_access(memory_accesses_locations[i], memory_accesses_modes[i]);

	}

	printf("Number of memory accesses: %d\n", NUM_REQUESTS);
	printf("Number of misses: %d\n", NUM_MISSES);
	printf("Number of writes: %d\n", NUM_WRITES);
	printf("Number of drops: %d\n", NUM_DROPS);

	return 0;

}

