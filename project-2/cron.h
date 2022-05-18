//
// Created by sebastian on 16.04.2022.
//
#ifndef CRON_H
#define CRON_H

#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>


struct cron_t{
    struct list_t* head;
    int taskId;
    bool exit;
};

struct list_t{
    struct task_t* task;
    struct list_t* next;
};

struct task_t{
    timer_t timer;
    struct tm baseTime, repeatTime;
    bool baseTimeRelative;
    int id;
    char* command;
    char** arguments;
    int numOfArguments;
    atomic_bool active;
};


void runCron();
void sendCommandToCron(char* argv[]);


#endif //CRON_H
