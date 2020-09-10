//
//  threadpool.c
//  threadpool_demo
//
//  Created by lan on 2017/5/9.
//  Copyright © 2017年 com.test. All rights reserved.
//

#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void *pool_function(void *arg) {
    pool_t *pool = (pool_t *)arg;
    job_t *job = NULL;
    while (1) {
        pthread_mutex_lock(&(pool->mutex));
        
        while (pool->queue_cur_num == 0 && !pool->pool_close) {    // 队列为空，等待队列非空
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex)); // 等待 pool_add 函数发送 queue_not_empty 信号
        }
        
        if (pool->pool_close) {   // 线程池关闭，线程就退出
            pthread_mutex_unlock(&(pool->mutex));
            pthread_exit(NULL);
        }
        
        pool->queue_cur_num--;
        job = pool->head;        // 从队列头部取出 任务
        if (pool->queue_cur_num == 0) {
            pool->head = pool->tail = NULL;
        } else {
            pool->head = job->next;
        }
        
        if (pool->queue_cur_num == 0) {   // 通知 destory 函数可以销毁线程池了
            pthread_cond_signal(&(pool->queue_empty));
        } else if (pool->queue_cur_num <= pool->queue_max_num - 1) {
            pthread_cond_broadcast(&(pool->queue_not_full));
        }
    
        pthread_mutex_unlock(&(pool->mutex));
        
        (*(job->callback))(job->argv);
        free(job);
        job = NULL;
    }
}


pool_t *pool_create(int thread_num, int queue_max_num) {
    pool_t *pool = NULL;
    pool = (pool_t *)malloc(sizeof(pool_t));
    if (pool == NULL) {
        perror("failed to malloc pool\n");
        return NULL;
    }
    
    pool->thread_num = thread_num;
    pool->queue_cur_num = 0;
    pool->queue_max_num = queue_max_num;
    pool->head = pool->tail = NULL;

    int result;
    result = pthread_mutex_init(&(pool->mutex), NULL);
    if (result < 0) {
        perror("failed to init mutex");
        return NULL;
    }
    
    result = pthread_cond_init(&(pool->queue_empty), NULL);
    if (result < 0) {
        perror("failed to init queue_empty");
        return NULL;
    }
    
    result = pthread_cond_init(&(pool->queue_not_empty), NULL);
    if (result < 0) {
        perror("failed to init queue_not_empty");
        return NULL;
    }
    result = pthread_cond_init(&(pool->queue_not_full), NULL);
    if (result < 0) {
        perror("failed to init queue_not_full");
        return NULL;
    }
    
    pool->pthreads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
    if (pool->pthreads == NULL) {
        perror("failed to malloc pthreads");
        return NULL;
    }
    
    pool->queue_close = 0;
    pool->pool_close = 0;
    for (int i = 0; i < pool->thread_num; i++) {
        result = pthread_create(&(pool->pthreads[i]), NULL, pool_function, (void *)pool);
        if (result < 0) {
            perror("failed to create pthreas\n");
        }
    }
    return pool;
}


int pool_add(pool_t *pool, callback_func callbak, void *arg) {
    if (pool == NULL || callbak == NULL) {
        return -1;
    }
    pthread_mutex_lock(&(pool->mutex));
    while (pool->queue_cur_num == pool->queue_max_num && !(pool->pool_close || pool->queue_close)) {             // 队列已经满等待，
        pthread_cond_wait(&(pool->queue_not_full), &(pool->mutex));  // 等待 pool_function 发送 queue_not_full 信号
    }
    
    if (pool->queue_close || pool->pool_close) {   // 队列关闭或者线程池关闭则退出
        printf("failed to add, pool already closed\n");
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    job_t *job = NULL;
    job = (job_t *)malloc(sizeof(job_t));
    if (job == NULL) {
        perror("failed to malloc job");
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    job->argv = arg;
    job->callback = callbak;
    job->next = NULL;
    
    if (pool->head == NULL) {      // 队列为空时
        pool->head = pool->tail = job;
        pthread_cond_broadcast(&(pool->queue_not_empty)); // 有任务来了，通知线程池中的线程队列非空，通知等待在 pool_function 的线程处理
    } else {
        pool->tail->next = job;
        pool->tail = job;         // 把任务插入到队尾
    }
    pool->queue_cur_num ++;
    
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

int pool_destory(pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }
    pthread_mutex_lock(&(pool->mutex));
    if (pool->queue_close && pool->pool_close) { // 线程池已经退出，直接返回
        pthread_mutex_unlock(&(pool->mutex));
        return 0;
    }
    
    pool->queue_close = 1;   // 关闭任务队列, 不接受新的任务了
    while (pool->queue_cur_num != 0) {
        pthread_cond_wait(&(pool->queue_empty), &(pool->mutex)); // 等待已经加入的队列的任务全部执行完成
    }
    
    pool->pool_close = 1;                   // 关闭线程池
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_broadcast(&(pool->queue_not_empty));  // 唤醒线程池中正在阻塞的线程
    pthread_cond_broadcast(&(pool->queue_not_full));   // 唤醒添加任务的 pool_add 函数
    
    for (int i = 0; i < pool->thread_num; i++) {
        pthread_join(pool->pthreads[i], NULL);  // 等待线程池中的所有线程执行完毕
    }
    
    pthread_mutex_destroy(&(pool->mutex));    // 清理资源
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_full));
    pthread_cond_destroy(&(pool->queue_not_empty));
    
    free(pool->pthreads);
    pool->pthreads = NULL;
    
    job_t *job = NULL;
    while (pool->head != NULL) {
        job = pool->head;
        pool->head = job->next;
        free(job);
        job = NULL;
    }
    
    free(pool);
    pool = NULL;
    return 0;
}
