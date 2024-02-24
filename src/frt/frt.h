#ifndef __FRT_H__
#define __FRT_H__

#include <Arduino.h>
#include <assert.h>

#if defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
#include <STM32FreeRTOS.h>
#include <message_buffer.h>
#include <queue.h>
#elif defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/message_buffer.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#elif defined(NRF52) || defined(NRF52840_XXAA)
#include <FreeRTOS.h>
#include <event_groups.h>
#include <queue.h>
#include <semphr.h>
#include <message_buffer.h>
#include <timers.h>
#include <task.h>
#else
#error "Platform not supported!"
#endif

#define FRT_TASK_NOTIFY_INDEX 0

#ifndef FRT_WARN_UNUSED
#define FRT_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#if defined(STM32)
#ifndef FRT_IS_IRQ_MASKED
#define FRT_IS_IRQ_MASKED() (__get_PRIMASK() != 0U)
#endif

#ifndef FRT_IS_IRQ_MODE
#define FRT_IS_IRQ_MODE() (__get_IPSR() != 0U)
#endif
#endif

#if defined(ESP32)
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() xPortInIsrContext()
#endif
#elif defined(STM32)
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() (FRT_IS_IRQ_MODE() || FRT_IS_IRQ_MASKED())
#endif
#elif defined(NRF52) || defined(NRF52840_XXAA)
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() isInISR()
#endif
#endif

#if defined(ESP32)
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                         \
        portMUX_TYPE mtx = portMUX_INITIALIZER_UNLOCKED;                             \
        bool __from_isr = FRT_IS_ISR();                                              \
        bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
        if (__from_isr)                                                              \
        {                                                                            \
                taskENTER_CRITICAL_ISR(&mtx);                                        \
        }                                                                            \
        else if (__kernel_running)                                                   \
        {                                                                            \
                taskENTER_CRITICAL(&mtx);                                            \
        }                                                                            \
        else                                                                         \
        {                                                                            \
                noInterrupts();                                                      \
        }
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                  \
        if (__from_isr)                      \
        {                                    \
                taskEXIT_CRITICAL_ISR(&mtx); \
        }                                    \
        else if (__kernel_running)           \
        {                                    \
                taskEXIT_CRITICAL(&mtx);     \
        }                                    \
        else                                 \
        {                                    \
                interrupts();                \
        }
#endif
#elif defined(STM32)
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
#elif defined(NRF52) || defined(NRF52840_XXAA)
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                         \
        bool __from_isr = FRT_IS_ISR();                                              \
        bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
        if (__from_isr)                                                              \
        {                                                                            \
                taskENTER_CRITICAL_FROM_ISR();                                       \
        }                                                                            \
        else if (__kernel_running)                                                   \
        {                                                                            \
                taskENTER_CRITICAL();                                                \
        }                                                                            \
        else                                                                         \
        {                                                                            \
                noInterrupts();                                                      \
        }
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                         \
        uint32_t __isrm = 0;                        \
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
                interrupts();                       \
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
#elif defined(NRF52) || defined(NRF52840_XXAA)
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
#endif
        }

        inline void spin() __attribute__((always_inline));
        void spin()
        {

#if defined(STM32)
                // Start the kernel scheduler
                assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
                vTaskStartScheduler();
#endif
        }
}

#include "log.h"
#include "mutex.h"
#include "queue.h"
#include "task.h"
#include "timer.h"
#include "msgs.h"
#include "manager.h"
#include "pubsub.h"
#include "node.h"
#include "messagebuffer.h"

#endif // __FRT_H__