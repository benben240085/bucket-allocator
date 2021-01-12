#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "xmalloc.h"
#include "free_list_cell.h"

const size_t PAGE_SIZE = 4096;
static free_list_cell* head;
pthread_mutex_t freeLock;
int locked = 0;


static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void*
searchFreeList(size_t size)
{
   if (head == NULL) {
	return 0;
   } 

   free_list_cell* prev = NULL;
   free_list_cell* newHead = head;
   void* rv = NULL;
   while (newHead != NULL) {
	if (newHead->size >= size) {
	    if (prev == NULL) {
		head = newHead->next;
	    } else if (newHead->next == NULL) {
		prev->next = NULL;
	    } else {
		prev->next = newHead->next;
	    }
	    newHead->next = NULL;
	    rv = newHead;
	    return rv;
	} else {
	    //this is when the current free_list_cell
	    //doesn't have enough space to hold the 
	    //new data so we increment through the linked
	    //list struct
	    prev = newHead;
	    newHead = newHead->next;
	}
   }
   return rv;
}

void*
xmalloc(size_t size)
{
    if (!locked) {
	pthread_mutex_init(&freeLock, 0);
	locked = 1;
    }
    size += sizeof(size_t);
    
    //allocations for less than a page size
    if (size < PAGE_SIZE) {
	pthread_mutex_lock(&freeLock);
	void* rv = NULL;
	int leftOver;

	if (head != NULL) {
	    //search free list for a chunk of free memory
	    rv = searchFreeList(size);
	}

	if (rv != NULL) {
	    size_t* cellSize = rv;
	    leftOver = *cellSize - size;
	} else {
	    //create the free list since rv is NULL
	    //meaning there's not enough free memory
	    rv = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	    leftOver = PAGE_SIZE - size;
	}

	//wasn't sure how this would work
	//found a similar memory allocator from Jonathan Zybert
	//and his used his logic over here and just adapted it to 
	//my code.
	if (leftOver >= sizeof(head)) {
	    //gets the address of rv and increments by the size
	    char* cell = rv;
	    cell += size;
	    free_list_cell* newItem = (free_list_cell*)cell;
	    newItem->size = leftOver;
	    newItem->next = NULL;
	    //find a place to put the new free_list item
	    if (head != NULL) {
		//search until next is NULL
		//and place newItem at that spot
		free_list_cell* newHead = head;
		while (newHead->next != NULL) {
		    newHead = newHead->next;
		}
		newHead->next = newItem;
	    } else {
		//if head is NULL then this newItem
		//will be at the start
		head = newItem;
	    }
	}
	pthread_mutex_unlock(&freeLock);

        size_t* sizeP = rv;
	*sizeP = size;
	//sets the return to be after the size field
	sizeP++;
        return (void*) sizeP;
    } else {
	int numPages = div_up(size, PAGE_SIZE);
        int data = numPages*PAGE_SIZE;
	void* rv = mmap(NULL, data, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

        size_t* sizeP = rv;
	*sizeP = size;
	sizeP++;
        return (void*) sizeP;
    }
}


void 
sort_free_list_cell()
{
    free_list_cell* x, *y, *z;

    x = head;
    head = NULL;

    while (x != NULL) {
	z = x;
	x = x->next;

	if (head != NULL) {
	    if (z > head) {
		y = head;
		while ((y->next != NULL) && (z > y->next)) {
		    y = y->next;
		}
		z->next = y->next;
		y->next = z;
	    } else {
		z->next = head;
		head = z;
	    }
	} else {
	    z->next = NULL;
	    head = z;
	}
    }
}

void
coalesce()
{
    free_list_cell* newHead = head;
    size_t size = newHead->size;
    free_list_cell* next = newHead->next;
    while (next != NULL) {
	char* headAdr = (char*)newHead;
	char* nextAdr = (char*)next;
	//means they are next to each other
	if (headAdr + size == nextAdr) {
	    //updates the current head's size
	    //by combining it with next and 
	    //doesn't have it point to the next
	    //item anymore but the one after
	    newHead->size += next->size;
	    newHead->next = next->next;
	} else {
	    //means they aren't next to each other
	    //so go to next item in list
	    newHead = newHead->next;
	}
	size = newHead->size;
	next = newHead->next;
    }
}

void
xfree(void* item)
{
    void* begBlock = item - sizeof(size_t);
    size_t* beg = begBlock;
    free_list_cell* freeItem = (free_list_cell*)begBlock;
    if (*beg >= 4096) {
	munmap(item, *beg);
    } else {
	//stick on free list
	pthread_mutex_lock(&freeLock);
	free_list_cell* newHead = head;
	free_list_cell* nextItem = newHead->next;

	while (nextItem != NULL) {
	    newHead = nextItem;
	    nextItem = nextItem->next;
	}
	//searches until the end of the list and then places 
	//it at the end
	newHead->next = freeItem;
	newHead->next->size = *beg;
	newHead->next->next = NULL;
	//then sort the free list by address and coalesce if
	//there are any next to each other
	sort_free_list_cell();
	coalesce();
	pthread_mutex_unlock(&freeLock);
    }
}

void*
xrealloc(void* prev, size_t bytes)
{
    if (prev == NULL) {
	return xmalloc(bytes);
    }

    void* begBlock = prev - sizeof(size_t);
    size_t* size = begBlock;
    if (*size > bytes) {
	int leftOver = bytes - *size;
	if (leftOver >= sizeof(free_list_cell)) {
	    pthread_mutex_lock(&freeLock);
	    begBlock += *size;

	    free_list_cell* newItem = (free_list_cell*)begBlock;
	    newItem->size = leftOver;
	    newItem->next = NULL;
	    if (head != NULL) {
		free_list_cell* newHead = head;
		while (newHead->next != NULL) {
		    newHead = newHead->next;
		}
		newHead->next = newItem;
	    } else {
		head = newItem;
	    }
	    pthread_mutex_unlock(&freeLock);
	    *size = bytes;
	    return prev;
	}
    } else if (*size == bytes) {
	return prev;
    } else {
	void* start = prev - sizeof(size_t);
	size_t* size = start;
	void* newMem = xmalloc(bytes);
	memcpy(newMem, prev, *size);
	xfree(prev);
	return newMem;	
    }
    return 0;
}
