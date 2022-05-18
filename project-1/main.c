//
// Created by sebastian on 19.03.2022.
//
#include "functions.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void *checkIfEvenOrOddThread(void *arg);
void *randomNumberGeneratorThread(void *arg);
void checkIfNumberIsDivisibleBy3(FILE *f);
void checkIfNumberIsDivisibleBy5(FILE *f);
void checkIfNumberIsDivisibleBy7(FILE *f);

static unsigned int number = 1;


int main() {
    srand(time(NULL));
    printf("pid = %d, dump signal = %d, enable logging signal = %d, log level change signal = %d\n",
           getpid(), DUMP_SIGNAL, ENABLE_SIGNAL, LEVEL_SIGNAL);
    assert(init() == 0);
    registerDumpFunction(checkIfNumberIsDivisibleBy3);
    registerDumpFunction(checkIfNumberIsDivisibleBy5);
    registerDumpFunction(checkIfNumberIsDivisibleBy7);
    pthread_t checkThread, generatorThread;
    pthread_create(&checkThread, NULL, checkIfEvenOrOddThread, NULL);
    pthread_create(&generatorThread, NULL, randomNumberGeneratorThread, NULL);
    pthread_join(checkThread, NULL);
    pthread_join(generatorThread, NULL);
    destroy();
    return 0;
}


void *checkIfEvenOrOddThread(void *arg) {
    while (1) {
        writeToLogFile(MIN, "Started checking if number %u is EVEN or ODD.",
                       number);
        number % 2 == 0 ? writeToLogFile(STANDARD, "Number %u is EVEN.", number)
                        : writeToLogFile(STANDARD, "Number %u is ODD.", number);
        sleep(10);
    }
}

void *randomNumberGeneratorThread(void *arg) {
    while (1) {
        writeToLogFile(MIN, "Started looking for a new random number.");
        number = rand() % 1000000000; // 1 BLN
        writeToLogFile(MAX, "Generated a new random number: %u.", number);
        sleep(5);
    }
}

void checkIfNumberIsDivisibleBy3(FILE *f) {
    number % 3 == 0 ? fprintf(f, "Number %u is divisible by 3.\n", number)
                    : fprintf(f, "Number %u is not divisible by 3.\n", number);
}

void checkIfNumberIsDivisibleBy5(FILE *f) {
    number % 5 == 0 ? fprintf(f, "Number %u is divisible by 5.\n", number)
                    : fprintf(f, "Number %u is not divisible by 5.\n", number);
}

void checkIfNumberIsDivisibleBy7(FILE *f) {
    number % 7 == 0 ? fprintf(f, "Number %u is divisible by 7.\n", number)
                    : fprintf(f, "Number %u is not divisible by 7.\n", number);
}
