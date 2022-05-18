#include <stdlib.h>
#include <signal.h>
#define LEVEL_SIGNAL SIGRTMIN + 2


int main(int argc, char**argv)
// Function takes 2 parameters: 1 - pid, 2, log level in 0-2 range,
// both inclusive.
{
    union sigval val = { .sival_int = atoi(argv[2]) };
    sigqueue((pid_t)atoi(argv[1]), LEVEL_SIGNAL, val);
}
