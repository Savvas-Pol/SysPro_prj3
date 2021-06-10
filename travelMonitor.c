#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>      /* sockets */
#include <netinet/in.h>      /* internet sockets */
#include <netdb.h>          /* gethostbyaddr */
#include "help_functions.h"
#include "hashtable_virus.h"
#include "hashtable_citizen.h"
#include "hashtable_country.h"
#include "hashtable_monitor.h"
#include "BF.h"
#include "record.h"
#include "commands_vaccinemonitor.h"
#include "constants.h"
#include "commands_travelmonitor.h"
#include "skiplist.h"

int quit = 0;

void catchinterrupt(int signo) {
    printf("\nCatching: signo=%d (SIGINT)\n", signo);
    printf("Catching: returning\n");

    quit = 1;
}

int main(int argc, char** argv) {
    /*  ---     DECLARATIONS    --- */
    int bloomSize, socketBufferSize, numMonitors, i, j, requestID = 0, numThreads = 0, cyclicBufferSize = 0;
    int totalAccepted = 0, totalRejected = 0;
    char* token;
    char* inputDirectoryPath = NULL;

    char* line = NULL;

    DIR* inputDirectory;
    struct dirent *direntp;

    FILE* logfile;

    static struct sigaction act;

    srand(time(0));

    int port = rand() % 30000 + 20000; // change to an arbitrary number

    /*      ---------------     */

    if ((inputDirectory = read_arguments_for_travel_monitor(argc, argv, &bloomSize, &socketBufferSize, &numMonitors, &inputDirectoryPath, &numThreads, &cyclicBufferSize)) == NULL) { //read arguments from command line
        return -1;
    } else {
        act.sa_handler = catchinterrupt;
        sigfillset(&(act.sa_mask));

        sigaction(SIGINT, &act, NULL);
        sigaction(SIGQUIT, &act, NULL);
    }

    HashtableVirus* ht_viruses = hash_virus_create(HASHTABLE_NODES); //create HashTable for viruses
    HashtableCountry* ht_countries = hash_country_create(HASHTABLE_NODES); //create HashTable for countries
    HashtableMonitor* ht_monitors = hash_monitor_create(HASHTABLE_NODES); //create HashTable for monitors

    while ((direntp = readdir(inputDirectory)) != NULL) {
        if (direntp->d_name[0] != '.') {
            HashtableCountryNode* country = hash_country_search(ht_countries, direntp->d_name);
            if (country == NULL) {
                hash_country_insert(ht_countries, direntp->d_name); //insert countries in parent's hashtable
            }
        }
    }
    closedir(inputDirectory);

    for (j = 0; j < numMonitors; j++) {
        char name[100];
        sprintf(name, "%d", j); //monitor names
        hash_monitor_insert(ht_monitors, name); //insert monitors in parent's hashtable
    }

    int welcoming_fd = create_welcoming_socket(ht_monitors, numMonitors, port);
    int tablelen;
    HashtableCountryNode** nodetable = hash_country_to_array(ht_countries, &tablelen); //convert hash table to array to sort countries
    char ** table = malloc(sizeof(char*)*tablelen);
    
    for(i=0;i<tablelen;i++) {                           //add paths to countries
        char* tempvalue = malloc(PATH_MAX);
        strcpy(tempvalue, inputDirectoryPath);
        strcat(tempvalue, "/");
        strcat(tempvalue, nodetable[i]->countryName);
        
        table[i] = tempvalue;
        //printf("%d. Country: %s\n",i+1,table[i]->countryName);
    }
    
    for (j = 0; j < numMonitors; j++) {
        char name[100];
        sprintf(name, "%d", j);

        HashtableMonitorNode* node = hash_monitor_search(ht_monitors, name);

        pid_t pid = fork();

        if (pid > 0) { //parent
            struct sockaddr_in client;
            socklen_t clientlen = sizeof (client);

            node->pid = pid;

            if ((node->fd = accept(welcoming_fd, (struct sockaddr *) &client, &clientlen)) < 0) {
                perror("accept");
                exit(j);
            }

//            char* info3 = inputDirectoryPath;
//            int info_length3 = strlen(inputDirectoryPath) + 1;
//
//            send_info(node->fd, info3, info_length3, info_length3);
        } else if (pid == 0) { //child
            argc = 11;
            argv = malloc(sizeof (char*)*12);
            argv[0] = "vaccineMonitor";
            argv[1] = "-p";
            argv[2] = malloc(11);
            sprintf(argv[2], "%d", port);
            argv[3] = "-t";
            argv[4] = malloc(11);
            sprintf(argv[4], "%d", numThreads);
            argv[5] = "-b";
            argv[6] = malloc(11);
            sprintf(argv[6], "%d", socketBufferSize);
            argv[7] = "-c";
            argv[8] = malloc(11);
            sprintf(argv[8], "%d", cyclicBufferSize);
            argv[9] = "-s";
            argv[10] = malloc(11);
            sprintf(argv[10], "%d", bloomSize);
            
            for(i = j; i < tablelen; i += numMonitors) {
                char** tmp = realloc(argv, sizeof (char*)*(argc+2));
                if(tmp == NULL) {
                    printf("tmp==NULL\n");
                    return -1;
                }
                argv = tmp;
                argv[argc] = malloc(PATH_MAX);
                strcpy(argv[argc], table[i]);
                
                argc++;
                
                //printf("path%d is %s\n",i,argv[argc]);
            }
            argv[argc] = NULL;

            // printf("argc = %d\n",argc);
            // for(i=0;i<argc;i++) {
            //     printf("argv[%d] for child <%d> is %s\n", i, j, argv[i]);
            // }
            // printf("-----------------------------------\n");

            //execvp("./vaccineMonitor", argv);

            int main_vaccine(int, char **argv);
            main_vaccine(argc, argv);

            exit(0);
        }
    }
    //only parent continues from now on

    send_countries_to_monitors(ht_monitors, nodetable, tablelen, numMonitors, socketBufferSize); //send countries round robin to monitors
//    send_finishing_character(ht_monitors, numMonitors, socketBufferSize); //send finishing character "#" to all monitors
    receive_bloom_filter(ht_monitors, ht_viruses, numMonitors, bloomSize, socketBufferSize);

    int vtablelen;
    HashtableVirusNode** vtable = hash_virus_to_array(ht_viruses, &vtablelen);

    while (quit != 1) { //commands from user
        size_t len = 0;

        sigset_t set1;
        sigemptyset(&set1);

        sigprocmask(SIG_SETMASK, &set1, NULL); // allow everything here!

        printf("\nGive command: ");
        if (getline(&line, &len, stdin) == 0 || quit == 1) {
            break;
        }

        sigaddset(&set1, SIGINT);
        sigaddset(&set1, SIGQUIT);
        sigaddset(&set1, SIGUSR1);
        sigaddset(&set1, SIGCHLD);

        sigprocmask(SIG_SETMASK, &set1, NULL); // disallow everything here!

        token = strtok(line, " \n");

        if (token != NULL) {
            if (!strcmp(token, "travelRequest")) {
                char* tokens[6];

                tokens[0] = strtok(NULL, " \n"); //citizenID
                tokens[1] = strtok(NULL, " \n"); //date
                tokens[2] = strtok(NULL, " \n"); //countryFrom
                tokens[3] = strtok(NULL, " \n"); //countryTo
                tokens[4] = strtok(NULL, " \n"); //virusName
                tokens[5] = strtok(NULL, " \n"); //NULL

                if (tokens[0] == NULL || tokens[1] == NULL || tokens[2] == NULL || tokens[3] == NULL || tokens[4] == NULL || tokens[5] != NULL) {
                    printf("syntax error\n");
                } else {
                    travel_request(ht_viruses, ht_countries, ht_monitors, bloomSize, socketBufferSize, tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], requestID, &totalAccepted, &totalRejected);
                    requestID++;
                }
            } else if (!strcmp(token, "travelStats")) {
                char* tokens[5];

                tokens[0] = strtok(NULL, " \n"); //virusName
                tokens[1] = strtok(NULL, " \n"); //date1
                tokens[2] = strtok(NULL, " \n"); //date2
                tokens[3] = strtok(NULL, " \n"); //country
                tokens[4] = strtok(NULL, " \n"); //NULL

                if (tokens[0] == NULL) {
                    printf("syntax error\n");
                } else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] == NULL) {
                    travel_stats(ht_viruses, ht_countries, ht_monitors, bloomSize, tokens[0], tokens[1], tokens[2]);
                } else if (tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL && tokens[3] != NULL && tokens[4] == NULL) {
                    travel_stats_country(ht_viruses, ht_countries, ht_monitors, bloomSize, tokens[0], tokens[1], tokens[2], tokens[3]);
                } else {
                    printf("syntax error\n");
                }
            } else if (!strcmp(token, "addVaccinationRecords")) {
                char* tokens[2];

                tokens[0] = strtok(NULL, " \n"); //country
                tokens[1] = strtok(NULL, " \n"); //NULL

                if (tokens[0] == NULL || tokens[1] != NULL) {
                    printf("syntax error\n");
                } else {
                    add_vaccination_records(ht_viruses, ht_countries, ht_monitors, bloomSize, socketBufferSize, tokens[0]);
                }
            } else if (!strcmp(token, "searchVaccinationStatus")) {
                char* tokens[2];

                tokens[0] = strtok(NULL, " \n"); //citizenID
                tokens[1] = strtok(NULL, " \n"); //NULL

                if (tokens[0] == NULL || tokens[1] != NULL) {
                    printf("syntax error\n");
                } else {
                    search_vaccination_status(ht_viruses, ht_countries, ht_monitors, bloomSize, socketBufferSize, numMonitors, tokens[0]);
                }
            } else if (!strcmp(token, "exit")) {
                exit_travelmonitor(ht_monitors, numMonitors, socketBufferSize);
                break;
            } else {
                printf("Invalid command!! Try again...\n");
            }
        }
    }

    char logfile_name[100];
    sprintf(logfile_name, "log_file.%d", getpid());
    logfile = fopen(logfile_name, "w");
    if (logfile == NULL) {
        printf("Error! Could not open file\n");
        exit(-1);
    }
    HashtableCountryNode* temp;
    for (i = 0; i < HASHTABLE_NODES; i++) {
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

    for (j = 0; j < numMonitors; j++) {
        HashtableMonitorNode* node = hash_monitor_search_with_int(ht_monitors, j);

        waitpid(node->pid, 0, 0);

        close(node->fd);
    }

    hash_virus_destroy(ht_viruses);
    hash_country_destroy(ht_countries);
    hash_monitor_destroy(ht_monitors);

    free(nodetable);
    free(vtable);

    close(welcoming_fd);

    return 0;
}