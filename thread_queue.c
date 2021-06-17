#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <malloc.h>

#include "thread_queue.h"

//from http://cgi.di.uoa.gr/~antoulas/k24/lectures/l13.pdf

ThreadQueue* thread_queue_create(int thread_queue_items) {
	ThreadQueue* tq = malloc(sizeof (ThreadQueue));
	tq->thread_queue_items = thread_queue_items;

	tq->data = (ThreadQueueItem*) malloc(thread_queue_items * sizeof (ThreadQueueItem)); //create hashtable for countries

	tq->start = 0;
	tq->end = -1;
	tq->count = 0;
	
	tq->items_processed = 0;

	pthread_mutex_init(&tq->mtx, 0);
	pthread_cond_init(&tq->cond_nonempty, 0);
	pthread_cond_init(&tq->cond_nonfull, 0);

	return tq;
}

void thread_queue_destroy(ThreadQueue* tq) {
	pthread_cond_destroy(&tq->cond_nonempty);
	pthread_cond_destroy(&tq->cond_nonfull);
	pthread_mutex_destroy(&tq->mtx);

	free(tq->data);
	free(tq);

}

ThreadQueueItem thread_queue_insert(ThreadQueue* tq, ThreadQueueItem item) {
	int POOL_SIZE = tq->thread_queue_items;

	pthread_mutex_lock(&tq->mtx);

	while (tq->count >= POOL_SIZE) {
		pthread_cond_wait(&tq->cond_nonfull, &tq->mtx);
	}
	tq->end = (tq->end + 1) % POOL_SIZE;
	tq->data[tq->end] = item;
	tq->count++;

	pthread_cond_signal(&tq->cond_nonempty);
	pthread_mutex_unlock(&tq->mtx);
	
	return item;
}

ThreadQueueItem thread_queue_remove(ThreadQueue* tq) {
	int POOL_SIZE = tq->thread_queue_items;

	ThreadQueueItem item = 0;
	pthread_mutex_lock(&tq->mtx);
	while (tq->count <= 0) {
		pthread_cond_wait(&tq->cond_nonempty, &tq->mtx);
	}
	item = tq->data[tq->start];
	tq->start = (tq->start + 1) % POOL_SIZE;
	tq->count--;
	pthread_cond_signal(&tq->cond_nonfull);
	
	pthread_mutex_unlock(&tq->mtx);

	return item;
}


void thread_queue_increase_counter(ThreadQueue* tq) {
	pthread_mutex_lock(&tq->mtx);
	tq->items_processed++;
	pthread_cond_signal(&tq->cond_nonfull);
	pthread_mutex_unlock(&tq->mtx);
}