/* Most logic taken from Bucket lectures by Nat Tuck in lecture series 10
 * as well as our hw08 structures that we discussed and turned in previously
 * We also referenced Jonathan Zybert's solution for this challenge */

#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "free_list_cell.h"

const size_t PAGE_SIZE = 4096;
//__thread free_list_cell* bins[7] = {32, 64, 128, 256, 512, 1028, 2048};//
__thread free_list_cell *bins[7] = {0, 0, 0, 0, 0, 0, 0};

static size_t
div_up(size_t xx, size_t yy)
{
	// This is useful to calculate # of pages
	// for large allocations.
	size_t zz = xx / yy;

	if (zz * yy == xx)
	{
		return zz;
	}
	else
	{
		return zz + 1;
	}
}

// returns the size of the bin needed for the allocation
int binSize(size_t size)
{
	if (size <= 32)
	{
		return 32;
	}

	for (int i = 32; i <= 2048; i *= 2)
	{
		if (size <= i)
		{
			return i;
		}
	}
	return 0;
}

// returns the index of the bin where this size should go
int sizeToIndex(size_t bytes)
{
	int count = 0;

	if (bytes > 2048)
	{
		return 6;
	}

	for (int i = 32; i <= 2048; i *= 2)
	{
		if (i >= bytes)
		{
			return count;
		}
		count++;
	}
	return 0;
}

// returns the maximum memory that can be fully taken from a size of memory
// i.e for 1920 we can only take 1024, then 512, etc for putting extra
// free memory back into bins
int maxBinLeftOver(int size)
{
	int count = 0;

	if (size > 2048)
	{
		return 6;
	}

	for (int i = 32; i <= 2048; i *= 2)
	{
		if (i == size)
		{
			return count;
		}
		else if (i < size && (i * 2) > size)
		{
			return count;
		}
		count++;
	}
	return 0;
}

// returns the proper size of the bins at the given index
int indexToSize(int index)
{
	int size = 32;
	while (index > 0)
	{
		index--;
		size *= 2;
	}
	return size;
}

// Used the logic from Jonathan Zybert
// ch02 on github, and adapted it to fit our structure
// that was largely taken from both mine and Ben's
// previous hw08
void coalesce(int index)
{
	free_list_cell *head = bins[index];
	int binSize = indexToSize(index);
	int nextBinSize = binSize * 2;
	int total = binSize + nextBinSize;

	while (head != NULL && head->next != NULL)
	{
		if (((int64_t)head + head->size == (int64_t)head->next) 
                && (head->size + head->next->size != total))
		{
			head->size += head->next->size;
			head->next = head->next->next;
		}

		head = head->next;
	}

	free_list_cell *prev = NULL;
	free_list_cell *itemToAdd = NULL;
	head = bins[index];

	while (head != NULL)
	{
		//means the current free_list_cell should be moved to another bin
		if (head->size > binSize)
		{
			//logic as to how we should ignore the bigger cell in the
			//current bucket/bin
			if (prev == NULL)
			{
				bins[index] = head->next;
			}
			else if (head->next == NULL && (prev != 0 || prev != NULL))
			{
				prev->next = NULL;
			}
			else
			{
				prev->next = head->next;
			}

			//assign the free_list_cell to itemToAdd so we
			//can move that item into a different bucket
			itemToAdd = head;
			itemToAdd->next = NULL;
			head = head->next;

			//find the bucket index for the item's size
			int nextBin = sizeToIndex(itemToAdd->size);
			free_list_cell *binToAdd = bins[nextBin];

            // taken from our addBackToBin logic, 
            // pretty much same stuff


		    if (binToAdd != NULL && binToAdd > itemToAdd)
		    {
			    while (binToAdd->next != NULL && binToAdd->next < itemToAdd)
			    {
			    	binToAdd = binToAdd->next;
			    }

		    	binToAdd->next = itemToAdd;
		    }
            else 
		    { // if head is empty or if this is the earliest item in list
			    itemToAdd->next = bins[nextBin];
			    bins[nextBin] = itemToAdd;
		    }
		}
		else
		{
			//traverse to next item in the list
			prev = head;
			head = head->next;
		}
	}
}

// adds extra free memory back to bins by
// subtracting the max amount of memory
// that can be fully allocated into a bin
// and adding that to the proper bin location
void addBackToBin(free_list_cell *cell)
{
	// get the size of the cell we must add to respective bins
	size_t size = cell->size;
	cell->next = NULL;
	int newSize = (int)size;
	char *cellAdr = (char *)cell;

	// while we have memory left to store
	while (newSize > 0)
	{
		int index = maxBinLeftOver(newSize);
		int binSize = indexToSize(index);

		// get the index and max bin size for this memory
		newSize -= binSize;
		free_list_cell *newItem = (free_list_cell *)cellAdr;
		// move to next item in the bin we need to allocate
		// and rework the newItem to represent it's new allocation
		cellAdr += binSize;
		newItem->size = binSize;
		newItem->next = NULL;

		// go to the head of the cell_list we need to insert new memory into
		free_list_cell *head = bins[index];

		// place the item in the proper location
		// in the list by adding by address
		// aka sorting it by address
		// this idea is taken from Nat Tuck
		// series 7 lectures where he mentioned sorting by address
		// to help make coalescing easier

		if (head != NULL && (head <= newItem))
		{
			while (head->next != NULL && (head->next < newItem))
			{
				head = head->next;
			}

			head->next = newItem;
		}
        else 
		{ // if head is empty or if this is the earliest item in list
			newItem->next = bins[index];
			bins[index] = newItem;
		}

		// if binsize is less than 2048, coalesce on the bins
		// we don't need to do this for 2048 bins since there
		// should be a max of 2 of those bins
		if (binSize < 2048)
		{
			coalesce(index);
		}
	}
}

// search the bins for a place to put the given bytes
// idea is to search through bins until we find enough space for our bytes, 
// and if we do, add extra mem back to other smaller bins
// took some logic from Jonathan Zybert, but largely written on our own, 
// just happens to be similar because of similar data structures in hw08
void *
searchBinIndex(size_t bytes, int index)
{
	//traverse through list
	free_list_cell *head = bins[index];

    // if we have free mem in proper space allotment
	if (head != NULL)
	{
		bins[index] = head->next;
		head->next = NULL;
		return (void *)head;
	}
	else    // if we have to search through bigger free mem
	{
		void *ret = NULL;
		for (int i = index + 1; i < 7; ++i)
		{
            head = bins[i];
			if (head != NULL)
			{
				free_list_cell * largeBin = head;
				bins[i] = largeBin->next;
				size_t oldSize = largeBin->size;

				free_list_cell * new = largeBin;
				new->size = bytes;
				new->next = NULL;
				size_t loSize = oldSize - bytes;

				char *leftAdr = (char *) new;
				leftAdr += bytes;
				free_list_cell *leftOver = (free_list_cell *) leftAdr;
				leftOver->size = loSize;
				leftOver->next = NULL;

				addBackToBin(leftOver);
				ret = (void *) new;
				break;
			}
		}

		return ret;
	}
}

// taken mainly from hw08, just added bucket logic
void *
xmalloc(size_t bytes)
{
	// add sizeof(size_t) to given bytes to make sure
	// we have space for the size field
	bytes += sizeof(size_t);

	if (bytes <= 2048)
	{
		bytes = binSize(bytes);
		int index = sizeToIndex(bytes);
		void *newItem = searchBinIndex(bytes, index);

		if (newItem != NULL)
		{
			//insert bytes into item
			char *newAdr = (char *)newItem;
			newAdr += sizeof(size_t);
			return (void *)newAdr;
		}
		else
		{
			//mmap cuz its NULL
			newItem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			size_t remaining = PAGE_SIZE - bytes;

			char *newAdr = (char *) newItem;
			newAdr += bytes;
			void *lo = (void *) newAdr;

			free_list_cell *leftOver = (free_list_cell*)lo;
			leftOver->size = remaining;
			leftOver->next = NULL;

			free_list_cell *current = (free_list_cell*)newItem;
			current->size = bytes;
			current->next = NULL;

			addBackToBin(leftOver);

			char *retAdr = (char *)current;
			retAdr += sizeof(size_t);

			return (void *)retAdr;
		}
	} 
	else 
	{
		//big memory allocation
		if (bytes > 2048 && bytes < PAGE_SIZE * 2)
		{
			bytes = PAGE_SIZE * 2;
		}
		int numPages = div_up(bytes, PAGE_SIZE);
		int data = numPages * PAGE_SIZE;
		void *rv = mmap(NULL, data, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

		size_t *sizeP = rv;
		*sizeP = bytes;
		sizeP++;
		return (void *)sizeP;
	}
}

// xfree method
void 
xfree(void *ptr)
{
	void *begBlock = ptr - sizeof(size_t);
	free_list_cell *newItem = (free_list_cell *)begBlock;

    if (newItem->size >= PAGE_SIZE)
    {
        munmap(newItem, newItem->size);
    }
    else
    {
        newItem->next = NULL;
        addBackToBin(newItem);
    }
}

// reallocation method 
void *
xrealloc(void *prev, size_t bytes)
{
	char *prevAdr = (char *)prev - sizeof(size_t);
	size_t prevSize = *((size_t *) prevAdr);

	void *current = xmalloc(bytes);
	char *currAdr = (char *) current - sizeof(size_t);

	// get size of currently stored data
	size_t currSize = *((size_t *) currAdr);

	// if allocating previous stuff to a smaller size
	if (prevSize > currSize)
	{
		size_t allocSize = prevSize - sizeof(size_t) - currSize;
		memcpy(current, prev, allocSize);
	}
	else
	{
		size_t allocSize = prevSize - sizeof(size_t);
		memcpy(current, prev, allocSize);
	}

	// free previously stored stuff
	xfree(prev);
	// return pointer to new stuff
	return current;
}
