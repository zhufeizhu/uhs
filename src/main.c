
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "mixdk.h"
#include <libpmem.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#define __USE_GNU
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


int thread_num = 0;
int task_num = 0;
char c = 0;
int nblock = 0;
size_t nvm_block_num = 2000000;

size_t offset = 0;  //

#define BUF_LEN 4
#define BLOCK_SIZE 4096
#define BUF_SIZE BLOCK_SIZE* BUF_LEN

char* buf1;
char* buf2;
char* path;

size_t (*func)(void*,size_t,size_t,size_t,int);

size_t offset;
char* pmemaddr;
static atomic_int_fast32_t threads_start = 0;
static atomic_int_fast32_t threads_end = 0;

void* write_func(void* arg) {
    int idx = *(int*)arg;
    struct sched_param param;
    param.sched_priority = 50;
    sched_setscheduler(0, SCHED_FIFO, &param);
    cpu_set_t mask;  //CPU核的集合
    cpu_set_t get;   //获取在集合中的CPU
    printf("the thread is:%d\n",idx);  //显示是第几个线程
    CPU_ZERO(&mask);    //置空
    CPU_SET(idx,&mask);   //设置亲和力值
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)//设置线程CPU亲和力
    {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    threads_start++;
    while(threads_start < thread_num);
    int start = idx * (task_num / thread_num);
    int end = (idx+1)* (task_num / thread_num);
    for(int i = start; i < end; i++){
        // func(buf1,nblock,offset + (i-start)*nblock,0,idx);
        //printf("nvm %d\n",idx);
        func(buf1,nblock,offset + 0,0,idx);
    }
    // printf("finish\n");
    // threads_end--;
    return NULL;
}


atomic_int_fast16_t ssd_block_num = 0;
void* ssd_write(void* arg) {
    int idx = *(int*)arg;
    printf("idx is %d\n",idx);
    struct sched_param param;
    param.sched_priority = 50;
    sched_setscheduler(0, SCHED_FIFO, &param);
    cpu_set_t mask;  //CPU核的集合
    cpu_set_t get;   //获取在集合中的CPU
    printf("the thread is:%d\n",idx);  //显示是第几个线程
    CPU_ZERO(&mask);    //置空
    CPU_SET(idx,&mask);   //设置亲和力值
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)//设置线程CPU亲和力
    {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    threads_start++;
    while(threads_start < 2 * thread_num);
    int i = 0;
    int d = idx - thread_num;
    //if(d>0) return NULL;
    while(likely(threads_end == 0)){
        //printf("ssd %d\n",idx);
        func(buf1,nblock,offset + i*nblock,0,idx);
        //if (d == 0 && i%10000 == 0) printf("i is %d\n",i);
        i++;
    }
    printf("%d thread ssd block num is %d\n",idx,i*nblock);
    return NULL;
}


void* nvm_write(void* arg) {
    int idx = *(int*)arg;
    printf("idx is %d\n",idx);
    struct sched_param param;
    param.sched_priority = 50;
    sched_setscheduler(0, SCHED_FIFO, &param);
    cpu_set_t mask;  //CPU核的集合
    cpu_set_t get;   //获取在集合中的CPU
    printf("the thread is:%d\n",idx);  //显示是第几个线程
    CPU_ZERO(&mask);    //置空
    CPU_SET(idx,&mask);   //设置亲和力值
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)//设置线程CPU亲和力
    {
        printf("warning: could not set CPU affinity, continuing...\n");
    }
    threads_start++;
    while(threads_start < 2*thread_num);
    int i = 0;
    while(likely(threads_end == 0)){
        func(buf2,1,(i++)%10000,0,idx);
    }
    printf("%d thread nvm block num is %d\n",idx,i);
    // for(int i = 0; i < task_num; i++){
    // }
    // // printf("finish\n");
    // threads_end--;
    return NULL;
}
 
int idx[16];
int main(int argc, char** argv) {
    if(argc != 7){
        printf("参数输入错误: thread_num task_num nblock content path type\n");
        return 0;
    }

    thread_num = atoi(argv[1]);
    task_num = atoi(argv[2]);
    nblock = atoi(argv[3]);
    c = argv[4][0];
    path = argv[5];

    printf("thread num is %d\n",thread_num);
    printf("task num is %d\n", task_num);
    printf("nblock num is %d\n", nblock);
    printf("content is %c\n", c);
    printf("path is %s\n",path);
    printf("type is %s\n",argv[6]);
    if(strcmp(argv[6],"ssd_read")==0){
        func = mixdk_read;
        offset = nvm_block_num;
    }else if(strcmp(argv[6],"ssd_write") ==0){
        func = mixdk_write;
        offset = nvm_block_num;
    }else if(strcmp(argv[6],"nvm_read") ==0){
        func = mixdk_read;
    }else if(strcmp(argv[6],"nvm_write") == 0){
        func = mixdk_write;
    }
    threads_end = 0;
    printf("mix init begin\n");
    mixdk_init(thread_num);
    printf("mix init succeed\n");
    buf1 = valloc(BLOCK_SIZE*nblock);
    memset(buf1,'c',BLOCK_SIZE*nblock);
    buf2 = valloc(BLOCK_SIZE*nblock);
    // int n = 0;
    pthread_t* pids = malloc(sizeof(pthread_t*) * thread_num);
    struct timespec start, end;



    for(int i = 0; i < thread_num * 2; i++)
        idx[i] = i;

    for (int i = 0; i < thread_num; i++) {
        //printf("idx is %d\n",idx[i]);
        if (pthread_create(pids + i, NULL, ssd_write, (void*)(idx + i))) {
            perror("create thread");
            return 0;
        }
    }

    for (int i = thread_num; i < 2*thread_num; i++) {
        printf("idx is %d\n",idx[i]);
        if (pthread_create(pids + i, NULL, nvm_write, (void*)(idx + i))) {
            perror("create thread");
            return 0;
        }
    }

    while(threads_start < thread_num*2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    size_t time = 0;
    do{
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time = (end.tv_sec - start.tv_sec) * 1000000 +
            (end.tv_nsec - start.tv_nsec) / 1000;
    }while(time < 2000000);
    threads_end = 1;

    for(int i = 0; i < thread_num*2; i++)
        pthread_join(pids[i], NULL);    

   
    printf("time is %llu us\n", time);
    printf("latency is %f us\n",(1.0 * time)/(task_num));
    printf("bandwidth is %lf MiB\n",((size_t)nblock*task_num*1000000.0)/(256*time));
    
    return 0;
} 
