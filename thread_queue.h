#ifndef THREAD_QUEUE_H
#define THREAD_QUEUE_H

#define ThreadQueueEmptyItem 0

typedef char* ThreadQueueItem;

typedef struct ThreadQueue {
	ThreadQueueItem* data;
	int start;
	int end;
	int count;
	int thread_queue_items;
	pthread_mutex_t mtx;
	pthread_cond_t cond_nonempty;
	pthread_cond_t cond_nonfull;
	
	int items_processed;
} ThreadQueue;

ThreadQueue* thread_queue_create(int thread_queue_items);
void thread_queue_destroy(ThreadQueue* tq);

ThreadQueueItem thread_queue_insert(ThreadQueue* tq, ThreadQueueItem item);

ThreadQueueItem thread_queue_remove(ThreadQueue* tq);

void thread_queue_increase_counter(ThreadQueue* tq);


#endif

