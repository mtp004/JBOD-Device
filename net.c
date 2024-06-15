#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int n_read = 0;
  // loop to keep reading until the specified number of bytes have been read
  while(n_read < len) {
      // use read() system call to read the remaining bytes from fd
    int n=read(fd, &buf[n_read], len - n_read);
    // if read() returns a non-positive value, it indicates a failure, so return false
    if(n==-1) 
    {
      return false;
    }
    n_read+=n;
  }
  return true;
}


/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int n_write = 0;
  while (n_write < len) 
  {
    // use write() system call to write the remaining bytes to fd
    int n=write(fd, &buf[n_write], len - n_write);
    // if write() returns a non-positive value, it indicates a failure, so return false
    if(n==-1) 
    {
      return false;
    }
    n_write += n;
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  if(sd==-1)
  {
    return false;
  }
  uint16_t len;
  uint8_t header[HEADER_LEN];       //declare array of size 8 to read the header and extract information

  if(nread(sd,HEADER_LEN, header)==false)      //read the header
  {
    return false;
  }
  memcpy(&len, header, sizeof(len));       //extract len
  len=ntohs(len);

  memcpy(ret, header+6, sizeof(*ret));       //extract return value
  *ret=ntohs(*ret);

  memcpy(op, header+2, sizeof(*op));            // extract op value
  *op=ntohl(*op);  

  if(len==(HEADER_LEN+JBOD_BLOCK_SIZE) && *ret==0)      // if len is greater than 8, continue to read the block that was send.
  {
    if(nread(sd,JBOD_BLOCK_SIZE,block)==false)
    {
      return false;
    }
  }
  return true;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  if(sd==-1)
  {
    return false;
  }
  uint16_t ret=0; 
  uint32_t cmd=(op >> 14) & 63;       //extracting the command from op
  uint16_t len = HEADER_LEN;           
  if(cmd == JBOD_WRITE_BLOCK) {         //if the command is write, then len include the block length
    len += JBOD_BLOCK_SIZE;
  }
  uint8_t header[len];          // declare an array of size len
  uint16_t length = htons(len);
  op = htonl(op);
  ret=htons(ret);
  memcpy(header, &length, sizeof(len));      //writing the necessary information into the array
  memcpy(header+2, &op, sizeof(op));
  memcpy(header+6, &ret, sizeof(ret));
  if(cmd == JBOD_WRITE_BLOCK) {           //if the command is write then copy the block  argument into the array
    memcpy(header+8, block, JBOD_BLOCK_SIZE);
  }
  if (nwrite(sd, len, header) == false) {       //write the array to server
    return false;
  }
  return true;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  cli_sd=socket(AF_INET, SOCK_STREAM, 0);         //creating socket
  if(cli_sd==-1)
  {
    return false;
  }
  struct sockaddr_in server_addr;       //assigning address into the socket struct
  server_addr.sin_family=AF_INET;
  server_addr.sin_port=htons(port);
  if(inet_aton(ip, &server_addr.sin_addr)==0)
  {
    return false;
  }
  if(connect(cli_sd,(const struct sockaddr*)&server_addr, sizeof(server_addr))==-1)
  {
    return false;
  }
  return true;
}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd=-1;
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block)
{
  if(cli_sd==-1) 
  {
    return -1;
  }
  uint16_t ret;
  if(send_packet(cli_sd, op, block)==true)    //send the packet and receive the server respond.
  {
    recv_packet(cli_sd, &op, &ret, block);
    return 0;
  } else {
    return -1;
  }
}
