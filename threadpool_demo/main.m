//
//  main.m
//  threadpool_demo
//
//  Created by lan on 2017/5/9.
//  Copyright © 2017年 com.test. All rights reserved.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "threadpool.h"

void *func(void *arg) {
    printf("thread %d will sleep 1 sec.\n", *(int*)arg);
    sleep(1);
    return NULL;
}

int main(int argc, const char * argv[]) {
    pool_t *pool = NULL;
    pool = pool_create(5, 13);
    if (pool == NULL) {
        perror("create pool error");
        return -1;
    }
    
    int num[50];
    for (int i = 0; i < 50; i++) {
        num[i] = i;
    }
    
    
    for (int i = 0; i < 50; i++) {
        pool_add(pool, func, (void *)&num[i]);
    }
    
//        sleep(5);
    pool_destory(pool);
    
    return 0;
}
