#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

int task_num = 0;
int thread_num = 0;

int block_size = 4096;//4K
size_t len = (size_t)4096 * 1024 * 1024 * 32;// 128G

void* nvm_buffer;

char ch;

char buffer[4096];

char opt_buffer[4096];

void* pthread_write(void* arg){
    int idx = *(int*)arg;
    for(int i = idx; i < task_num; i += thread_num){
        //printf("[%d]:task %d\n",idx,i);
        memcpy(buffer,nvm_buffer + (size_t)i * block_size,block_size);

        if(memcmp(buffer,opt_buffer,block_size) != 0){
            printf("not equal\n");
            return;
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("error argurements\n");
        return 0;
    }

    thread_num = atoi(argv[1]);
    task_num = atoi(argv[2]);
    ch = argv[3][0];

    printf("thread num is %d\n",thread_num);
    printf("task num is %d\n",task_num);

    int nvm_fd = open("/dev/pmem0",O_RDWR);

    if(nvm_fd < 0){
        perror("open");
        return -1;
    }

    nvm_buffer = mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_SHARED,nvm_fd,0);

    if(nvm_buffer == MAP_FAILED){
        perror("mmap");
        return -1;
    }

    memset(opt_buffer,ch,4096);

    time_t start  = time(NULL);

    pthread_t pids[thread_num];
    int idx[thread_num];
    for(int i = 0; i < thread_num; i++){
        idx[i] = i;
        pthread_create(&pids[i],NULL,pthread_write,(void*)(idx+i));
    }

    for(int i = 0; i < thread_num; i++){
        pthread_join(pids[i],NULL);
    }

    time_t end = time(NULL);
    printf("time is %ld\n",end-start);
    return 0;
}

