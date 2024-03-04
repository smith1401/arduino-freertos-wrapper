#ifndef __FRT_H__
#define __FRT_H__

#include <Arduino.h>
#include <assert.h>

#if defined(STM32F1) || defined(STM32F2) || defined(STM32F4) || defined(STM32U5)
#define STM32
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
#ifndef NRF52
#define NRF52
#endif
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

#ifndef FRT_UNUSED
#define FRT_UNUSED(expr)      \
        do                    \
        {                     \
                (void)(expr); \
        } while (0)
#endif

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
#elif defined(NRF52)
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() isInISR()
#endif
#endif

#if defined(ESP32)
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                                 \
        do                                                                                   \
        {                                                                                    \
                bool __from_isr = FRT_IS_ISR();                                              \
                bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
                if (__from_isr)                                                              \
                {                                                                            \
                        taskENTER_CRITICAL_ISR(&spinlock);                                   \
                }                                                                            \
                else if (__kernel_running)                                                   \
                {                                                                            \
                        taskENTER_CRITICAL(&spinlock);                                       \
                }                                                                            \
                else                                                                         \
                {                                                                            \
                        taskDISABLE_INTERRUPTS();                                            \
                }                                                                            \
        } while (0)
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                                                                  \
        do                                                                                   \
        {                                                                                    \
                bool __from_isr = FRT_IS_ISR();                                              \
                bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
                if (__from_isr)                                                              \
                {                                                                            \
                        taskEXIT_CRITICAL_ISR(&spinlock);                                    \
                }                                                                            \
                else if (__kernel_running)                                                   \
                {                                                                            \
                        taskEXIT_CRITICAL(&spinlock);                                        \
                }                                                                            \
                else                                                                         \
                {                                                                            \
                        taskENABLE_INTERRUPTS();                                             \
                }                                                                            \
        } while (0)
#endif
#elif defined(STM32) || defined(NRF52)
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER()                                                                 \
        do                                                                                   \
        {                                                                                    \
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
                }                                                                            \
        } while (0)
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT()                                                                  \
        do                                                                                   \
        {                                                                                    \
                uint32_t __isrm = 0;                                                         \
                bool __from_isr = FRT_IS_ISR();                                              \
                bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
                if (__from_isr)                                                              \
                {                                                                            \
                        taskEXIT_CRITICAL_FROM_ISR(__isrm);                                  \
                }                                                                            \
                else if (__kernel_running)                                                   \
                {                                                                            \
                        taskEXIT_CRITICAL();                                                 \
                }                                                                            \
                else                                                                         \
                {                                                                            \
                        interrupts();                                                        \
                }                                                                            \
        } while (0)
#endif
#endif

namespace frt
{
        namespace detail
        {
                inline void yieldFromIsr(BaseType_t &tasks_woken) __attribute__((always_inline));
                void yieldFromIsr(BaseType_t &tasks_woken)
                {
#ifdef ESP32
                        if (!tasks_woken)
                                return;
#if defined(portYIELD_FROM_ISR)
                        portYIELD_FROM_ISR();
#elif defined(portEND_SWITCHING_ISR)
                        portEND_SWITCHING_ISR();
#else
                        taskYIELD();
#endif
#else
#if defined(portYIELD_FROM_ISR)
                        portYIELD_FROM_ISR(tasks_woken);
#elif defined(portEND_SWITCHING_ISR)
                        portEND_SWITCHING_ISR(tasks_woken);
#else
                        taskYIELD();
#endif
#endif
                }
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

#endif // __FRT_H__