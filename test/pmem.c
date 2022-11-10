#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <libpmem.h>

int thread_num = 0;
int task_num = 0;
char c = 0;
int nblock = 0;
size_t nvm_block_num = 2000000;

size_t offset = 0;  //

#define BUF_LEN 4

#define TASK

#define BLOCK_SIZE 4096

#define BUF_SIZE BLOCK_SIZE* BUF_LEN

char* buf1;
char* buf2;
char* path;

size_t (*func)(void*,size_t,size_t,size_t,int);

size_t offset;
extern nvm_info_t* nvm_info;
char* pmemaddr;

void* write_func(void* arg) {
    int idx = *(int*)arg;
    char* test = "hello, persistent memory";
    printf("%s\n",test);
    pmem_memcpy_persist(pmemaddr,test,10);
    printf("hello world\n");
    // struct sched_param param;
    // param.sched_priority = 50;
    // sched_setscheduler(0, SCHED_FIFO, &param);
    // for(int i = 0; i < task_num; i+= thread_num){
    //     func(buf1,nblock,offset+ idx*task_num + i*nblock,0,idx);
    // }
    return NULL;
}
/* using 4k of pmem for this example */
#define PMEM_LEN (size_t)16*1024*1024*1024
#define PATH "/mnt/pmem1/myfile" 

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
    // if(strcmp(argv[6],"ssd_read")==0){
    //     func = mixdk_read;
    //     offset = nvm_block_num;
    // }else if(strcmp(argv[6],"ssd_write") ==0){
    //     func = mixdk_write;
    //     offset = nvm_block_num;
    // }else if(strcmp(argv[6],"nvm_read") ==0){
    //     func = mixdk_read;
    // }else if(strcmp(argv[6],"nvm_write") == 0){
    //     func = mixdk_write;
    // }
    
    // printf("mix init begin\n");
    // mixdk_init(thread_num);
    // printf("mix init succeed\n");
    //mix_segments_clear();
    size_t mapped_len = (size_t)4096 * 1024*1024;
    int is_pmem;

    /* create a 4k pmem file and memory map it */
    if ((pmemaddr = pmem_map_file(PATH, PMEM_LEN, PMEM_FILE_CREATE,
                0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }
    
    buf1 = valloc(BLOCK_SIZE*nblock);
    memset(buf1,'c',BLOCK_SIZE*nblock);
    buf2 = valloc(BLOCK_SIZE*nblock);
    // int n = 0;
    pthread_t* pids = malloc(sizeof(pthread_t*) * thread_num);
    struct timespec start, end;

    int* idx = (int*)malloc(sizeof(int) * thread_num);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (int i = 0; i < thread_num; i++) {
        idx[i] = i;
        if (pthread_create(pids + i, NULL, write_func, (void*)(idx + i))) {
            perror("create thread");
            return 0;
        }
    }
    sleep(100);
    for(int i = 0; i < thread_num; i++)
        pthread_join(pids[i], NULL);

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    size_t time = (end.tv_sec - start.tv_sec) * 1000000 +
                                   (end.tv_nsec - start.tv_nsec) / 1000;
    printf("time is %llu us\n", time);
    printf("latency is %f us\n",(1.0 * time)/(task_num));
    printf("bandwidth is %lf MiB\n",((size_t)nblock*task_num*1000000.0)/(256*time));
    
    return 0;
} 
