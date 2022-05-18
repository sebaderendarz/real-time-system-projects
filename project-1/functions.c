//
// Created by sebastian on 19.03.2022.
//
#include "functions.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_FILENAME "2022-04-07.log"
#define SIZEOF_ENUM sizeof(log_level_t)

FILE* createFile();
void handleDumpSignal(int signum, siginfo_t *info,void* other);
void handleEnableSignal(int signum, siginfo_t *info,void* other);
void handleLogLevelSignal(int signum, siginfo_t *info,void* other);
void *runDumpFunctions(void* arg);
void *updateEnableDisableLogState(void *arg);

static log_level_t logLevel;
static sem_t dumpSem, enableSem;
static state_t state;
static FILE *logFile;
static FUN *funcsRegister;
static int funcsRegisterSize, numOfFuncs;
static int initialized = 0;
static pthread_t dumpThread, enableThread;
static pthread_mutex_t registerDumpFunctionMutex, writeToLogFileMutex;


int init() {
    if (initialized) return EXIT_FAILURE;

    numOfFuncs = 0;
    funcsRegisterSize = 5;
    logLevel = STANDARD;
    state = ENABLED;

    logFile = fopen(LOG_FILENAME, "a+");
    if (logFile == NULL) {
        return EXIT_FAILURE;
    }

    sigset_t set;
    struct sigaction action;
    sigfillset(&set);
    action.sa_sigaction = handleDumpSignal;
    action.sa_flags = SA_SIGINFO;
    action.sa_mask = set;
    int error = sigaction(DUMP_SIGNAL, &action, NULL);
    if (error != 0) {
        fclose(logFile);
        return EXIT_FAILURE;
    }

    sigfillset(&set);
    action.sa_sigaction = handleEnableSignal;
    action.sa_flags = SA_SIGINFO;
    action.sa_mask = set;
    error = sigaction(ENABLE_SIGNAL, &action, NULL);
    if (error != 0) {
        fclose(logFile);
        return EXIT_FAILURE;
    }

    sigfillset(&set);
    action.sa_sigaction = handleLogLevelSignal;
    action.sa_flags = SA_SIGINFO;
    action.sa_mask = set;
    error = sigaction(LEVEL_SIGNAL, &action, NULL);
    if (error != 0) {
        fclose(logFile);
        return EXIT_FAILURE;
    }

    error = sem_init(&dumpSem,0,0);
    if(error != 0){
        fclose(logFile);
        return EXIT_FAILURE;
    }

    error = sem_init(&enableSem,0,0);
    if(error != 0){
        fclose(logFile);
        sem_destroy(&dumpSem);
        return EXIT_FAILURE;
    }

    error = pthread_mutex_init(&registerDumpFunctionMutex, NULL);
    if (error != 0) {
        fclose(logFile);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        return EXIT_FAILURE;
    }

    error = pthread_mutex_init(&writeToLogFileMutex, NULL);
    if (error != 0) {
        fclose(logFile);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        return EXIT_FAILURE;
    }

    funcsRegister = (FUN *) calloc(sizeof(FUN), funcsRegisterSize);
    if (funcsRegister == NULL) {
        fclose(logFile);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        pthread_mutex_destroy(&writeToLogFileMutex);
        return EXIT_FAILURE;
    }

    error = pthread_create(&dumpThread, NULL, runDumpFunctions, NULL);
    if(error != 0) {
        fclose(logFile);
        free(funcsRegister);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&writeToLogFileMutex);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        return EXIT_FAILURE;
    }

    error = pthread_create(&enableThread, NULL, updateEnableDisableLogState,
                           NULL);
    if(error != 0) {
        fclose(logFile);
        free(funcsRegister);
        pthread_cancel(dumpThread);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&writeToLogFileMutex);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        return EXIT_FAILURE;
    }

    error = pthread_detach(dumpThread);
    if(error != 0) {
        free(funcsRegister);
        fclose(logFile);
        pthread_cancel(dumpThread);
        pthread_cancel(enableThread);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&writeToLogFileMutex);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        return EXIT_FAILURE;
    }

    error = pthread_detach(enableThread);
    if(error != 0) {
        free(funcsRegister);
        fclose(logFile);
        pthread_cancel(dumpThread);
        pthread_cancel(enableThread);
        sem_destroy(&dumpSem);
        sem_destroy(&enableSem);
        pthread_mutex_destroy(&writeToLogFileMutex);
        pthread_mutex_destroy(&registerDumpFunctionMutex);
        return EXIT_FAILURE;
    }

    initialized = 1;
    return EXIT_SUCCESS;
}

void destroy() {
    if (logFile != NULL) {
        fclose(logFile);
    }
    if (funcsRegister != NULL) {
        free(funcsRegister);
    }
    pthread_cancel(dumpThread);
    pthread_cancel(enableThread);
    sem_destroy(&dumpSem);
    sem_destroy(&enableSem);
    pthread_mutex_destroy(&registerDumpFunctionMutex);
    pthread_mutex_destroy(&writeToLogFileMutex);
}

void handleDumpSignal(int signum, siginfo_t *info, void *other) {
    sem_post(&dumpSem);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, DUMP_SIGNAL);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void *runDumpFunctions(void *arg) {
    FILE *file;
    while(1){
        sem_wait(&dumpSem);

        file = createFile();
        if (file != NULL) {
            for (int i = 0; i < numOfFuncs; i++) {
                funcsRegister[i](file);
            }
            fclose(file);
            file = NULL;
        }
    }
}

FILE *createFile() {
    char timeBuffer[30];
    char fileName[35];
    time_t rawTime = time(0);
    struct tm *timeInfo = localtime(&rawTime);
    strftime(timeBuffer, 30, DATE_FORMAT_FILE, timeInfo);
    sprintf(fileName, "%s.log", timeBuffer);
    return fopen(fileName, "a+");
}

int registerDumpFunction(FUN f) {
    if (initialized == 0) { return EXIT_FAILURE; }
    pthread_mutex_lock(&registerDumpFunctionMutex);
    if (numOfFuncs >= funcsRegisterSize) {
        FUN *functions = (FUN *) realloc(funcsRegister,
                                         sizeof(FUN) * funcsRegisterSize * 2);
        if (functions == NULL) { return EXIT_FAILURE; }
        funcsRegister = functions;
        funcsRegisterSize = funcsRegisterSize * 2;
    }
    funcsRegister[numOfFuncs++] = f;
    pthread_mutex_unlock(&registerDumpFunctionMutex);
    return EXIT_SUCCESS;
}

void handleEnableSignal(int signum, siginfo_t *info, void *other) {
    sem_post(&enableSem);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signum);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void *updateEnableDisableLogState(void *arg) {
    while(1){
        sem_wait(&enableSem);

        state = state == DISABLED ? ENABLED : DISABLED;
    }
}

void handleLogLevelSignal(int signum, siginfo_t *info, void *other) {
    memcpy(&logLevel, &info->si_value.sival_int, SIZEOF_ENUM);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signum);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}


void writeToLogFile(log_level_t level, char *string, ...) {
    if (initialized == 0 || state == DISABLED || logLevel > level) { return; }
    char timeBuffer[30];
    time_t rawTime = time(0);
    struct tm *timeInfo = localtime(&rawTime);
    strftime(timeBuffer, 30, DATE_FORMAT, timeInfo);

    pthread_mutex_lock(&writeToLogFileMutex);
    fprintf(logFile,
            "|| %s || message log level: %s || global log level: %s ||",
            timeBuffer,
            mapLogLevelToString(level), mapLogLevelToString(logLevel));
    va_list list;
    va_start(list, string);
    vfprintf(logFile, string, list);
    fprintf(logFile, " ||\n");
    fflush(logFile);
    va_end(list);
    pthread_mutex_unlock(&writeToLogFileMutex);
}

static char *mapLogLevelToString(log_level_t level) {
    if (level == MAX) return "MAX";
    else if (level == MIN) return "MIN";
    return "STANDARD";
}
