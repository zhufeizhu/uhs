#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <stdatomic.h>



int task_num = 0;
int thread_num = 0;

atomic_int_fast8_t flag = 0;

int fd = 0;
char* buf;

void* write_func(void* arg){
    int idx = *(int*)arg;
    for(int i = 0; i < task_num; i+= thread_num){
        pwrite(fd,buf,4096*4,(i*thread_num + idx)*4096*4);
    }
    printf("thread %d finish\n",idx);
    flag++;
    return NULL;
}


int main(int argc, char** argv){
    int block_size = 4096;//4K
    int len = 4096;//1G
    task_num = atoi(argv[1]);
    int nblock = atoi(argv[2]);
    fd = open("/dev/nvme3n1",O_RDWR|O_DIRECT);

    if(fd < 0){
        perror("open");
        return -1;
    }

    // char* buf1 = "hello world";
    // int len = strlen(buf1);
    // printf("%d\n",len);
    // len = pwrite(raw_ssd_fd,buf1,strlen(buf1),0);
    // printf("%d\n",len);

    // if(len <= 0){
    //     perror("pwrite");
    // }


    // char* buf1 = "hello2022";
    // // char buf1[10000];
    // len = pwrite(raw_nvm_fd,buf1,9,0);
    // printf("%d\n",len);
    // if(len <= 0){
    //     perror("pread");
    // }

    //fsync(raw_nvm_fd);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    buf = valloc(4096*nblock);
    memset(buf,'o',4096*nblock);

    for(int i = 0; i < task_num; i++){
        //lseek(fd,0,0);
        pwrite(fd,buf,nblock*4096,i*nblock*4096);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    size_t time = (end.tv_sec - start.tv_sec) * 1000000 +
                                   (end.tv_nsec - start.tv_nsec) / 1000;
    printf("time is %lu us\n", time);
    printf("latency is %f us\n",(1.0 * time)/(task_num));
    printf("bandwidth is %lf MiB\n",((size_t)nblock*task_num*1000000.0)/(256*time));

    return 0;
}