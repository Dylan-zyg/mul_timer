#ifndef __TIMER_H_INCLUDE__
#define __TIMER_H_INCLUDE__
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
typedef struct
{
        void *timer;
        int64_t id;
} NewTimer;
typedef int (*TimerHandler)(void *para, void *args, int cmd);

void *UtilTimerInit(int thread_num);

void UtilTimerClean();

int UtilTimerStart(NewTimer *newtimer, uint64_t ms, int cmd, void *args, size_t args_len,
                   TimerHandler, void *para, char repeat);

void UtilTimerStop(NewTimer *args);

int UtilTimerCheck(NewTimer args);

#endif // __TIMER_H_INCLUDE__
