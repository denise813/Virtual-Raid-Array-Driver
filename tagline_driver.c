//////////////////////////////////////////////////////////////////////////////
//
//  File           : tagline_driver.c
//  Description    : This is the implementation of the driver interface
//                   between the OS and the low-level hardware.
//
//  Author         : Hao Wang
//  Created        : Fri 11 Dec 2015 10:39:45 PM EST 

// Include Files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>
 
// Project Includes
#include "raid_bus.h"
#include "tagline_driver.h"
#include "raid_cache.h"
//Global variables/pointers:

int **On_tag;		//for dynamically allocate 2D array (On_tag[maxlines][MAX_TAGLINE_BLOCKS]) that is used to track index of every block of every tagline
int **On_disk;		//for dynamically allocate 2D array (On_disk[RAID_DISKS][RAID_DISKBLOCKS]) that is used to track index of every block of every disk
int **index_tag;	//for dynamically allocate 2D array (On_tag[maxlines][MAX_TAGLINE_BLOCKS]) that index every block of every tagline from 1 to maxlines*MAX_TAGLINE_BLOCkS
int MAXLINES;		//used to store maxlines as a global variable

int disk_w;		//used for raid write, determin which disk to write primary data, and the backup data is stored the next disk (if primary is 8, backup goes to 0)

//variables for calculate cache part
int cache_hit;
int cache_access;
int cache_insert;




//declaration for pack_code so that all functions can use it even if it is defined after them
RAIDOpCode pack_code (uint64_t req_type, uint64_t num_blks, uint64_t disk_num, uint64_t unused, uint64_t status, uint64_t blk_id); 

/////////////////////////////////////////////////////////////////////////////////
// Function     : tag_info
// Description  : Get location (disk No. or block No.) of a given tagline block on disk
//
// Inputs       : tag - The tagline that needed to get info for
// 		  blk_num - block number of tag that needed to get info for
// 		  type - define which type of info the function returns, 'd' for disk No., 'b' for block No. 
// Outputs      : disk number or block number of a given tag_line


int tag_info (TagLineNumber tag, uint8_t blk_num,  char type) {

	//declraration of local variables
	int i;		//counter i, j		
	int j;
	int index;	//index of given tagline block
	index = On_tag[tag][blk_num];
	
	//loop for find the tagline disk and block using index matching, get the ouput
	for (i=0; i<RAID_DISKS; i++) {
			for (j=0; j<RAID_DISKBLOCKS; j++){
				if (On_disk[i][j] == index){
					if (type == 'd')
						return i;
					if (type == 'b')
						return j;
				}
			}
	}
	return (0);
}


/////////////////////////////////////////////////////////////////////////////////
// Function     : bk_info
// Description  : Get location (disk No. or block No.) of a given tagline block BACKUP on disk
//
// Inputs       : tag - The tagline that needed to get info for
// 		  blk_num - block number of tag that needed to get info for
// 		  type - define which type of info the function returns, 'd' for disk No., 'b' for block No. 
// Outputs      : BACKUP disk number or BACKUP block number of a given tag_line


int bk_info (TagLineNumber tag, uint8_t blk_num, char type) {

	//declaration of local variables
	int i;		//counter for loop
	int j;
	int index;	//index of given tagline block
	int counter;	//counter for make sure we find the backup not the orignial one
	index = On_tag[tag][blk_num];	
	counter = 0;
	
	//loop for find the backup disk and block using index matching, counter = 2 means it is the 'BACKUP', get the output
	for (i=0; i<RAID_DISKS; i++) {
		for (j=0; j< RAID_DISKBLOCKS; j++) {
			if (On_disk[i][j] == index) {
				counter = counter + 1;
				if (type == 'd' && counter == 2)
					return i;
				if (type == 'b' && counter == 2)
					return j;
			}
		}
	}
	return (0);
}


//////////////////////////////////////////////////////////////////////////////////
// Function     : check_free
// Description  : Check if a disk has non-writen block
//
// Inputs       : disk - the disk number needed to be checked
// Outputs      : first block number that is avavliable, or -1 for disk is full

int check_free (int disk)
{
	int i;
	for (i=0; i<RAID_DISKBLOCKS; i++) {
		if (On_disk[disk][i] == 0) {
			return i;
		}
	}

	return (-1);
}


/////////////////////////////////////////////////////////////////////////////////
// Function     : blk_info
// Description  : Get the tagline No. or block No. of a given disk block
//
// Inputs       : disk - the disk No. needed to get info for
// 		  blk - the block No. needed to get info for
// 		  type - define which type of info the function returns, 't' for tagline No., 'b' for block No. 
// Outputs      : 0 if successful, -1 if failure

int blk_info(int disk, int blk, char type)
{
	//declaration of local variables
	int i;		//counter for loop
	int j;
	
	//loop used for find tagline No. and block No. using index matching
	for (i=0; i<MAXLINES; i++)
		for (j=0; j<MAX_TAGLINE_BLOCK_NUMBER; j++) {
			if (On_tag[i][j] == On_disk[disk][blk]) {
				if (type == 't')
					return i;
				if (type == 'b')
					return j;
			}
		}

	return (0);
}

/////////////////////////////////////////////////////////////////////////////////
// Function     : raid_disk_singal
// Description  : check for disk failrue and repair accordingly
//
// Inputs       : None
// Outputs      : 0 for successful, -1 for failure


int raid_disk_signal(void)
{	
	//delcaration for local variables
	int status;				//status from feedback of check_disk, 
	int disk, disk_bk, tag, bnum, blk;	//variable decalriation (bk stands for backup)
	int i, j;				//counters for loop
	
	char buf[RAID_BLOCK_SIZE];		//buffer for store one block data

	RAIDOpCode check_disk;			//opcode for check ith disk
	RAIDOpCode feedback;			//opcode for feedback from check_disk function
	RAIDOpCode action;			//opcode action for action according to status	

	for (i=0; i<RAID_DISKS; i++) {
		//check each disk status
		check_disk = pack_code (RAID_STATUS,0,i,0,0,0);
		feedback = client_raid_bus_request (check_disk, NULL);

		//unpack opcode to find the status
		status = (feedback << 32) >> 32;
		
		//format disk if status shows disk failure caused by unformated disk
		if (status == RAID_DISK_UNINITIALIZED) {
			action = pack_code (RAID_FORMAT,0,i,0,0,0);
			client_raid_bus_request (action, NULL);
		}
		
		//do nothing if disk is ready
		if (status == RAID_DISK_READY)
			i = i; 		//do nothing, continue loop;
		
		//format disk and recover disk using backups
		if (status == RAID_DISK_FAILED) {

			//step1: format failed disk
			action = pack_code (RAID_FORMAT,0,i,0,0,0);
			client_raid_bus_request (action, NULL);

			//step2: recover the disk, one blk each time
			for (j=0; j<RAID_DISKBLOCKS; j++) {
				//check the On_disk index to see if the block is written
				//if it is written, get the tagline No. and blk No., find the backup loaction of it, read it, and write it to the failed block
				if (On_disk[i][j] != 0) {		
					tag = blk_info (i, j, 't');	
					bnum = blk_info (i, j, 'b');

					disk = tag_info (tag, bnum, 'd');
					disk_bk = bk_info (tag, bnum, 'd');

					//if the fail disk is where the original block copy stored, use the backup to restore 
					if (i == disk) {
						blk = bk_info (tag, bnum, 'b');
						action = pack_code (RAID_READ,1,disk_bk,0,0,blk);
						client_raid_bus_request (action, buf);
						action = pack_code (RAID_WRITE,1,i,0,0,j);
						client_raid_bus_request (action, buf);
					}

					//if the fail disk is where the backup block copy stored, use the orignial copy to restore
					if (i == disk_bk) {
						blk = tag_info (tag, bnum, 'b');
						action = pack_code (RAID_READ,1,disk,0,0,blk);
						client_raid_bus_request (action, buf);
						action = pack_code (RAID_WRITE,1,i,0,0,j);
						client_raid_bus_request (action, buf);
					}

				}
				
				//if the block is empty, it is the same as before after formate, do nothing
				//My strategy is to store the tagline block on random disk, but one by one on the disk, 
				//so if the blk is empty, the rest of the disk blocks are all empty
				else {
				//	return (0);
			// Return successfully
					logMessage(LOG_INFO_LEVEL, "TAGLINE : disk[%d] recovery complelted",i);
					return(0);
				}
			}
		}
	}
	return (0);
}


///////////////////////////////////////////////////////////////////////////////
//Function	:pack_code
//Description	:pack code for the raid_bus
//
//Input		:int request type, munber of blocks, deisk number, unused, status(0), block ID
//Output:	:RAIDOpCode opcode

RAIDOpCode pack_code (uint64_t req_type, uint64_t num_blks, uint64_t disk_num, uint64_t unused, uint64_t status, uint64_t blk_id) {

	//Setup local variables
	RAIDOpCode Opcode = 0x0;	
	uint64_t a, b, c, d, e, f;

	//pack code, since the format is req_type....block_ID, pack the first 8 bit to the leftest position
	a = (req_type&0xff) << 56;	//shift 64-8 to the left to 	
	b = (num_blks&0xff) << 48;	//shift 56-8
	c = (disk_num&0xff) << 40;	//shift 48-8
	d = (unused&0xff) << 33;	//shift 40-7
	e = (status&0xff) << 32;	//shift 33-1
	f = blk_id&0xffffffff;		//pack the final 32bit
	Opcode = a|b|c|d|e|f;		//pack the code together by using or

	return (Opcode);		//return the Opcode
}


/////////////////////////////////////////////////////////////////////////////////
// Function     : tagline_driver_init
// Description  : Initialize the driver with a number of maximum lines to process
//
// Inputs       : maxlines - the maximum number of tag lines in the system
// Outputs      : 0 if successful, -1 if failure

int tagline_driver_init(uint32_t maxlines) {

	//declaration local variables
	int i, j;	
	int count;
	int num_of_tracks;
	RAIDOpCode init;
	RAIDOpCode format;

	num_of_tracks = RAID_DISKBLOCKS/RAID_TRACK_BLOCKS;

	//dynamical allocate 2D arrays, allocate array of pointers and then allocate each pointer an array
	On_tag = malloc (maxlines * sizeof(*On_tag));
	if (On_tag == NULL) {
		printf ("ERROR,	OUT OF MEMORY!");
		exit(0);
	}
	for (i=0; i<maxlines; i++) {
		On_tag[i] = calloc (MAX_TAGLINE_BLOCK_NUMBER, sizeof(*On_tag[i]));
		if (On_tag[i] == NULL) {
			printf ("ERROR,	OUT OF MEMORY!");
			exit(0);
		}
	}
	index_tag = malloc (maxlines * sizeof(*index_tag));
	if (index_tag == NULL) {
		printf ("ERROR,	OUT OF MEMORY!");
		exit(0);
	}
	for (i=0; i<maxlines; i++) {
		index_tag[i] = calloc (MAX_TAGLINE_BLOCK_NUMBER, sizeof(*index_tag[i]));
		if (On_tag[i] == NULL) {
			printf ("ERROR,	OUT OF MEMORY!");
			exit(0);
		}
	}
	On_disk = malloc (RAID_DISKS * sizeof(*On_disk));
	if (On_disk == NULL) {
		printf ("ERROR,	OUT OF MEMORY!");
		exit(0);
	}
	for (i=0; i<RAID_DISKS; i++) {
		On_disk[i] = calloc (RAID_DISKBLOCKS, sizeof(*On_disk[i]));
			if (On_disk[i] == NULL) {
			printf ("ERROR,	OUT OF MEMORY!");
			exit(0);
		}
	}


	//set maxlines to be global variable
	MAXLINES = maxlines;
	

	//Initialize the interface
	init = pack_code (RAID_INIT,num_of_tracks,RAID_DISKS,0,0,0);
	client_raid_bus_request(init, NULL);

	//formate all disks
	for(i=0; i < RAID_DISKS; i++) {
		format = pack_code (RAID_FORMAT,0,i,0,0,0);
		client_raid_bus_request(format, NULL);
	}
	
	//index the index_tag array from 1 to maxline*MAX_TAGLINE_BLOCK_NUMBER so that each tagline_block has a unique index
   	count = 1;
	for (i=0; i<maxlines; i++) {
		for (j=0; j<MAX_TAGLINE_BLOCK_NUMBER; j++) {
			index_tag[i][j] = count;
			count = count + 1;
		}
	}

	//init cache
	init_raid_cache (TAGLINE_CACHE_SIZE);
	cache_hit = 0;
	cache_access = 0;
	cache_insert = 0;

	//initialize the disk NO. for write primary data
	disk_w = 0;
	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE: initialized storage (maxline=%u)", maxlines);
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_read
// Description  : Read a number of blocks from the tagline driver
//
// Inputs       : tag - the number of the tagline to read from
//                bnum - the starting block to read from
//                blks - the number of blocks to read
//                buf - memory block to read the blocks into
// Outputs      : 0 if successful, -1 if failure

int tagline_read(TagLineNumber tag, TagLineBlockNumber bnum, uint8_t blks, char *buf) {
	
	//declaration for local variables
	int i;			
	int disk, blk;
	RAIDOpCode read;
	char *cache_buf;	//a pointer for cache
//	char cache_buf[1024];	
	
//	cache_buf = malloc (1024* sizeof(char));
	//read one block each time using loop
	for (i=0; i<blks; i++) {
		disk = tag_info(tag, bnum, 'd');
		blk = tag_info(tag, bnum, 'b');
		
		cache_buf = get_raid_cache(disk, blk);	//call get_raid_cache, to see if the data is in cache, save the return pointer to cache_buf

		//search cache first
		if (cache_buf == NULL) {
		//not in the cache, read from disk and put it in the cache
			read = pack_code (RAID_READ,1,disk,0,0,blk);
			client_raid_bus_request(read, &cache_buf);		//move pointer 1024 bytes every time to store the next data read

			put_raid_cache(disk, blk, &cache_buf);			//put data into cache
			memset(&buf[i*RAID_BLOCK_SIZE], &cache_buf[0], RAID_BLOCK_SIZE);		//get data from cache and give it to buf to read to tagline_sim
			cache_insert++;						
			cache_access++;
		}

		//IN CACHE, update time only
		else	{
			memset(&buf[i*RAID_BLOCK_SIZE], &cache_buf[0], RAID_BLOCK_SIZE); 	//get data from cache instead of disk, HIT!
			cache_hit++;
			cache_access++;
		}
		bnum = bnum + 1;
	}

	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE : read %u blocks from tagline %u, starting block %u.",
			blks, tag, bnum);
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_write
// Description  : Write a number of blocks from the tagline driver
//
// Inputs       : tag - the number of the tagline to write from
//                bnum - the starting block to write from
//                blks - the number of blocks to write
//                buf - the place to write the blocks into
// Outputs      : 0 if successful, -1 if failure

int tagline_write(TagLineNumber tag, TagLineBlockNumber bnum, uint8_t blks, char *buf) {

	//declaration for local variables
	int i;
	int disk, blk, disk_bk, blk_bk;		//'bk' for backup
	RAIDOpCode write;
	RAIDOpCode write_bk;	
	int freeblk;				//used to store the loacation of free block of a given disk
	//setup disk number which used in write new (NOT OVERWRITE)
	disk = disk_w;		
	//if disk is 8, then the backup is at disk0, else backup is at the next disk of disk
	if (disk == 8)
		disk_bk = 0;
	else
		disk_bk = disk + 1;

	//loop for tagline_write, one blk each time
	for (i=0; i<blks; i++) {
			
		//check if it is to write data to a new block or overwrite, if it is new block then:
		if (On_tag[tag][bnum] == 0) {
			
			//all set, ready to write

			freeblk= check_free(disk);	
			write = pack_code (RAID_WRITE,1,disk,0,0,check_free(disk));
			write_bk = pack_code (RAID_WRITE,1,disk_bk,0,0,check_free(disk_bk));
			client_raid_bus_request(write, buf+i*RAID_BLOCK_SIZE);		//move the pointer 1024 bytes each time
			client_raid_bus_request(write_bk, buf+i*RAID_BLOCK_SIZE);

			//update the index of On_tag, On_disk from 0 to index -- mark this blk is written
			
			On_tag[tag][bnum] = index_tag[tag][bnum];
			On_disk[disk][freeblk] = index_tag[tag][bnum];
			On_disk[disk_bk][check_free(disk_bk)] = index_tag[tag][bnum];
//logMessage(LOG_INFO_LEVEL, "DISK WHLE INSERT: %d", index_tag[tag][bnum]);
//
			//new blk is not in the cache (even if the data is same, 'address' is different tho), so put a copy to cache, write to the disk
			//I did not put backup into cache, i think it is not neccessary

			put_raid_cache(disk, freeblk, buf+i*RAID_BLOCK_SIZE);
//			put_raid_cache(disk_bk, blk_bk, buf+i*RAID_BLOCK_SIZE);
			cache_access++;
//			cache_access++;
			cache_insert++;
//			cache_insert++;

		}
		
		//if the block is writen
		//may in the cache, search cache and update time
		else {

			//overwrite old blk
			disk = tag_info(tag, bnum, 'd');
			blk = tag_info(tag, bnum, 'b');

			disk_bk = bk_info(tag, bnum, 'd');
			blk_bk = bk_info(tag, bnum, 'b');

			write = pack_code (RAID_WRITE,1,disk,0,0,blk);
			write_bk = pack_code (RAID_WRITE,1,disk_bk,0,0,blk_bk);

			client_raid_bus_request(write, buf+i*RAID_BLOCK_SIZE);
			client_raid_bus_request(write_bk, buf+i*RAID_BLOCK_SIZE);

			put_raid_cache(disk, blk, buf+i*RAID_BLOCK_SIZE);
//			put_raid_cache(disk_bk, blk_bk, buf+i*RAID_BLOCK_SIZE);
			cache_access++;
//			cache_access++;
			cache_insert++;
//			cache_insert++;
			logMessage(LOG_INFO_LEVEL, "OVERWRITEEEEEEEEEEEEEE???!!!");

		}
		//update disk number
		if (disk_w == 8) {
			disk_w = 0;
		}
		else {
			disk_w++;
		}
		bnum = bnum + 1;
		logMessage(LOG_INFO_LEVEL, "WRITE DATA TO DISK: [%d]", (int)buf[i*1024]);
	}

	// Return successfully
	logMessage(LOG_INFO_LEVEL, "TAGLINE : wrote %u blocks to tagline %u, starting block %u.",
			blks, tag, bnum);
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : tagline_close
// Description  : Close the tagline interface
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int tagline_close(void) {

	//declaration of local variables
	int i;
	RAIDOpCode close;

	//close tagline interface
	close = pack_code (RAID_CLOSE,0,0,0,0,0);
	client_raid_bus_request(close, NULL);

	//free dynamically allocate memory regions
	//free array of pointer first using loop, set them to NULL also, then free 2D array pointers
	for (i=0; i<MAXLINES; i++) {
		free (On_tag[i]);
		On_tag[i] = NULL;
	}
	free (On_tag);
	On_tag = NULL;

	for (i=0; i<MAXLINES; i++) {
		free (index_tag[i]);
		index_tag[i] = NULL;
	}
	free (index_tag);
	index_tag = NULL;

	for (i=0; i<RAID_DISKS; i++) {
		free (On_disk[i]);
		On_disk[i] = NULL;
	}
	free (On_disk);
	On_disk = NULL;

	close_raid_cache();
	// Return successfully
	//LOG cache result
	logMessage(LOG_INFO_LEVEL, "**Cache Statstistics**");
	logMessage(LOG_INFO_LEVEL, "Total cache insert: %u", cache_insert);
	logMessage(LOG_INFO_LEVEL, "Total cache get: %u", cache_access);
	logMessage(LOG_INFO_LEVEL, "Total cache hits: %u", cache_hit);
	logMessage(LOG_INFO_LEVEL, "Total cache misses: %u", cache_access-cache_hit);
	logMessage(LOG_INFO_LEVEL, "Cache efficiency: %f", (cache_hit+0.0)/(cache_access+0.0));


	logMessage(LOG_INFO_LEVEL, "TAGLINE storage device: closing completed.");
	return(0);
}

