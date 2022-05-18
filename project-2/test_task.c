#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<unistd.h>

#define DATE_FORMAT_FILE "%Y.%m.%d-%H.%M.%S"


FILE *createFile() {
    char timeBuffer[30];
    char fileName[50];
    time_t rawTime = time(0);
    struct tm *timeInfo = localtime(&rawTime);
    strftime(timeBuffer, 30, DATE_FORMAT_FILE, timeInfo);
    sprintf(fileName, "task_trigger_%s.log", timeBuffer);
    return fopen(fileName, "a+");
}


int main(int argc, char** argv){
    FILE* logFile = createFile();
    if (logFile == NULL) {
        return EXIT_FAILURE;
    }

    if (argc > 1){
        char argsList[2000] = "";
        for (int i = 1; argv[i] != NULL; i++) {
            strcat(argsList, argv[i]);
            strcat(argsList, " ");
        }
        argsList[strlen(argsList)] = 0;
        fprintf(logFile, "Test task process (pid: %d) triggered with %d arguments. Args list: %s\n",
                getpid(), argc - 1, argsList);
    }
    else {
        fprintf(logFile, "Test task process (pid: %d) triggered without arguments.\n", getpid());
    }

    fclose(logFile);
    return EXIT_SUCCESS;
}
