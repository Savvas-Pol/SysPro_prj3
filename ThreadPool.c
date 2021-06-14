#include <malloc.h>
#include <pthread.h>

#include "ThreadPool.h"
#include "record.h"
#include "help_functions.h"
#include "commands_vaccinemonitor.h"

static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t struct_mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_main(void * argp) {
	ThreadPoolArgp* ptargp = (ThreadPoolArgp*) argp;
	ThreadPoolArgp targp = *ptargp;
	char* line = NULL;
	size_t len = 0;

	//printf("In thread with ID: %lu starting .. \n", pthread_self());

	while (1) {
		ThreadQueueItem item = thread_queue_remove(targp.tq);

		//printf("In thread with ID: %lu consume :%p .. \n", pthread_self(), item);

		if (item == ThreadQueueEmptyItem) {
			break;
		}

		char* buffer5 = item;

		FILE* citizenRecordsFile = fopen(buffer5, "r");

		if (citizenRecordsFile != NULL) {
			int r;

			pthread_mutex_lock(&file_mutex);
			// # CS #
			hash_filenames_insert(targp.ht_filenames, buffer5);
			// # CS #
			pthread_mutex_unlock(&file_mutex);

			while ((r = getline(&line, &len, citizenRecordsFile)) != -1) { //read file line by line
				Record record;

				fill_record(line, &record); //create a temp record
				
				// # CS #
				pthread_mutex_lock(&struct_mutex);
				insert_citizen_record(targp.ht_viruses, targp.ht_citizens, targp.ht_countries, targp.bloomSize, record, 1); //flag=1 means from file
				pthread_mutex_unlock(&struct_mutex);
				// # CS #
				
				free_record(&record); //free temp record
			}

			fclose(citizenRecordsFile);
			free(buffer5);
		}
		
		thread_queue_increase_counter(targp.tq);
		
	}

	//printf("In thread with ID: %lu exiting .. \n", pthread_self());

	return NULL;

}

ThreadPool* thread_pool_create(int thread_pool_items, void* argp) {
	ThreadPool* tp = malloc(sizeof (ThreadPool));
	int i;

	tp->thread_pool_items = thread_pool_items;

	tp->data = (pthread_t*) malloc(thread_pool_items * sizeof (pthread_t)); //create hashtable for countries

	for (i = 0; i < thread_pool_items; i++) {
		pthread_create(&tp->data[i], 0, thread_main, argp);
	}


	return tp;
}

void thread_pool_destroy(ThreadPool* tp, ThreadQueue* tq) {
	int i;

	int thread_pool_items = tp->thread_pool_items;

	for (i = 0; i < thread_pool_items; i++) {
		thread_queue_insert(tq, ThreadQueueEmptyItem);
	}

	for (i = 0; i < thread_pool_items; i++) {
		pthread_join(tp->data[i], 0);
	}

	free(tp->data);
	free(tp);
}