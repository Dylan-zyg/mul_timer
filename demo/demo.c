#include <stdio.h>
#include "../src/util_timer.h"
#define TIMER_PROC_THREAD_NUM (5)

int TimerHandler1(void *para, void *args, int cmd)
{
    printf("TimerHandler ---1--- runging\n");
}

int TimerHandler2(void *para, void *args, int cmd)
{
    printf("TimerHandler ---2--- runging\n");
}

int main(int argc, char *argv[])
{
    int i = 200;
    NewTimer timer1,timer2; 
    UtilTimerInit(TIMER_PROC_THREAD_NUM);
    UtilTimerStart(&timer1, 5*100, 0, NULL, 0,TimerHandler1, NULL, 1);
    UtilTimerStart(&timer2,3*100, 0, NULL, 0,TimerHandler2, NULL, 1);
    
    while(i--)
    {
        sleep(1);
        printf("sleep ---%d---\n",i);

    };

    UtilTimerClean();
    return 0;
}
