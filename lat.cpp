/*k_sudarsun@yahoo.com, Feb-2007*/
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ITER   1
#define MB    (1024*1024)
#define MAX_N 16*MB
#define STEPS_N 16*MB
#define KB    1024
#define CACHE_LN_SZ 64
// LLC Parameters assumed
#define START_SIZE 1*MB
#define STOP_SIZE  256*MB
#define MIN_LLC    256*KB
#define MAX_STRIDE 1024
#define INT_MIN     (-2147483647 - 1)

using namespace std;
unsigned int memrefs;
volatile void* global;


void LineSizeTest(void)
{    
  double retval=0, prev=0;
  int j=0,s=0, stride=0;
  int diff =0;
  int max =INT_MIN,maxidx=0,index = 0;
  struct timespec tps, tpe;
  char *array = (char *)malloc(sizeof(char) *CACHE_LN_SZ * STEPS_N);

  for (j=4; j <= MAX_STRIDE/sizeof(int); j=j<<1) {
    stride = j;
    clock_gettime(CLOCK_REALTIME, &tps);
    for (s=1; s < STEPS_N; s++){
      array[s*(stride+2) & STEPS_N-1]++;
    }
    clock_gettime(CLOCK_REALTIME, &tpe);
    retval = ((tpe.tv_sec-tps.tv_sec)*1000000000  + tpe.tv_nsec-tps.tv_nsec)/1000;
    if(index > 0){
      diff = retval - prev;
      if(max < diff) {
      	max= diff;
      	maxidx =j;
      }
    }
    //cout << "CacheLineTest "<< retval <<" stride "<<j<< " diffs: "<<diff<<endl;
    prev = retval;
    index++;
  }
  cout << "CacheLine size " <<maxidx/2<<" B"<<endl;
  free(array);
}


void LLCCacheSizeTest(unsigned int arg)
{    
  double retval,prev;
  struct timespec tps, tpe;
  int max =INT_MIN,index = 0, maxidx = 0, iter = 0, i=0;
  int diff =0, stride = 0, j = MIN_LLC;
  double change_per[10];
  char *array = (char *)malloc(sizeof(char) *CACHE_LN_SZ * STEPS_N);

  while( j < STOP_SIZE) {

    stride = j-1;
    clock_gettime(CLOCK_REALTIME, &tps);

    for (iter = 0; iter < ITER; iter++)
      for (unsigned int i = 0; i < STEPS_N; i++) {
      	/*First read into cache
        * make sure to access diff cache line.
        * Values are not important*/
      	array[(i*CACHE_LN_SZ) & stride] *= 100;
      }
    clock_gettime(CLOCK_REALTIME, &tpe);
    retval = ((tpe.tv_sec-tps.tv_sec)*1000000000  + tpe.tv_nsec-tps.tv_nsec)/1000;

    if(index > 0){
      diff = retval - prev;
      if(max < diff) {
      	max= diff;
      	maxidx =j;
      }
    }
    //cout << "LLCCacheSizeTest "<< retval <<" stride "<<j<< " diffs: "<<diff<<endl;
    prev = retval;
    if(index == 0) j = MB;
    else j = j + (MB);
    index++;
  }
  free(array);
  array = NULL;
  cout<<"Effective LLC Cache Size "<<maxidx/MB/2<<" MB"<<endl;
}

/////////////////////////////////////////////////////////
// node structure padded to cache line size
/////////////////////////////////////////////////////////
struct node {
  int val;
  struct node *next;
  struct node *prev;
  //pad to make each node a cache line sz;
  char padding[CACHE_LN_SZ - 2 * sizeof(struct node*) - sizeof(int)];
};


struct node* delete_node( struct node *j) {

  if(!j) return NULL;

  j->prev->next = j->next;
  j->next->prev = j->prev;
  //ok I am free
  return j;
}

struct node* insert_node( struct node *i, struct node *j) {

  if(!j) return NULL;

  i->next->prev = j;
  j->next = i->next;
  j->prev = i;
  i->next = j;
  return i;
}

double MemoryTimingTest(void)
{    
  double retval;
  int j = 0,sequential=0, i=0;
  struct timespec tps, tpe;
  int index = 0;
  unsigned long avg_memref[256],avg=0,ws=0;
  unsigned int idx=0;
  struct node *arr=NULL;

  for(i=START_SIZE; i<STOP_SIZE; i=i<<1) {
    ws = i;
    arr = (struct node *)malloc(sizeof(struct node) * ws);
    //first link all the node continuos
    for(idx =1; idx < ws-1; idx++){
      arr[idx].val = 1;
      arr[idx].prev = &arr[idx-1];
      arr[idx].next = &arr[idx+1];
    }
    //set the boundary values
    arr[0].prev = &arr[ws-1];
    arr[0].next = &arr[1];
    arr[0].val =1;
    arr[ws-1].prev= &arr[ws-2];
    arr[ws-1].next = &arr[0];
    arr[ws-1].val = 1;
    //fill random
    srand ( time(NULL) );

    if(!sequential){
      /*Now start linking random nodes
       delete old node links and then create
       new links*/
      for(idx =0; idx < ws; idx++) {
      	//generate a random id
      	j = rand()% (ws-1);
      	delete_node(&arr[j]);
      	//insert between i and its next
      	insert_node(&arr[idx],&arr[j]);
      }
    }

    /*Now visit/iterate the linked list
      sum the values*/
    struct node *tmp = &arr[0];
    int val =0;

    clock_gettime(CLOCK_REALTIME, &tps);

    for(idx =0; idx < STEPS_N; idx++){
      tmp = tmp->next;
      ++tmp->val;
    }

    clock_gettime(CLOCK_REALTIME, &tpe);
    retval = (tpe.tv_sec-tps.tv_sec)*1000000000  + tpe.tv_nsec-tps.tv_nsec;

    memrefs = STEPS_N;
    avg_memref[index] = retval/memrefs/2;
    //cout << "MemTiming test "<<avg_memref[index]<<" wss: "<<i*64<<endl;
    index++;
    free(arr);
    arr=NULL;
  }
  cout<<"Avg. memory access latency: "<<avg_memref[index-1]<<" ns"<<endl;
  return retval;
}

/////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
  //LineSizeTest();
  //LLCCacheSizeTest(0);
  MemoryTimingTest();
  //cout<<".........."<<endl;
  //cout<<""<<endl;
  return 0;
}
/////////////////////////////////////////////////////////
