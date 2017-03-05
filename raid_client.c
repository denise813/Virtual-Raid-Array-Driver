////////////////////////////////////////////////////////////////////////////////
//
//  File          : raid_client.c
//  Description   : This is the client side of the RAID communication protocol.
//
//  Author        : Hao Wang
//  Last Modified : Fri 11 Dec 2015 10:43:03 PM EST 
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <raid_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

// Global data
unsigned char *raid_network_address = NULL; // Address of CRUD server
unsigned short raid_network_port = 0; // Port of CRUD server


int socket_fd;
uint64_t length;
char data;
char data_recv[1024];
struct sockaddr_in caddr;
RAIDOpCode ans;


//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_raid_bus_request
// Description  : This the client operation that sends a request to the RAID
//                server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : op - the request opcode for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

	


RAIDOpCode client_raid_bus_request(RAIDOpCode op, void *buf) {


 	char *ip = RAID_DEFAULT_IP;
	unsigned short port = RAID_DEFAULT_PORT;
	int instruction;
	instruction = op >> 56;
	//according to instruction, send op length data (some does not send data since length is suppose to be 0)

	if (instruction == RAID_INIT) {

 		caddr.sin_family = AF_INET;
 		caddr.sin_port = htons(port);
 		if ( inet_aton(ip, &caddr.sin_addr) == 0 ) {
			return( -1 );
 			logMessage(LOG_INFO_LEVEL, "Error on internet address conversion");

 		}
 		socket_fd = socket(PF_INET, SOCK_STREAM, 0);
 		if (socket_fd == -1) {
 			logMessage(LOG_INFO_LEVEL, "Error on socket creation [%s]\n", strerror(errno) );
 			return( -1 );
 		}
 		if ( connect(socket_fd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1 ) {
 			logMessage(LOG_INFO_LEVEL, "Error on socket connect [%s]\n", strerror(errno) );
 			return( -1 );
 		}

		//send op length data
 		op = htonll64(op);
		length = htonll64(0);
	
 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}	
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}	
 		ans = ntohll64(op);
		logMessage(LOG_INFO_LEVEL, "Client: INIT SUCCESS!!");
	}


	if (instruction == RAID_FORMAT) {
		//send op length data
 		op = htonll64(op);
		length = htonll64(0);

 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}


		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}	
 		ans = ntohll64(op);
		logMessage(LOG_INFO_LEVEL, "Client: FORMAT SUCCESS!!");
	}

	if (instruction == RAID_READ) {

		//send op length data
 		op = htonll64(op);
		length = htonll64(0);

 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
		
		

	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}

		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		
		length = ntohll64(length);
		if ( read( socket_fd, &data_recv, length ) != length ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		
		*(char*)buf = data_recv[0];

 		ans = ntohll64(op);
		logMessage(LOG_INFO_LEVEL, "Client: READ SUCCESS!!");
	}

	if (instruction == RAID_WRITE) {

		//send op length data
 		op = htonll64(op);
		length = htonll64(1);
		data = *((char*)buf);

 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
 		if ( write( socket_fd, &data, sizeof(data)) != sizeof(data) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		length = ntohll64(length);
		if ( read( socket_fd, &data_recv, length) != length ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
 		ans = ntohll64(op);
		logMessage(LOG_INFO_LEVEL, "Client: WRITE SUCCESS!!");
 	}


	if (instruction == RAID_CLOSE) {
		//send op length 
		op = htonll64(op);
 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}



	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
      		
		socket_fd = -1;
		logMessage(LOG_INFO_LEVEL, "Client: CLOSE SUCCESS!!");

	}

	if (instruction == RAID_DISKFAIL) {

		//send op length 
 		op = htonll64(op);
		length = htonll64(0);
		
 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}

	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
	
 		ans = ntohll64(op);

		logMessage(LOG_INFO_LEVEL, "Client: RECRIVED RAID_DISKFAIL SUCCESS");
	}


	if (instruction == RAID_STATUS) {

		//send op length 
 		op = htonll64(op);
		length = htonll64(0);
		
 		if ( write( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}


 		if ( write( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error writing network data [%s]\n", strerror(errno) );
			return( -1 );
 		}
		


	
		if ( read( socket_fd, &op, sizeof(op)) != sizeof(op) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		if ( read( socket_fd, &length, sizeof(length)) != sizeof(length) ) {
 			logMessage(LOG_INFO_LEVEL, "Error reading network data [%s]\n", strerror(errno) );
 			return( -1 );
 		}
		ans = ntohll64(op);
		logMessage(LOG_INFO_LEVEL, "Client: RECRIVED RAID_STATUS SUCCESS!!");
	}


	return(ans);

}

