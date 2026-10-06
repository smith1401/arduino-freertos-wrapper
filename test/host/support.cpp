#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include <stdlib.h>

/* Memory for the idle and timer task when static allocation is enabled. */
#if (configSUPPORT_STATIC_ALLOCATION == 1)
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, configSTACK_DEPTH_TYPE *size)
{
    static StaticTask_t idleTcb;
    static StackType_t idleStack[configMINIMAL_STACK_SIZE];
    *tcb = &idleTcb;
    *stack = idleStack;
    *size = configMINIMAL_STACK_SIZE;
}

extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t **tcb, StackType_t **stack, configSTACK_DEPTH_TYPE *size)
{
    static StaticTask_t timerTcb;
    static StackType_t timerStack[configTIMER_TASK_STACK_DEPTH];
    *tcb = &timerTcb;
    *stack = timerStack;
    *size = configTIMER_TASK_STACK_DEPTH;
}
#endif
