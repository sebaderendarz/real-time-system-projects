//
// Created by sebastian on 16.04.2022.
//

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <spawn.h>
#include <ctype.h>
#include "cron.h"
#include "logger.h"


void handleCronTask(struct cron_t *server, int socketFileDescriptor);
char *handleIncomingCronCommand(struct cron_t *cron, char *command);
char *handleAddTaskCommand(struct cron_t *cron, char *arguments[], int numOfWordsInCmd);
char *handleHelpCommand(struct cron_t *cron);
char *handleListTasksCommand(struct cron_t *cron);
char *handleRemoveTaskCommand(struct cron_t *cron, char *arguments[]);
char *handleTerminateCronCommand(struct cron_t *cron);
int terminateCron(struct cron_t *cron);
int addTaskToTasksList(struct cron_t *cron, struct task_t *task);
int terminateAllTasks(struct cron_t *cron);
int cancelTaskByTaskId(struct cron_t *cron, int taskId);
int parseTimeStringToTmStruct(char *timeString, struct tm *destTime,bool repeatTime, bool relative);
int setupTaskTimer(struct cron_t* cron, struct task_t *task);
long getTimeInSeconds(struct tm time);
static void triggerTask(__sigval_t value);
void spawnProcess(struct task_t* task);


void runCron() {
    initLogger();

    struct cron_t cron = {NULL, 1, false};
    char defaultMessage[50] = "Cron start failed. Is Cron already running?\n";

    int socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor == -1) {
        writeToLogFile(STANDARD, "Creation of the server socket failed.");
        printf("%s", defaultMessage);
        destroyLogger();
        return;
    }

    const int option = 1;
    int result = setsockopt(socketFileDescriptor, SOL_SOCKET, SO_REUSEADDR,
                            (char *) &option, sizeof(option));
    if (result < 0) {
        writeToLogFile(STANDARD, "Setting options for the socket failed.");
        printf("%s", defaultMessage);
        destroyLogger();
        return;
    }

    struct sockaddr_in socketAddress = {0};
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(6000);

    result = bind(socketFileDescriptor, (struct sockaddr *) &socketAddress,
                  sizeof(socketAddress));
    if (result < 0) {
        writeToLogFile(STANDARD, "Address binding to the socket failed.");
        printf("%s", defaultMessage);
        destroyLogger();
        return;
    }

    result = listen(socketFileDescriptor, 5);
    if (result < 0) {
        writeToLogFile(STANDARD, "Marking the socket as passive failed.");
        printf("%s", defaultMessage);
        destroyLogger();
        return;
    }

    writeToLogFile(MAX, "Starting Cron...");
    while (1) {
        handleCronTask(&cron, socketFileDescriptor);
        if (cron.exit) {
            break;
        }
    }

    writeToLogFile(MAX, "Terminating Cron...");
    close(socketFileDescriptor);
    destroyLogger();
}


void sendCommandToCron(char *argv[]) {
    initLogger();
    writeToLogFile(STANDARD, "Started handling Cron command.");
    char defaultMessage[30] = "Command handling failed.\n";

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        writeToLogFile(STANDARD, "Creation of the client socket failed.");
        printf("%s", defaultMessage);
        destroyLogger();
        return;
    }

    struct sockaddr_in addressDescription;
    addressDescription.sin_family = AF_INET;
    addressDescription.sin_port = htons(6000);
    addressDescription.sin_addr.s_addr = inet_addr("127.0.0.1");
    int result = connect(clientSocket, (struct sockaddr *) &addressDescription,
                         sizeof(addressDescription));
    if (result == -1) {
        writeToLogFile(STANDARD, "Connecting to the socket 127.0.0.1:6000 "
                                 "failed.");
        printf("%s", defaultMessage);
        close(clientSocket);
        destroyLogger();
        return;
    }

    char message[2000] = "";
    for (int i = 0; argv[i] != NULL; i++) {
        strcat(message, argv[i]);
        strcat(message, " ");
    }
    message[strlen(message) - 1] = 0;
    writeToLogFile(STANDARD, "Cron command being handled: %s", message);
    result = send(clientSocket, message, strlen(message), 0);
    if (result == -1) {
        writeToLogFile(STANDARD, "Sending a message to the socket 127.0.0.1:6000 failed.");
        printf("%s", defaultMessage);
        close(clientSocket);
        destroyLogger();
        return;
    }

    char response[2001];
    result = recv(clientSocket, response, 2000, 0);
    if (result == -1) {
        writeToLogFile(STANDARD, "Failed when taking a message from the server.");
        printf("%s", defaultMessage);
        close(clientSocket);
        destroyLogger();
        return;
    }
    response[result] = 0;
    printf("%s", response);
    writeToLogFile(STANDARD, "Handled Cron command: %s", message);

    close(clientSocket);
    destroyLogger();
}


void handleCronTask(struct cron_t *server, int socketFileDescriptor) {
    struct sockaddr_in addressDescription;
    socklen_t socketAddressLength = sizeof(addressDescription);

    int acceptedSocketFileDescriptor = accept(socketFileDescriptor, (struct sockaddr *)
            &addressDescription, &socketAddressLength);
    if (acceptedSocketFileDescriptor < 0) {
        writeToLogFile(STANDARD, "Failed when opening a new socket for the "
                                 "communication with the client.");
        return;
    }

    char message[2000];
    int result = recv(acceptedSocketFileDescriptor, message, 2000, 0);
    if (result < 0) {
        writeToLogFile(STANDARD, "Failed when taking a message from the client.");
        close(acceptedSocketFileDescriptor);
        return;
    }
    message[result] = 0;

    char *responseForTheClient = handleIncomingCronCommand(server, message);
    if (responseForTheClient != NULL) {
        result = send(acceptedSocketFileDescriptor, responseForTheClient,
                      strlen(responseForTheClient), 0);
        if (result < 0) {
            writeToLogFile(STANDARD, "Sending response to the client failed.");
        }
    } else {
        writeToLogFile(STANDARD, "Command handling failed. Incorrect command "
                            "passed by the client or internal error.");
        send(acceptedSocketFileDescriptor,
             "Command handling failed. Have you passed a correct command?\n", strlen
                     ("Handling command failed. Have you passed a correct command?\n"),
             0);
    }
    free(responseForTheClient);
    close(acceptedSocketFileDescriptor);
}


char *handleIncomingCronCommand(struct cron_t *cron, char *command) {
    int numOfWordsInCmd = 0;
    for (int i = 0; i < strlen(command); i++) {
        if (*(command + i) == ' ') numOfWordsInCmd++;
    }

    if (numOfWordsInCmd == 0) {
        if (strncmp(command, "exit", strlen(command)) == 0) {
            return handleTerminateCronCommand(cron);
        } else if (strncmp(command, "help", strlen(command)) == 0) {
            return handleHelpCommand(cron);
        } else if (strncmp(command, "list", strlen(command)) == 0) {
            return handleListTasksCommand(cron);
        }
    } else {
        char *nextWordInCommand = strtok(command, " ");
        char commandName[200];
        strcpy(commandName, nextWordInCommand);
        char **listOfArgs = (char **) calloc(sizeof(char *), numOfWordsInCmd);
        if (listOfArgs == NULL) return NULL;
        for (int i = 0; i < numOfWordsInCmd; i++) {
            listOfArgs[i] = (char *) calloc(sizeof(char), 200);
            if (listOfArgs[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(listOfArgs[j]);
                }
                free(listOfArgs);
                return NULL;
            }
        }
        nextWordInCommand = strtok(NULL, " ");
        int i = 0;
        while (nextWordInCommand != NULL) {
            strcpy(listOfArgs[i++], nextWordInCommand);
            nextWordInCommand = strtok(NULL, " ");
        }

        char *responseMessage = NULL;
        if (strcmp(commandName, "add") == 0) {
            responseMessage = handleAddTaskCommand(cron, listOfArgs,
                                                   numOfWordsInCmd);
        } else if (strcmp(commandName, "rm") == 0) {
            responseMessage = handleRemoveTaskCommand(cron, listOfArgs);
        }
        for (int j = 0; j < i; j++) {
            free(listOfArgs[j]);
        }
        free(listOfArgs);
        return responseMessage;
    }
    return NULL;
}


char *handleAddTaskCommand(struct cron_t *cron, char *arguments[], int numOfWordsInCmd) {
    writeToLogFile(MIN, "Started handling ADD TASK command.");
    char *message = (char *) calloc(sizeof(char), 100);
    if (message == NULL) {
        return NULL;
    }

    char defaultMessage[60] = "Incorrect command passed to schedule a new task.\n";
    int index = 0;
    bool relative = false;
    if (strcmp(arguments[index], "-r") == 0) {
        relative = true;
        index++;
    }
    if (arguments[index] == NULL) {
        strcpy(message, defaultMessage);
        return message;
    }

    struct task_t *task = (struct task_t *) calloc(sizeof(struct task_t), 1);
    if (task == NULL) {
        free(message);
        return NULL;
    }
    time_t currentTime = time(NULL);
    struct tm* currentTimeLocale = localtime(&currentTime);
    memset(&task->baseTime, 0, sizeof(struct tm));
    memset(&task->repeatTime, 0, sizeof(struct tm));
    task->baseTime.tm_isdst = currentTimeLocale->tm_isdst;

    int result = parseTimeStringToTmStruct(arguments[index++], &task->baseTime, false,
                                           relative);
    if (result != 0) {
        free(task);
        strcpy(message, defaultMessage);
        return message;
    }
    if (arguments[index] == NULL) {
        free(task);
        strcpy(message, defaultMessage);
        return message;
    }

    if (strcmp(arguments[index], "-c") == 0) {
        if (relative == false || arguments[++index] == NULL) {
            free(task);
            strcpy(message, defaultMessage);
            return message;
        }
        result = parseTimeStringToTmStruct(arguments[index++], &task->repeatTime, true, relative);
        if (result != 0) {
            free(task);
            strcpy(message, defaultMessage);
            return message;
        }
    }

    if (arguments[index] == NULL){
        free(task);
        strcpy(message, defaultMessage);
        return message;
    }
    task->id = cron->taskId++;
    task->active = true;
    task->baseTimeRelative = relative;
    task->command = (char *) calloc(sizeof(char), 200);
    strcpy(task->command, arguments[index++]);
    if (task->command == NULL) {
        free(task);
        free(message);
        return NULL;
    }

    task->numOfArguments = numOfWordsInCmd - index + 1;
    task->arguments = (char **) calloc(sizeof(char *), task->numOfArguments + 1);
    if (task->arguments == NULL) {
        free(task->command);
        free(task);
        free(message);
        return NULL;
    }
    task->arguments[task->numOfArguments] = NULL;
    for (int i = 0; i < task->numOfArguments; i++) {
        task->arguments[i] = (char *) calloc(sizeof(char), 200);
        if (task->arguments[i] == NULL) {
            free(task->command);
            for (int j = 0; j < i; j++) {
                free(task->arguments[i]);
            }
            free(task->arguments);
            free(task);
            free(message);
            return NULL;
        }
        if (i == 0){
            strcpy(task->arguments[i], task->command);
        }
        else{
            strcpy(task->arguments[i], arguments[index++]);
        }
    }

    result = setupTaskTimer(cron, task);
    if (result != 0) {
        free(task->command);
        for (int i = 0; i < task->numOfArguments; i++) {
            free(task->arguments[i]);
        }
        free(task->arguments);
        free(task);
        free(message);
        return NULL;
    }

    result = addTaskToTasksList(cron, task);
    if (result != 0) {
        timer_delete(task->timer);
        free(task->command);
        for (int i = 0; i < task->numOfArguments; i++) {
            free(task->arguments[i]);
        }
        free(task->arguments);
        free(task);
        free(message);
        return NULL;
    }

    strcpy(message, "Task scheduled.\n");
    return message;
}


int parseTimeStringToTmStruct(char *timeString, struct tm *destTime,bool repeatTime, bool relative) {
    char tab[6][20];
    char* timeNumber = strtok(timeString,":");
    int i = 0;
    while(timeNumber != NULL){
        for(int j = 0; j < strlen(timeNumber); j++){
            if(isdigit(timeNumber[j]) == 0){
                return -1;
            }
        }
        strcpy(tab[i++],timeNumber);
        timeNumber = strtok(NULL, ":");
    }

    if(!repeatTime){
        destTime->tm_year = tab[0] != NULL ? (int) strtol(tab[0], NULL, 10) : 0;
        destTime->tm_mon = tab[1] != NULL ? (int) strtol(tab[1], NULL, 10) : 0;
        destTime->tm_mday = tab[2] != NULL ? (int) strtol(tab[2], NULL, 10) : 0;
        destTime->tm_hour = tab[3] != NULL ? (int) strtol(tab[3], NULL, 10) : 0;
        destTime->tm_min = tab[4] != NULL ? (int) strtol(tab[4], NULL, 10) : 0;
        destTime->tm_sec = tab[5] != NULL ? (int) strtol(tab[5], NULL, 10) : 0;
        if (!relative){
            destTime->tm_year -= 1900;
            destTime->tm_mon -= 1;
        }
    }
    else{
        destTime->tm_sec = tab[0] != NULL ? (int) strtol(tab[0], NULL, 10) : 0;
        destTime->tm_min = tab[1] != NULL ? (int) strtol(tab[1], NULL, 10) : 0;
        destTime->tm_hour = tab[2] != NULL ? (int) strtol(tab[2], NULL, 10) : 0;
        destTime->tm_mday = tab[3] != NULL ? (int) strtol(tab[3], NULL, 10) : 0;
        destTime->tm_mon = tab[4] != NULL ? (int) strtol(tab[4], NULL, 10) : 0;
        destTime->tm_year = tab[5] != NULL ? (int) strtol(tab[5], NULL, 10) : 0;
    }
    return 0;
}


int setupTaskTimer(struct cron_t* cron, struct task_t *task) {
    struct sigevent event = {0,0,0,0};
    event.sigev_notify = SIGEV_THREAD;
    event.sigev_value.sival_ptr = task;
    event.sigev_notify_function = triggerTask;
    int result = timer_create(CLOCK_REALTIME,&event,&task->timer);
    if (result != 0) {
        return -1;
    }

    long seconds;
    if (task->baseTimeRelative){
        seconds = getTimeInSeconds(task->baseTime);
        if (seconds < 0) {
            return -1;
        }
    }
    else {
        seconds = mktime(&task->baseTime);
        long currentSecond = time(NULL);
        if (currentSecond > seconds){
            return -1;
        }
    }

    struct itimerspec timerSpec;
    timerSpec.it_value.tv_sec = seconds;
    timerSpec.it_interval.tv_sec = getTimeInSeconds(task->repeatTime);
    timerSpec.it_interval.tv_nsec = 0;
    timerSpec.it_value.tv_nsec = 0;
    result = timer_settime(task->timer, task->baseTimeRelative ? 0 : TIMER_ABSTIME,
                           &timerSpec, NULL);
    if (result != 0){
        return -1;
    }
    return 0;
}


long getTimeInSeconds(struct tm time){
    return time.tm_sec + time.tm_min*60 + time.tm_hour*3600 + time.tm_mday*3600*24 + time.tm_mon*3600*24*30 + time.tm_year*3600*24*30*365;
}


static void triggerTask(__sigval_t value){
    struct task_t* task = (struct task_t*) value.sival_ptr;
    if (task->active) {
        spawnProcess(task);
        if (getTimeInSeconds(task->repeatTime) == 0) {
            task->active = false;
        }
    }
}


void spawnProcess(struct task_t* task){
    pid_t pid;
    posix_spawn(&pid, task->command, 0, 0, task->arguments, 0);
}


char *handleRemoveTaskCommand(struct cron_t *cron, char *arguments[]) {
    writeToLogFile(MIN, "Started handling REMOVE TASK command.");
    char *message = (char *) calloc(sizeof(char), 100);
    if (message == NULL) {
        return NULL;
    }

    int count = 0;
    for (int i = 0; arguments[i] != NULL; i++) count++;
    if (count != 1) {
        strcpy(message, "Incorrect number of arguments passed in order to "
                        "cancel the task.\n");
        return message;
    }
    int taskId = atoi(arguments[0]);
    if (taskId < 1){
        strcpy(message, "Incorrect task id passed. It must be positive integer.\n");
    }
    else{
        int result = cancelTaskByTaskId(cron, taskId);
        if (result != 0) {
            sprintf(message, "Task with id %d does not exist.\n", taskId);
        } else {
            sprintf(message, "Task with id %d was cancelled.\n", taskId);
        }
    }
    return message;
}


char *handleHelpCommand(struct cron_t *cron) {
    writeToLogFile(MIN, "Started handling HELP command.");
    char *message = (char *) calloc(sizeof(char), 400);
    if (message == NULL) return NULL;
    strcpy(message, "./cron help -> display help message\n"
                    "./cron list -> display list of scheduled tasks\n"
                    "./cron exit -> terminate all tasks and kill Cron\n"
                    "./cron add [-r] <y:m:d:h:m:s> [-c] <s:m:h:d:m:y> path/to/run"
                    " <path-to-run-args> -> schedule new task\n"
                    "./cron rm <task-id> -> remove Cron task\n");
    return message;
}


char *handleListTasksCommand(struct cron_t *cron) {
    writeToLogFile(MIN, "Started handling LIST TASKS command.");
    int allocatedMessageMemory = 1000;
    char *message = (char *) calloc(sizeof(char), allocatedMessageMemory);
    if (message == NULL) return NULL;

    if (cron->head != NULL) {
        struct list_t *elem = cron->head;
        while (elem != NULL) {
            if (elem->task->active) {
                if (strlen(message) + 500 >= allocatedMessageMemory) {
                    allocatedMessageMemory = (allocatedMessageMemory + 500) * 2;
                    char *reallocatedMessage = (char *) calloc(sizeof(char),
                                                               allocatedMessageMemory);
                    if (reallocatedMessage == NULL) {
                        free(message);
                        return NULL;
                    }
                    strcpy(reallocatedMessage, message);
                    free(message);
                    message = reallocatedMessage;
                }
                char tempTaskDescriptionString[500];
                sprintf(tempTaskDescriptionString,
                        "ID: %d, %04d:%02d:%02d:%02d:%02d:%02d %s\n",
                        elem->task->id, elem->task->baseTimeRelative ?
                        elem->task->baseTime.tm_year : elem->task->baseTime.tm_year + 1900,
                        elem->task->baseTimeRelative ? elem->task->baseTime.tm_mon :
                        elem->task->baseTime.tm_mon + 1,
                        elem->task->baseTime.tm_mday, elem->task->baseTime.tm_hour,
                        elem->task->baseTime.tm_min, elem->task->baseTime.tm_sec,
                        elem->task->command);
                strcat(message, tempTaskDescriptionString);
            }
            elem = elem->next;
        }
    }
    if (strlen(message) == 0) {
        strcpy(message, "No tasks scheduled.\n");
    }
    return message;
}


char *handleTerminateCronCommand(struct cron_t *cron) {
    writeToLogFile(MIN, "Started handling TERMINATE CRON command.");
    char *message = (char *) calloc(sizeof(char), 100);
    if (message == NULL) {
        return NULL;
    }
    int numOfCancelledTasks = terminateCron(cron);
    sprintf(message, "Cron terminated. %d tasks cancelled.\n",
            numOfCancelledTasks);
    return message;
}

int terminateCron(struct cron_t *cron) {
    if (cron->head == NULL) {
        cron->exit = true;
        return 0;
    }
    int numOfCancelledTasks = terminateAllTasks(cron);
    cron->exit = true;
    return numOfCancelledTasks;
}


int terminateAllTasks(struct cron_t *cron) {
    if (cron == NULL || cron->head == NULL) return 0;
    int count = 0;
    struct list_t *next;
    do {
        next = cron->head->next;
        if (cron->head->task->active) count++;
        timer_delete(cron->head->task->timer);
        free(cron->head->task->command);
        for (int i = 0; i < cron->head->task->numOfArguments; i++) {
            free(cron->head->task->arguments[i]);
        }
        free(cron->head->task->arguments);
        free(cron->head->task);
        free(cron->head);
        cron->head = next;
    } while (cron->head != NULL);
    return count;
}


int cancelTaskByTaskId(struct cron_t *cron, int taskId) {
    if (cron == NULL || cron->head == NULL) return -1;

    if (cron->head->task->id == taskId && cron->head->task->active) {
         cron->head->task->active = false;
         return 0;
    } else {
        struct list_t *current = cron->head;
        while (current->next != NULL) {
            if (current->next->task->id == taskId && current->next->task->active) {
                current->next->task->active = false;
                return 0;
            }
            current = current->next;
        }
    }
    return -1;
}


int addTaskToTasksList(struct cron_t *cron, struct task_t *task) {
    struct list_t *newElem = (struct list_t *) calloc(sizeof(struct list_t), 1);
    if (newElem == NULL) {
        return -1;
    }
    newElem->task = task;
    if (cron->head == NULL) {
        cron->head = newElem;
        cron->head->next = NULL;
    } else {
        newElem->next = cron->head;
        cron->head = newElem;
    }
    return 0;
}
