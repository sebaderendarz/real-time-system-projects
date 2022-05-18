//
// Created by sebastian on 19.03.2022.
//
#ifndef PROJECT_1_FUNCTIONS_H
#define PROJECT_1_FUNCTIONS_H

#include <stdio.h>
#include <signal.h>

#define DATE_FORMAT "%Y:%m:%d-%H:%M:%S"
#define DATE_FORMAT_FILE "%Y.%m.%d-%H.%M.%S"
#define DUMP_SIGNAL SIGRTMIN
#define ENABLE_SIGNAL SIGRTMIN + 1
#define LEVEL_SIGNAL SIGRTMIN + 2

typedef enum {MIN,STANDARD,MAX} log_level_t;
typedef enum {DISABLED,ENABLED} state_t;
typedef void (*FUN)(FILE* f);

int init();
void destroy();
static char * mapLogLevelToString(log_level_t detail);
int registerDumpFunction(FUN fun);
void writeToLogFile(log_level_t d,char* string,...);

#endif //PROJECT_1_FUNCTIONS_H
