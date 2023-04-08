#ifndef __FRT_H__
#define __FRT_H__

#include <Arduino.h>
#include <assert.h>

#if defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
#include <STM32FreeRTOS.h>
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

    inline void startScheduler() __attribute__((always_inline));
    void startScheduler()
    {

        assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

        // Start the kernel scheduler
        vTaskStartScheduler();
    }
}

#endif // __FRT_H__