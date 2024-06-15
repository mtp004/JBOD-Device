/* Author: Minh Tri PHam
   Date:02/25/2024
*/

/***
 *      ______ .___  ___. .______     _______.  ______        ____    __   __
 *     /      ||   \/   | |   _  \   /       | /      |      |___ \  /_ | /_ |
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'        __) |  | |  | |
 *    |  |     |  |\/|  | |   ___/   \   \    |  |            |__ <   | |  | |
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.       ___) |  | |  | |
 *     \______||__|  |__| | _|   |_______/     \______|      |____/   |_|  |_|
 *
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cache.h"
#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "net.h"
//declared a variable to keep track of whether the JBOD is mounted or not, started as not mounted.
int IS_MOUNTED=0;

//defined a function to that takes disk_ID, block_ID and enum command and combines them to create an uint32_t op
uint32_t encode_operation(int disk_ID, int block_ID, int command)
{
  //enum command  from bit 14 to 19, disk_ID from bit 28to 31, block_ID from bit 20 to 27
  uint32_t op=(uint32_t)command << 14| (uint32_t)disk_ID << 28 | (uint32_t)block_ID << 20;
  return op;
}

//defines mount operation
int mdadm_mount(void) {
  //creates uint32_t op that uses JBOD_MOUNT to mount the disk and passed it in the given jbod_client_operation() function
  // since mount ignores disk and block number, I used 0 and 0 as their value since it doesn;t  matter
  uint32_t mount_op=encode_operation(0,0, JBOD_MOUNT);
  //if mount fails return -1
  if(jbod_client_operation(mount_op, NULL)==-1)
  {
    return -1;
  } else
  {
    IS_MOUNTED=1;
    return 1;
  }
}

//defines unmount operation
int mdadm_unmount(void) {
  //creates uint32_t op that uses JBOD_UNMOUNT to unmount the disks and passed it in the given jbod_client_operation() function
  // since unmount ignores disk and block number, I used 0 and 0 as their value since it doesn;t  matter
  uint32_t unmount_op=encode_operation(0,0, JBOD_UNMOUNT);
  //checks if unmount fails, return -1
  if(jbod_client_operation(unmount_op, NULL)==-1)
  {
    return -1;
  } else
  {
    IS_MOUNTED=0;
    return 1;
  }
}

//getter function that takes int address and return the ID number of the disk that the address is contained in.
int get_disk_num(uint32_t addr)
{
  int disk_ID=addr/65536;
  return disk_ID;
}

//getter function that takes int address and return the ID number of the block that the address is contained in.
int get_block_num(uint32_t addr)
{
  int remainder_block=addr%65536;
  int block_ID=remainder_block/256;
  return block_ID;
}

//method that takes in integer disk number and construct the uint32_t operation to seek to that specific disk number
//It is important to call this before changing block number since block number resets to 0- after changing disk
int go_to_disk(int disk_num)
{
  uint32_t change_disk_op=encode_operation(disk_num,0, JBOD_SEEK_TO_DISK);
  //set returns value for debugging, the return values aren't used by the program.
  if(jbod_client_operation(change_disk_op, NULL)==-1)
  {
    return -1;
  }
  return 1;
}

//method that takes in integer block number and construct the uint32_t operation to seek to that specific block number
int go_to_block(int block_num)
{
  uint32_t change_block_op=encode_operation(0, block_num, JBOD_SEEK_TO_BLOCK);
  //set returns value for debugging, the return values aren't used by the program.
  if(jbod_client_operation(change_block_op, NULL)==-1)
  {
    return -1;
  }
  return 1;
}

//defined method that takes in a uint8_t buffer and copy the entire current block into that buffer at the current disk and block.
//important that buffer must be size 256 since blokc size is 256
int read_block(uint8_t* buf)
{
  uint32_t read_op=encode_operation(0, 0, JBOD_READ_BLOCK);
  //set returns value for debugging, the return values aren't used by the program.
  if(jbod_client_operation(read_op, buf)==-1)
  {
    return -1;
  }
  return 1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  // if statement to check if mounted, if read length is not greater than 1024 byte, and end address is not out of bound
  if(addr+len>1048575 || IS_MOUNTED==0 || len>1024)
  {
    return -1;
  } else if (buf==NULL && len!=0)       //if statement checks for buf is NULL and read length is not 0, which it should fail
  {
    return -1;
  } else
  {
    uint8_t temporary[256];                      //declare temporary buffer that is 256 byte long

    int disk_num=get_disk_num(addr);             //declare disk_num of the the given address argument
    int block_num=get_block_num(addr);           //declare block_num of the the given address argument
    
    go_to_disk(disk_num);                        //call to seek to disk specified by the starting address and prepare for read
    go_to_block(block_num);
    if(cache_lookup(disk_num,block_num,temporary)==-1)
    {
      read_block(temporary);
      cache_insert(disk_num,block_num,temporary);  
    }             
    //code segment to filter out only the wanted bytes
    int index=0;                   //declare index variables of the buffer that will be read, starting at buff[0]
    for(int i=addr; i<addr+len; i++)    //for loops to read from starting address to end address
    {
      int j=i%256;          //j specifies the current byte number in the current block that will be read to buff from temporary, can never be greater than 255
      //if statement to check for the end of disk and disk transition
      //will be check first compare to block transition because once we change disk, there is no need to change block since it wll reset to 0
      if(i%65536==0 && i!=0)
      {
        go_to_disk(disk_num);                        //call to seek to disk specified by the starting address and prepare for read
        go_to_block(block_num);
        disk_num=get_disk_num(i);
        block_num=get_block_num(i); 
        if(cache_lookup(disk_num,block_num,temporary)==-1)//checks in cache for specified block
        {
          read_block(temporary);
          cache_insert(disk_num,block_num,temporary);  
        }       
      } else if(j==0 && i!=0)
      //if statement to check for blokc transition, have lower priority than checking disk transition
      //since read_block automatically move to the next block and read if called, no need to seek to next block
      {
        if(cache_lookup(disk_num,block_num,temporary)==-1)//checks in cache for specified block
        {
          read_block(temporary);
          cache_insert(disk_num,block_num,temporary);  
        }       
      }
      //code to read temporary array into buff
      buf[index]=temporary[j];
      index++;
    }
  }
  //if succeed, return len
  return len;
}

int write_block(uint8_t* buf)
{
  //function writing buff to the current block
  uint32_t write_op=encode_operation(0, 0, JBOD_WRITE_BLOCK);
  if(jbod_client_operation(write_op, buf)==-1)
  {
    return -1;
  }
  return 1;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if(buf==NULL && len==0)          //checking case where buf is NULL and len is 0 and do nothing
  {
    return len;
  } else if(addr+len-1>1048575 || IS_MOUNTED==0 || len>1024)        //checking for out of bound writing condition
  {
    return -1;
  } else if (buf==NULL && len!=0)        //  checking fail case where buf is NULL yet read len is not 0
  {
    return -1;
  } else
  {
    uint8_t temporary[256];         
    int current_block_lbound=addr-addr%256;         //declare variable to keep track of the starting bit number for the current block for writting
    int current_block_ubound=current_block_lbound+255;       //declare variable to keep track of the ending bit number for the current block for writting
    int write_addr=addr;      //the starting address to write for each block everytime write_block operation is called
    int buff_idx=0;         //variable to keep track of the given buffer index
    int write_len=0;
    int end_addr=addr+len;    //variable representing the end write address, use to terminate while loop for writing purposes

    int disk_num=get_disk_num(addr);
    int block_num=get_block_num(addr);
    go_to_disk(disk_num);                       
    go_to_block(block_num); 
    while(write_addr<end_addr)     //while loop to start writing
    {
      disk_num=get_disk_num(write_addr);
      block_num=get_block_num(write_addr);
      if(write_addr%65536==0)     //checks for disk transition
      {
        go_to_disk(get_disk_num(write_addr));  
      }
      if(current_block_lbound>addr)       //checks whether the first whole block is written or just a fraction of it.
      {
        write_addr=current_block_lbound;
      }
      write_len=current_block_ubound-write_addr+1;
      if(current_block_ubound>end_addr-1)     //checks wther the last whole block is written or just a fraction of it
      {
        write_len=end_addr-write_addr;
      }
      if(write_len!=256)             //checks if a whole block is written, if it is, there no need to write part of buf into temporary array.
      {
        if(cache_lookup(disk_num,block_num,temporary)==-1)
        {
          go_to_disk(disk_num);                        //call to seek to disk specified by the starting address and prepare for read
          go_to_block(block_num);
          read_block(temporary);
          cache_insert(disk_num,block_num,temporary);  //checks in cache for specified block
        }   
        go_to_block(get_block_num(write_addr)); 
      }
      memcpy(temporary+write_addr%256,buf+buff_idx,write_len);
      write_block(temporary);
      cache_update(disk_num,block_num,temporary);    //everytime write is called, update the corresponding entry in cache with new write data
      write_addr+=write_len, buff_idx+=write_len;     //after every write, the starting write_addr is updated so that next time it will start from there
      current_block_lbound+=256, current_block_ubound+=256;   //after current block is written, increment the current block upper and lower bound
    }
  }
  return len;
}


/*CWD /home/cmpsc311/sp24-lab4-mtp004 */
/*CWD /home/cmpsc311/sp24-lab4-mtp004 */
