//
// Created by Sebastian Derendarz on 09.04.2022.
//

#include "cron.h"


int main(int argc, char* argv[]) {
    if(argc == 1){
        runCron();
    }
    else{
        argv = argv + 1;
        sendCommandToCron(argv);
    }
    return 0;
}
