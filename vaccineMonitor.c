#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/socket.h>      /* sockets */
#include <netinet/in.h>      /* internet sockets */
#include <netdb.h>
#include <pthread.h>          /* gethostbyaddr */

#include "help_functions.h"
#include "hashtable_virus.h"
#include "hashtable_citizen.h"
#include "hashtable_country.h"
#include "hashtable_filenames.h"
#include "BF.h"
#include "record.h"
#include "commands_vaccinemonitor.h"
#include "constants.h"
#include "thread_queue.h"
#include "ThreadPool.h"

int writelog = 0;

void catchinterrupt2(int signo) {
	printf("\nCatching: signo=%d\n", signo);
	printf("Catching: returning\n");

	writelog = 1;
}

int main_vaccine(int argc, char** argv) {

	/*  ---     DECLARATIONS    --- */

	int i, bloomSize = -1, bufferSize = -1, numThreads = -1, cyclicBufferSize = -1, j, port;
	int fd;
	char* token;
	char * saveptr = NULL;
	char* line = NULL;

	DIR* inputDirectory = NULL;
	struct dirent *direntp;

	static struct sigaction act;

	int totalAccepted = 0, totalRejected = 0;

	/*      ---------------     */

	srand(time(0));

	int total_countries = argc - 11; //11 are the standard arguments
	char*** out = read_arguments_for_vaccine_monitor(argc, argv, &bloomSize, &bufferSize, &numThreads, &port, &cyclicBufferSize);
	char** countries = out[0];
	char** countryPaths = out[1];

	// for(i = 0; i < argc-11; i++) {
	//     printf("Country %d: %s\n",i, countries[i]);
	// }
	act.sa_handler = catchinterrupt2;
	sigfillset(&(act.sa_mask));

	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	//sigaction(SIGCHLD, &act, NULL);

	struct sockaddr_in server;
	struct sockaddr *serverptr = (struct sockaddr*) &server;
	struct hostent *rem;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	if ((rem = gethostbyname("localhost")) == NULL) {
		herror("gethostbyname");
		exit(1);
	}

	server.sin_family = AF_INET; /* Internet domain */
	memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	server.sin_port = htons(port); /* Server port */

	if (connect(fd, serverptr, sizeof (server)) < 0) {
		perror("connect");
		exit(1);
	}

	//printf("Child: <%d>: waiting for bloom size and buffer size ... \n", id);
	//    receive_info(fd, &inputDirectoryPath, bufferSize);
	//printf("Child: <%d>: waiting for countries (bloom:%d, buffer:%d, dir:%s )... \n", id, bloomSize, bufferSize, inputDirectoryPath);

	HashtableVirus* ht_viruses = hash_virus_create(HASHTABLE_NODES); //create HashTable for viruses
	HashtableCitizen* ht_citizens = hash_citizen_create(HASHTABLE_NODES); //create HashTable for citizens
	HashtableCountry* ht_countries = hash_country_create(HASHTABLE_NODES); //create HashTable for countries
	HashtableFilenames* ht_filenames = hash_filenames_create(HASHTABLE_NODES); //create HashTable for filenames
	ThreadQueue * tq = thread_queue_create(cyclicBufferSize);

	ThreadPoolArgp targp;
	targp.bloomSize = bloomSize;
	targp.bufferSize = bufferSize;
	targp.numThreads = numThreads;
	targp.cyclicBufferSize = cyclicBufferSize;
	targp.ht_viruses = ht_viruses;
	targp.ht_citizens = ht_citizens;
	targp.ht_countries = ht_countries;
	targp.ht_filenames = ht_filenames;
	targp.tq = tq;

	void * argp = (void*) &targp;
	ThreadPool * tp = thread_pool_create(numThreads, argp);

	int files_to_be_processed = 0;

	for (i = 0; i < total_countries; i++) {
		char* buffer = countries[i];

		hash_country_insert(ht_countries, buffer);

		char* buffer4 = countryPaths[i];

		if (!(inputDirectory = opendir(buffer4))) {
			printf("Error in opening %s\n", buffer4);
		} else {
			while ((direntp = readdir(inputDirectory)) != NULL) {
				if (direntp->d_name[0] != '.') {

					char* buffer5 = malloc(strlen(buffer4) + 1 + strlen(direntp->d_name) + 1);
					strcpy(buffer5, buffer4);
					strcat(buffer5, "/");
					strcat(buffer5, direntp->d_name);

					thread_queue_insert(tq, buffer5);

					files_to_be_processed++;
				}
			}
		}

		//        free(buffer4);
	}

	pthread_mutex_lock(&tq->mtx);
	while (tq->items_processed < files_to_be_processed) {
		pthread_cond_wait(&tq->cond_nonfull, &tq->mtx);
	}
	tq->items_processed = 0;
	pthread_mutex_unlock(&tq->mtx);

	int tablelen;
	HashtableVirusNode** table = hash_virus_to_array(ht_viruses, &tablelen);
	for (j = 0; j < tablelen; j++) { //sending viruses and bloom filters to father
		char* virus = table[j]->virusName;

		//printf("Child <%d>:%d - Sending disease :%s to parent through pipe: %s via fd: %d \n", id, getpid(), virus, from_child_to_parent, writefd);

		char* info1 = (char*) virus;
		int info_length1 = strlen(virus) + 1;

		send_info(fd, info1, info_length1, bufferSize);

		char* info2 = table[j]->bloom->vector;
		int info_length2 = bloomSize;

		send_info(fd, info2, info_length2, bufferSize);
	}

	char buffer[2] = "#"; //sending finishing character to father
	char* info1 = (char*) buffer;
	int info_length1 = strlen(buffer) + 1;
	send_info(fd, info1, info_length1, bufferSize);

	//closedir(inputDirectory);

	while (1) {
		char* line = NULL;

		if (writelog == 1) {
			writelog = 0;
			// writelog
		}

		//            add_vaccination_records_for_child(inputDirectoryPath, ht_filenames, ht_citizens, ht_countries, ht_viruses, table, tablelen, bloomSize, from_child_to_parent, bufferSize, readfd, writefd);

		sigset_t set1;
		sigemptyset(&set1);

		sigprocmask(SIG_SETMASK, &set1, NULL); // allow everything here!

		// read from pipe instead of stdin
		receive_info(fd, &line, bufferSize);

		if (writelog == 1) {
			continue;
		}

		if (line == NULL) {
			continue;
		}

		token = strtok_r(line, " \n", &saveptr);

		sigaddset(&set1, SIGINT);
		sigaddset(&set1, SIGQUIT);
		sigaddset(&set1, SIGUSR1);

		sigprocmask(SIG_SETMASK, &set1, NULL); // disallow everything here!

		if (token != NULL) {

			if (!strcmp(token, "travelRequest")) {
				char* tokens[5];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //date
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[3] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[4] = strtok_r(NULL, " \n", &saveptr); //virusName

				travel_request_for_child(ht_viruses, ht_citizens, tokens[0], tokens[1], tokens[2], tokens[4], fd, fd, bufferSize);
			} else if (!strcmp(token, "searchVaccinationStatus")) {
				char* tokens[1];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID

				search_vaccination_status_for_child(ht_viruses, ht_countries, ht_citizens, bloomSize, bufferSize, fd, fd, tokens[0]);
			} else if (!strcmp(token, "addVaccinationRecords")) {
				char* tokens[1];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //countryName

				//                add_vaccination_records_new(ht_countries, ht_viruses, total_countries, countryPaths, bloomSize, bufferSize, fd, tokens[0]);

				int files_to_be_processed = 0;
				tq->items_processed = 0;

				for (i = 0; i < total_countries; i++) {
					char* buffer = countries[i];

					hash_country_insert(ht_countries, buffer);

					char* buffer4 = countryPaths[i];

					if (!(inputDirectory = opendir(buffer4))) {
						printf("Error in opening %s\n", buffer4);
					} else {
						while ((direntp = readdir(inputDirectory)) != NULL) {
							if (direntp->d_name[0] != '.') {

								char* buffer5 = malloc(strlen(buffer4) + 1 + strlen(direntp->d_name) + 1);
								strcpy(buffer5, buffer4);
								strcat(buffer5, "/");
								strcat(buffer5, direntp->d_name);

								thread_queue_insert(tq, buffer5);
								files_to_be_processed++;
							}
						}
					}
					free(buffer4);
				}
				pthread_mutex_lock(&tq->mtx);
				while (tq->items_processed < files_to_be_processed) {
					pthread_cond_wait(&tq->cond_nonfull, &tq->mtx);
				}
				tq->items_processed = 0;
				pthread_mutex_unlock(&tq->mtx);

				int tablelen;
				HashtableVirusNode** table = hash_virus_to_array(ht_viruses, &tablelen);
				for (j = 0; j < tablelen; j++) { //sending viruses and bloom filters to father
					char* virus = table[j]->virusName;

					//printf("Child <%d>:%d - Sending disease :%s to parent through pipe: %s via fd: %d \n", id, getpid(), virus, from_child_to_parent, writefd);

					char* info1 = (char*) virus;
					int info_length1 = strlen(virus) + 1;

					send_info(fd, info1, info_length1, bufferSize);

					char* info2 = table[j]->bloom->vector;
					int info_length2 = bloomSize;

					send_info(fd, info2, info_length2, bufferSize);
				}

				char buffer[2] = "#"; //sending finishing character to father
				char* info1 = (char*) buffer;
				int info_length1 = strlen(buffer) + 1;
				send_info(fd, info1, info_length1, bufferSize);

			} else if (!strcmp(token, "/vaccineStatusBloom")) {
				char* tokens[3];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL || tokens[1] == NULL || tokens[2] != NULL) {
					printf("syntax error\n");
				} else {
					vaccine_status_bloom(ht_viruses, tokens[0], tokens[1]);
				}
			} else if (!strcmp(token, "/vaccineStatus")) {
				char* tokens[3];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL) {
					printf("syntax error\n");
				} else if (tokens[0] != NULL && tokens[1] == NULL) {
					vaccine_status_id(ht_viruses, ht_citizens, tokens[0], bufferSize, fd, fd);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] == NULL) {
					vaccine_status_id_virus(ht_viruses, ht_citizens, tokens[0], tokens[1], NULL);
				} else { // more than 2
					printf("syntax error\n");
				}
			} else if (!strcmp(token, "/populationStatus")) {
				char* tokens[5];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //date1
				tokens[3] = strtok_r(NULL, " \n", &saveptr); //date2
				tokens[4] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL) {
					printf("syntax error\n");
				} else if (tokens[0] != NULL && tokens[1] == NULL) {
					population_status_virus(ht_viruses, ht_citizens, ht_countries, tokens[0]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] == NULL) {
					population_status_country(ht_viruses, ht_countries, tokens[0], tokens[1]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] == NULL) {
					population_status_virus_dates(ht_viruses, ht_countries, tokens[0], tokens[1], tokens[2]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] != NULL && tokens[4] == NULL) {
					population_status_country_dates(ht_viruses, ht_countries, tokens[0], tokens[1], tokens[2], tokens[3]);
				} else {
					printf("syntax error\n");
				}
			} else if (!strcmp(token, "/popStatusByAge")) {
				char* tokens[5];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //date1
				tokens[3] = strtok_r(NULL, " \n", &saveptr); //date2
				tokens[4] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL) {
					printf("syntax error\n");
				} else if (tokens[0] != NULL && tokens[1] == NULL) {
					pop_status_by_age_virus(ht_viruses, ht_countries, tokens[0]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] == NULL) {
					pop_status_by_age_country(ht_viruses, ht_countries, tokens[0], tokens[1]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] == NULL) {
					pop_status_by_age_virus_dates(ht_viruses, ht_countries, tokens[0], tokens[1], tokens[2]);
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] != NULL && tokens[4] == NULL) {
					pop_status_by_age_country_dates(ht_viruses, ht_countries, tokens[0], tokens[1], tokens[2], tokens[3]);
				} else {
					printf("syntax error\n");
				}
			} else if (!strcmp(token, "/insertCitizenRecord")) {
				char* tokens[9];
				Record record = {0};

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //firstName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //lastName
				tokens[3] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[4] = strtok_r(NULL, " \n", &saveptr); //age
				tokens[5] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[6] = strtok_r(NULL, " \n", &saveptr); //YES/NO
				tokens[7] = strtok_r(NULL, " \n", &saveptr); //date
				tokens[8] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL || tokens[8] != NULL) {
					printf("syntax error\n");
				} else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] != NULL && tokens[4] != NULL && tokens[5] != NULL && tokens[6] != NULL && tokens[7] != NULL) {
					//YES
					record.citizenID = malloc((strlen(tokens[0])) + 1);
					record.firstName = malloc((strlen(tokens[1])) + 1);
					record.lastName = malloc((strlen(tokens[2])) + 1);
					record.country = malloc((strlen(tokens[3])) + 1);
					record.virusName = malloc((strlen(tokens[5])) + 1);

					strcpy(record.citizenID, tokens[0]);
					strcpy(record.firstName, tokens[1]);
					strcpy(record.lastName, tokens[2]);
					strcpy(record.country, tokens[3]);
					record.age = atoi(tokens[4]);
					strcpy(record.virusName, tokens[5]);

					record.dateVaccinated = malloc(sizeof (Date));

					token = strtok_r(tokens[7], "-", &saveptr);

					j = 0;

					while (token != NULL) {
						if (j == 0)
							record.dateVaccinated->day = atoi(token);
						else if (j == 1)
							record.dateVaccinated->month = atoi(token);
						else if (j == 2)
							record.dateVaccinated->year = atoi(token);
						token = strtok_r(NULL, "-\n", &saveptr);
						j++;
					}

					insert_citizen_record(ht_viruses, ht_citizens, ht_countries, bloomSize, record, 0); //flag=0 means from file
					free_record(&record);
				} else { //NO
					record.citizenID = malloc((strlen(tokens[0])) + 1);
					record.firstName = malloc((strlen(tokens[1])) + 1);
					record.lastName = malloc((strlen(tokens[2])) + 1);
					record.country = malloc((strlen(tokens[3])) + 1);
					record.virusName = malloc((strlen(tokens[5])) + 1);

					strcpy(record.citizenID, tokens[0]);
					strcpy(record.firstName, tokens[1]);
					strcpy(record.lastName, tokens[2]);
					strcpy(record.country, tokens[3]);
					record.age = atoi(tokens[4]);
					strcpy(record.virusName, tokens[5]);
					record.dateVaccinated = NULL;

					insert_citizen_record(ht_viruses, ht_citizens, ht_countries, bloomSize, record, 0);

					free_record(&record);
				}
			} else if (!strcmp(token, "/vaccinateNow")) {
				char* tokens[7];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //citizenID
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //firstName
				tokens[2] = strtok_r(NULL, " \n", &saveptr); //lastName
				tokens[3] = strtok_r(NULL, " \n", &saveptr); //country
				tokens[4] = strtok_r(NULL, " \n", &saveptr); //age
				tokens[5] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[6] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL || tokens[1] == NULL || tokens[2] == NULL || tokens[3] == NULL || tokens[4] == NULL || tokens[5] == NULL || tokens[6] != NULL) {
					printf("syntax error\n");
				} else {
					vaccinate_now(ht_viruses, ht_citizens, ht_countries, bloomSize, tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
				}
			} else if (!strcmp(token, "/list-nonVaccinated-Persons")) {
				char* tokens[2];

				tokens[0] = strtok_r(NULL, " \n", &saveptr); //virusName
				tokens[1] = strtok_r(NULL, " \n", &saveptr); //NULL

				if (tokens[0] == NULL || tokens[1] != NULL) {
					printf("syntax error\n");
				} else {
					list_nonVaccinated_Persons(ht_viruses, tokens[0]);
				}
			} else if (!strcmp(token, "exit")) {
				break;
			}
		}
	}

	// logfile

	char logfile_name[100];
	sprintf(logfile_name, "log_file.%d", getpid());
	FILE* logfile = fopen(logfile_name, "w");
	if (logfile == NULL) {
		printf("Error! Could not open file\n");
		exit(-1);
	}
	HashtableCountryNode* temp;
	for (int i = 0; i < HASHTABLE_NODES; i++) {
		temp = ht_countries->nodes[i];
		while (temp != NULL) {
			fprintf(logfile, "%s\n", temp->countryName);
			temp = temp->next;
		}
	}
	fprintf(logfile, "TOTAL TRAVEL REQUESTS %d\n", totalAccepted + totalRejected);
	fprintf(logfile, "ACCEPTED %d\n", totalAccepted);
	fprintf(logfile, "REJECTED %d\n", totalRejected);
	fclose(logfile);

	// frees

	if (line != NULL) {
		free(line);
	}

	thread_pool_destroy(tp, tq);
	hash_virus_destroy(ht_viruses);
	hash_citizen_destroy(ht_citizens);
	hash_country_destroy(ht_countries);
	hash_filenames_destroy(ht_filenames);
	thread_queue_destroy(tq);

	//printf("Child: <%d>: Exiting ... \n", port);

	close(fd);
	return 0;
}