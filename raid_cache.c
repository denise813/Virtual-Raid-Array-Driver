////////////////////////////////////////////////////////////////////////////////
///
//
//  File           : raid_cache.c
//  Description    : This is the implementation of the cache for the TAGLINE
//                   driver.
//
//  Author         : Hao Wang
//  Last Modified  : Fri 11 Dec 2015 10:39:27 PM EST 
//

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// Project includes
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <raid_cache.h>

//#include <tagline_driver.c>i


//////////////////////////////
// 
// Function 	:find_min
//
//
//get the loaction of the least recent used cache line

int current_time;		//current time
int cache_blks;			//total  cache blks
int *cache_time;		//cache block time, using LRU policy
int *On_cache;			//index of cache to map data on disks

extern int **On_disk;		//disk index from driver.c
char *cache;			//cache

//
// TAGLINE Cache interface

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_raid_cache
// Description  : Initialize the cache and note maximum blocks
//
// Inputs       : max_items - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int init_raid_cache(uint32_t max_items) {

	//initialize cache
	cache_blks = max_items;					
	cache = malloc (cache_blks * sizeof(char));	//dyniacally allocate cache
	cache_time = calloc (cache_blks, sizeof(int));		//allocate global varables
	On_cache = calloc (cache_blks, sizeof(int));
	current_time = 1;					//set current time to 1, when first access occures, time is 1

	
	// Return successfully
	logMessage(LOG_INFO_LEVEL, "Cache INIT SUCCESS!!");
	return(0);

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_raid_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_raid_cache(void) {
	
	//free global variables, set pointers to null
	free(cache);
	free(cache_time);
	cache = NULL;
	cache_time = NULL;

	logMessage(LOG_INFO_LEVEL, "Cache CLOSE SUCCESS!!");
	
	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_raid_cache
// Description  : Put an object into the block cache
//
// Inputs       : dsk - this is the disk number of the block to cache
//                blk - this is the block number of the block to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_raid_cache(RAIDDiskID dsk, RAIDBlockID blk, void *buf)  {
	
	//local variables declariation
	int i, min;
	min = 0;

	//find the loacation of least used cache block (if 0, it is empty)
	for (i=0; i<cache_blks; i++) {
		if (cache_time[i] < min)
			min = i;
	}


	//DATA is in cache, update data, update time
	for (i=0; i<cache_blks; i++) {
		//if index in cache matches index on disk and they are not 0s, update data 
		if (On_cache[i] == On_disk[dsk][blk] && On_disk[dsk][blk] != 0) {
			On_cache[i] = On_disk[dsk][blk];
			cache[i] = *((char*)buf);
			cache_time[i] = current_time;
			current_time++;
			logMessage(LOG_INFO_LEVEL, "IN CACHE WhilE WRiTE, update data, update time");
			break;
		}
	}


	//Not in cache, have space
	if (On_cache[min] == 0) {
		cache[min] = *((char*)buf);
		On_cache[min] = On_disk[dsk][blk];

		cache_time[min] = current_time;
		current_time++;
		logMessage(LOG_INFO_LEVEL, "Not in cache, have space: INSERT IT INTO CACHE, UPDATE TIME!!!");
	}
//	}
	
	//Not in cache, no space
	else {
		cache[min] = *((char*)buf);
		On_cache[min] = On_disk[dsk][blk];
		cache_time[min] = current_time;
		current_time++;
		logMessage(LOG_INFO_LEVEL, "Not in cache, no space: INSERT IT INTO CACHE, UPDATE TIME!!!");
		
	}


	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_raid_cache
// Description  : Get an object from the cache (and return it)
//
// Inputs       : dsk - this is the disk number of the block to find
//                blk - this is the block number of the block to find
// Outputs      : pointer to cached object or NULL if not found

void * get_raid_cache(RAIDDiskID dsk, RAIDBlockID blk) {

	//local variables
	char *buf;
	int i;

	//search cache, return pointer if found
	for (i=0; i<cache_blks; i++) { 
		if ( (On_cache[i] != 0) && (On_cache[i] == On_disk[dsk][blk])) {
			cache_time[i] = current_time;
			current_time++;
			logMessage(LOG_INFO_LEVEL, "FIND IN CACHE, UPDATE TIME!!!");
	//		logMessage(LOG_INFO_LEVEL, "CACHE VALUE %d", cache[i]);
			memcpy(&buf, &cache[i], sizeof(char));
			return buf;
		}
	}
//	logMessage(LOG_INFO_LEVEL, "CACHE VALUE %d", On_cache[i]);
//	logMessage(LOG_INFO_LEVEL, "DISK VALUE %d", On_disk[dsk][blk]);
	logMessage(LOG_INFO_LEVEL, "NOT IN ThE CACHE!!!");
	return(NULL);
}

