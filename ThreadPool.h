#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <signal.h>

#include "hashtable_virus.h"
#include "hashtable_citizen.h"
#include "hashtable_country.h"
#include "hashtable_filenames.h"
#include "thread_queue.h"

typedef struct ThreadPool {
    pthread_t* data;
    int thread_pool_items;
} ThreadPool;

typedef struct ThreadPoolArgp {
    int bloomSize;
    int bufferSize;
    int numThreads;
    int cyclicBufferSize;
    HashtableVirus* ht_viruses;
    HashtableCitizen* ht_citizens;
    HashtableCountry* ht_countries;
    HashtableFilenames* ht_filenames;
    ThreadQueue* tq;
} ThreadPoolArgp;

ThreadPool* thread_pool_create(int thread_pool_items, void* argp);
void thread_pool_destroy(ThreadPool* tp, ThreadQueue* tq);




#endif

