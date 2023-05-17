#ifndef __FRT_H__
#define __FRT_H__

#include <Arduino.h>
#include <assert.h>

#if defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
#include <STM32FreeRTOS.h>
#include <message_buffer.h>
#elif defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/message_buffer.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#else
#error "Board not supported!"
#endif

#define FRT_TASK_NOTIFY_INDEX 0

#ifndef FRT_WARN_UNUSED
#define FRT_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#ifndef FRT_IS_IRQ_MASKED
#define FRT_IS_IRQ_MASKED() (__get_PRIMASK() != 0U)
#endif

#ifndef FRT_IS_IRQ_MODE
#define FRT_IS_IRQ_MODE() (__get_IPSR() != 0U)

#endif
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() (FRT_IS_IRQ_MODE() || FRT_IS_IRQ_MASKED())
#endif

#ifdef ESP32
        portMUX_TYPE mtx = portMUX_INITIALIZER_UNLOCKED;
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                         \
        uint32_t __isrm = 0;                                                         \
        bool __from_isr = FRT_IS_ISR();                                              \
        bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
        if (__from_isr)                                                              \
        {                                                                            \
                __isrm = taskENTER_CRITICAL_FROM_ISR(&mtx);                          \
        }                                                                            \
        else if (__kernel_running)                                                   \
        {                                                                            \
                taskENTER_CRITICAL(&mtx);                                            \
        }                                                                            \
        else                                                                         \
        {                                                                            \
                __disable_irq();                                                     \
        }
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                               \
        if (__from_isr)                                   \
        {                                                 \
                taskEXIT_CRITICAL_FROM_ISR(__isrm, &mtx); \
        }                                                 \
        else if (__kernel_running)                        \
        {                                                 \
                taskEXIT_CRITICAL(&mtx);                  \
        }                                                 \
        else                                              \
        {                                                 \
                __enable_irq();                           \
        }
#endif
#else
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                         \
        uint32_t __isrm = 0;                                                         \
        bool __from_isr = FRT_IS_ISR();                                              \
        bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
        if (__from_isr)                                                              \
        {                                                                            \
                __isrm = taskENTER_CRITICAL_FROM_ISR();                              \
        }                                                                            \
        else if (__kernel_running)                                                   \
        {                                                                            \
                taskENTER_CRITICAL();                                                \
        }                                                                            \
        else                                                                         \
        {                                                                            \
                __disable_irq();                                                     \
        }
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                         \
        if (__from_isr)                             \
        {                                           \
                taskEXIT_CRITICAL_FROM_ISR(__isrm); \
        }                                           \
        else if (__kernel_running)                  \
        {                                           \
                taskEXIT_CRITICAL();                \
        }                                           \
        else                                        \
        {                                           \
                __enable_irq();                     \
        }
#endif
#endif


namespace frt
{
        namespace detail
        {
#if defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
                inline void yieldFromIsr(BaseType_t &tasks_woken) __attribute__((always_inline));
                void yieldFromIsr(BaseType_t &tasks_woken)
                {
#if defined(portYIELD_FROM_ISR)
                        portYIELD_FROM_ISR(tasks_woken);
#elif defined(portEND_SWITCHING_ISR)
                        portEND_SWITCHING_ISR(tasks_woken);
#else
                        taskYIELD();
#endif
                }
#elif defined(ESP32)
                inline void yieldFromIsr() __attribute__((always_inline));
                void yieldFromIsr()
                {
#if defined(portYIELD_FROM_ISR)
                        portYIELD_FROM_ISR();
#elif defined(portEND_SWITCHING_ISR)
                        portEND_SWITCHING_ISR();
#else
                        taskYIELD();
#endif
                }
#endif
        }

        inline void spin() __attribute__((always_inline));
        void spin()
        {

#ifndef ESP32
                assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
#endif

                // Start the kernel scheduler
                vTaskStartScheduler();
        }
}

#include "mutex.h"
#include "queue.h"
#include "task.h"
#include "timer.h"
#include "msgs.h"
#include "manager.h"
#include "pubsub.h"
#include "node.h"
#include "messagebuffer.h"
#include "log.h"

#endif // __FRT_H__