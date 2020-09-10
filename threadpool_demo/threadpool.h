//
//  threadpool.h
//  threadpool_demo
//
//  Created by lan on 2017/5/9.
//  Copyright © 2017年 com.test. All rights reserved.
//

#ifndef threadpool_h
#define threadpool_h

#include <stdio.h>
#include <pthread.h>

typedef void *(*callback_func)(void*);

typedef struct job {
    void *argv;
    callback_func callback;
    struct job *next;
} job_t;


typedef struct pool {
    int thread_num;
    int queue_cur_num;
    int queue_max_num;
    
    job_t *head;
    job_t *tail;
    
    pthread_t *pthreads;
    pthread_mutex_t mutex;
    pthread_cond_t queue_empty;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
    
    int queue_close;
    int pool_close;
} pool_t;

pool_t *pool_create(int thread_num, int queue_max_num);
int pool_add(pool_t *pool, callback_func callbak, void *arg);
int pool_destory(pool_t *pool);

#endif /* threadpool_h */
