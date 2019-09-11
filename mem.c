#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "mem.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct block_header {
        int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} block_header;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */

block_header *start_block = NULL;

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use BEST-FIT PLACEMENT POLICY to find the block closest to the required block size
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic.
 */
void* Alloc_Mem(int size) {
	
	if (size < 1) {
		return NULL;
	}
	
	// size of the block we are attempting to allocate
	int newBlockSize = 0;
	
	// add header to the size requested
	size = size + 4;
	
	// add padding if needed
	if ( (size % 8) == 0) {
		newBlockSize = size;
	} else {
		newBlockSize = size + (8 - (size % 8));
	}


	// use this to check each block in the heap
        block_header *block = start_block;
        int blockSize;
	
	// store the 'best fit' block in here
        block_header *blockMatch = NULL;
        int blockMatchSize = 2147483647;

	// find 'best fit' block in the heap
	while (block->size_status != 1) {
	    blockSize = ((block -> size_status)/8) * 8;
	    
	    // check if block in heap is large enough, and if it's the 'best-fit'
	    if (blockSize >= newBlockSize && blockSize < blockMatchSize) {
		
		// check if block in heap is available or not. If available, update blockMatch
		if ((block->size_status % 8) != 3 && (block->size_status % 8) != 1) {
			blockMatch = block;
                	blockMatchSize = ((blockMatch -> size_status)/8) * 8;
		}
	    }

	    // address arithmetic to check the next block	
	    block = (block_header*)((void*)block + blockSize);
		
	}
	
	// if no block big enough was found, return NULL
	if (blockMatch == NULL) {
		return NULL;
	}


	// check remaining 'free space'
	// if there are at least eight remaining bytes, split block
	if ( (blockMatchSize - newBlockSize) >= 8 ) {
		
		// find address of new header and the footer
		block_header *newHeader = (block_header*)((void*)blockMatch + newBlockSize);
		block_header *footer = (block_header*)((void*)blockMatch + blockMatchSize - 4);
		
		// add value to the new header and update the old value of the footer
		newHeader -> size_status = (blockMatchSize - newBlockSize) + 2;
		footer -> size_status = (blockMatchSize - newBlockSize);
	
	} else {
		// if no splitting occurs, then update p-bit of next block
		block_header *nextBlock = ((void*)blockMatch + newBlockSize);
        	if ( (nextBlock->size_status) != 1) {
			nextBlock->size_status = (nextBlock->size_status) + 2;
        	}


	}

	
	// if the previous block is allocated
	if ( ((blockMatch -> size_status) % 8) == 2 ) {
		(blockMatch -> size_status) = newBlockSize + 3;
	} else {
		// if previous block is not allocated
		(blockMatch -> size_status) = newBlockSize + 1;
	}


	
	// returns pointer to payload
	return ((void*)blockMatch + 4);

}

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int Free_Mem(void *ptr) {         
	if (ptr == NULL)
		return -1;
	if ( ((int)ptr) % 8 != 0)
		return -1;
	
	
	block_header *block = start_block;
	int blockSize;
	block_header *footer = NULL;
	block_header *nextBlock = NULL;
	int nextBlockSize;

	while ( (block->size_status) != 1) {
		
		blockSize = ((block->size_status) / 8) * 8;

		// matching addresses found
		if ( ((void*)block + 4) == ptr) {

			// return -1 if the block is already freed
			if ( (block->size_status)%8==0 || (block->size_status)%8==2 )
				return -1;
			

			nextBlock = (block_header*)((void*)block + blockSize);
			// the case when the previous block is allocated
			if ( (block->size_status) % 8 == 3) {
			

				// if next block is allocated
				if ( (nextBlock->size_status)==1 || ((nextBlock->size_status)-2) % 8  == 1 ) {
					
					// update header and add footer of current block
					block->size_status = (block->size_status) - 1;
					footer = ((void*)block + blockSize - 4);
					footer->size_status = blockSize;

					// update header of next block
					nextBlock->size_status = (nextBlock->size_status) - 2;
					return 0;

				} else {
					// if next block is free
					nextBlockSize = ((nextBlock->size_status) / 8) * 8;
					
					// update header of current block and footer of next block
					block->size_status = (block->size_status) + nextBlockSize - 1;
					footer = (block_header*)((void*)nextBlock + nextBlockSize - 4);
					footer->size_status = blockSize + nextBlockSize;
					return 0;
				}

				
			
			} else {
				// the case where previous block is free
				
				// the header and footer of the previous block
				block_header *prevFooter = (block_header*)((void*)block - 4);
				int prevBlockSize = prevFooter->size_status;
				block_header *prevHeader = (block_header*)((void*)block - prevBlockSize);


				// if next block is allocated, combine current and previous block
				if ( (nextBlock->size_status)==1 || ((nextBlock->size_status)-2) % 8  == 1 ) {
					
					// update header of previous block and footer of current block
					prevHeader->size_status = prevHeader->size_status + blockSize;
					footer = (block_header*)((void*)block + blockSize - 4);
					footer->size_status = prevBlockSize + blockSize;
					
					// remove p-bit from next block
					nextBlock->size_status = (nextBlock->size_status) - 2;
					
					return 0;

				} else {
					// if next block is free, 
					nextBlockSize = ((nextBlock->size_status) / 8) * 8;
					
					// update header of previous block and footer of next block
					prevHeader->size_status = prevHeader->size_status + blockSize + nextBlockSize;
					footer = (block_header*)((void*)nextBlock + nextBlockSize - 4);
					footer->size_status = prevBlockSize + blockSize + nextBlockSize;
					return 0;
				}

			}



		}
		// check the next block for the address
		block = (block_header*)((void*)block + blockSize);

	}
	

	return -1;
}

/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int Init_Mem(int sizeOfRegion) {         
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    block_header* end_mark;
    static int allocated_once = 0;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: Init_Mem has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
                    fd, 0);
    if (MAP_FAILED == space_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // To begin with there is only one big free block
    // initialize heap so that start block meets 
    // double word alignement requirement
    start_block = (block_header*) space_ptr + 1;
    end_mark = (block_header*)((void*)start_block + alloc_size);
  
    // Setting up the header
    start_block->size_status = alloc_size;

    // Marking the previous block as used
    start_block->size_status += 2;

    // Setting up the end mark and marking it as used
    end_mark->size_status = 1;

    // Setting up the footer
    block_header *footer = (block_header*) ((char*)start_block + alloc_size - 4);
    footer->size_status = alloc_size;
  
    return 0;
}         
                 
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void Dump_Mem() {         
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    block_header *current = start_block;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (block_header*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}         
