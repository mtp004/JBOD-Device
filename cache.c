#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if(cache_size!=0 || num_entries<2 || num_entries>4096)     //if cache already initialized or size is greater than 4096 or smaller than 2 then fail
  {
    return -1;
  } else
  {
    cache=(cache_entry_t*)malloc(num_entries*sizeof(cache_entry_t));  //allocating an array of size num_entries to cache
    cache_size=num_entries;    //update cache_size
    return 1;
  }
}

int cache_destroy(void) {
  if(cache_size==0)     //no cache is intialized, fail
  {
    return -1;
  }else
  {
    free(cache);         //deallocate cache and set it back to null
    cache=NULL;
    cache_size=0;        //update cache_size when destroyed
    return 1;
  }
}

int detect_duplicate(int disk_num, int block_num)//helper function that returns the index number of cache entry of disk_num and block_num
{
  if(cache_size!=0)
  {
    int i=0;
    while(cache[i].valid==true)   //while loop to check for matching entry
    {
      if(cache[i].disk_num==disk_num && cache[i].block_num==block_num)
      {
        return i;
      }
      i++;
    }
  }
  return -1;       //if no match found, return -1
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  num_queries++;      //increment num_queries regardless of output
  if(cache_size==0 || buf==NULL)
  {
    return -1;
  } else
  {
    int match_index=detect_duplicate(disk_num,block_num);     //get match entry index
    if(match_index!=-1)                 //checks if there is a match or not
    {
      if(cache[match_index].valid==true){
        memcpy(buf,cache[match_index].block, 256);    //if there is am match, copy its block content into buf
        num_hits++, clock++;       //update clock and num_hit
        cache[match_index].access_time=clock;        //update its access_time
        return 1;
      }
    }
    return -1;
  }
}

int get_lru_index()      //helper function to get the least recently used entry index in the cache array
{
  int lru_index=0;
  for(int i=0; i<cache_size; i++)   //start searching from the beginning of the cache
  {
    if(cache[i].access_time<cache[lru_index].access_time)
    {
      lru_index=i;
    }
  }
  return lru_index;        //if there is any cache entry that is not valid, it will return its index to fill up the cache before replacing valid entry
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) 
{
  int dup_index=detect_duplicate(disk_num, block_num);
  if(dup_index!=-1)     //if there is a match, update the entry
  {
    memcpy(cache[dup_index].block,buf,256);
    cache[dup_index].access_time=clock;
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if(buf==NULL || cache==NULL)     //fail if buf is NULL and cache is not initialized
  {
    return -1;
  } else if(disk_num>15 || disk_num<0 || block_num>255 || block_num<0) //checks for out of bound disk and block argument
  {
    return -1;
  } else
  {
    if(detect_duplicate(disk_num, block_num)!=-1)    //if there is a duplicate, fail
    {
      return -1;
    } else
    {
      clock++;
      int insert_index=get_lru_index();   //get the least recently used, if there are invalid entry, that will be use first before replacing valid entry
      cache[insert_index].disk_num=disk_num;
      cache[insert_index].block_num=block_num;
      cache[insert_index].valid=true;
      cache[insert_index].access_time=clock;
      memcpy(cache[insert_index].block,buf,256);
      return 1;
    }
  }
}

bool cache_enabled(void) 
{
  if(cache_size>1)   //if cache is initialized, return true
  {
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
