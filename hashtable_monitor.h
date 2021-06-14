#ifndef HASHTABLE_MONITOR_H
#define HASHTABLE_MONITOR_H

#include "skiplist.h"
#include "BF.h"
#include "citizen.h"
#include "hashtable_country.h"
#include "hashtable_virus.h"

typedef struct HashtableMonitorNode {
	char* monitorName;
	int fd;
	pid_t pid;
	struct HashtableMonitorNode* next; //pointer to next bucket node
} HashtableMonitorNode;

typedef struct HashtableMonitor {
	HashtableMonitorNode** nodes;
	int hash_nodes;
} HashtableMonitor;

HashtableMonitor* hash_monitor_create(int hashNodes);
void hash_monitor_destroy(HashtableMonitor* ht);
HashtableMonitorNode* hash_monitor_search(HashtableMonitor* ht, char* monitorName);
HashtableMonitorNode* hash_monitor_search_with_int(HashtableMonitor* ht, int monitorName);
HashtableMonitorNode* hash_monitor_search_pid(HashtableMonitor* ht, pid_t pid);
HashtableMonitorNode* hash_monitor_insert(HashtableMonitor* ht, char* monitorName);
void hash_monitor_delete(HashtableMonitor* ht, char* monitorName);

int create_welcoming_socket(HashtableMonitor* ht_monitors, int numMonitors, int *port);
void send_countries_to_monitors(HashtableMonitor* ht_monitors, HashtableCountryNode** table, int tablelen, int numMonitors, int bufferSize);
void send_finishing_character(HashtableMonitor* ht_monitors, int numMonitors, int bufferSize);
void receive_bloom_filter(HashtableMonitor* ht_monitors, HashtableVirus* ht_viruses, int numMonitors, int bloomSize, int bufferSize);

HashtableMonitorNode** hash_monitor_to_array(HashtableMonitor* ht, int* len);

#endif