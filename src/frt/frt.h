#ifndef __FRT_H__
#define __FRT_H__

#include <Arduino.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>

#if defined(FRT_HOST)
// Host build (FreeRTOS POSIX port), used for the unit tests in test/host
#include <FreeRTOS.h>
#include <event_groups.h>
#include <queue.h>
#include <semphr.h>
#include <message_buffer.h>
#include <timers.h>
#include <task.h>
// STM32F4 etc. come from the CMSIS headers, STM32F4xx etc. from the compiler
// flags. The latter are needed for PlatformIO's dependency scan (deep+),
// which evaluates these conditions without reading the core headers.
#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F4) || defined(STM32U5) || \
    defined(STM32F1xx) || defined(STM32F2xx) || defined(STM32F4xx) || defined(STM32U5xx)
#ifndef STM32
#define STM32
#endif
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
// The FreeRTOS headers of the Adafruit core have the same names as the ones
// of the STM32FreeRTOS library. Included through a function-like macro,
// PlatformIO's dependency scan can not resolve them and therefore never
// pulls STM32FreeRTOS into an nRF52 build, while the compiler still finds
// the core's headers.
#define FRT_SYS_HEADER(name) <name>
#include FRT_SYS_HEADER(FreeRTOS.h)
#include FRT_SYS_HEADER(event_groups.h)
#include FRT_SYS_HEADER(queue.h)
#include FRT_SYS_HEADER(semphr.h)
#include FRT_SYS_HEADER(message_buffer.h)
#include FRT_SYS_HEADER(timers.h)
#include FRT_SYS_HEADER(task.h)
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
#elif defined(FRT_HOST)
#ifndef FRT_IS_ISR
#define FRT_IS_ISR() (false)
#endif
#endif

namespace frt
{
        /**
         *  Timeout value (in milliseconds) meaning "block forever".
         *  Every API taking a timeout in milliseconds maps this to portMAX_DELAY.
         */
        static const unsigned int FOREVER = UINT_MAX;

        namespace detail
        {
                /**
                 *  Convert a timeout in milliseconds to ticks.
                 *  - 0 stays 0 (do not block)
                 *  - FOREVER becomes portMAX_DELAY
                 *  - everything else is rounded up to at least one tick, so a
                 *    short timeout never silently turns into a non-blocking poll
                 *    when the tick period is longer than the requested time.
                 */
                inline TickType_t msToTicks(unsigned int msecs)
                {
                        if (msecs == 0)
                                return 0;

                        if (msecs == FOREVER)
                                return portMAX_DELAY;

                        const uint64_t ticks = (static_cast<uint64_t>(msecs) * configTICK_RATE_HZ + 999U) / 1000U;

                        if (ticks >= static_cast<uint64_t>(portMAX_DELAY))
                                return portMAX_DELAY - 1;

                        return static_cast<TickType_t>(ticks);
                }

                /**
                 *  Used by the overloads taking a remainder: adds the remainder
                 *  of the previous call to msecs, converts the sum to whole ticks
                 *  (rounding down) and stores the milliseconds that did not fit
                 *  in remainder, to be carried into the next call. Works for
                 *  tick rates above 1 kHz as well (where portTICK_PERIOD_MS is 0).
                 */
                inline TickType_t msToTicksCarry(unsigned int msecs, unsigned int &remainder)
                {
                        // Saturating add, FOREVER stays FOREVER
                        msecs = (msecs > FOREVER - remainder) ? FOREVER : msecs + remainder;
                        remainder = 0;

                        if (msecs == FOREVER)
                                return portMAX_DELAY;

                        const uint64_t ticks = static_cast<uint64_t>(msecs) * configTICK_RATE_HZ / 1000U;

                        // Less than one tick: wait one tick, nothing left over
                        if (ticks == 0)
                                return msecs > 0 ? 1 : 0;

                        if (ticks >= static_cast<uint64_t>(portMAX_DELAY))
                                return portMAX_DELAY - 1;

                        // Milliseconds covered by the whole ticks, rounded up
                        const uint64_t covered = (ticks * 1000U + configTICK_RATE_HZ - 1) / configTICK_RATE_HZ;
                        remainder = covered < msecs ? static_cast<unsigned int>(msecs - covered) : 0;

                        return static_cast<TickType_t>(ticks);
                }

                inline unsigned int ticksToMs(TickType_t ticks)
                {
                        return static_cast<unsigned int>((static_cast<uint64_t>(ticks) * 1000U) / configTICK_RATE_HZ);
                }

                inline void yieldFromIsr(BaseType_t &tasks_woken) __attribute__((always_inline));
                inline void yieldFromIsr(BaseType_t &tasks_woken)
                {
#if defined(FRT_HOST)
                        FRT_UNUSED(tasks_woken);
#elif defined(ESP32)
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

#if defined(ESP32)
                /**
                 *  The spinlock used by all frt critical sections on ESP32.
                 *  It must be a single object for the whole program: a `static`
                 *  variable in a header would give every translation unit its own
                 *  lock, and critical sections in different files would not
                 *  exclude each other on the second core.
                 */
                inline portMUX_TYPE &spinlock()
                {
                        static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
                        return lock;
                }
#endif
        }

        /**
         *  RAII critical section that can be used from tasks, ISRs and before the
         *  scheduler is started. The way the section was entered is remembered,
         *  so it is always left the same way.
         *
         *  @code
         *  {
         *      frt::CriticalSection cs;
         *      shared_counter++;
         *  }
         *  @endcode
         */
        class CriticalSection final
        {
        public:
                struct State
                {
                        uint8_t mode;
                        UBaseType_t saved;
                };

                CriticalSection() : m_state(enter()) {}
                ~CriticalSection() { exit(m_state); }

                CriticalSection(const CriticalSection &other) = delete;
                CriticalSection &operator=(const CriticalSection &other) = delete;

                static State enter()
                {
                        State s = {MODE_TASK, 0};

#if defined(FRT_HOST)
                        taskENTER_CRITICAL();
#elif defined(ESP32)
                        if (FRT_IS_ISR())
                        {
                                s.mode = MODE_ISR;
                                taskENTER_CRITICAL_ISR(&detail::spinlock());
                        }
                        else if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
                        {
                                taskENTER_CRITICAL(&detail::spinlock());
                        }
                        else
                        {
                                s.mode = MODE_NO_SCHEDULER;
                                taskDISABLE_INTERRUPTS();
                        }
#else
                        if (FRT_IS_ISR())
                        {
                                s.mode = MODE_ISR;
                                s.saved = taskENTER_CRITICAL_FROM_ISR();
                        }
                        else if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
                        {
                                taskENTER_CRITICAL();
                        }
                        else
                        {
                                s.mode = MODE_NO_SCHEDULER;
                                s.saved = __get_PRIMASK();
                                __disable_irq();
                        }
#endif
                        return s;
                }

                static void exit(const State &s)
                {
#if defined(FRT_HOST)
                        FRT_UNUSED(s);
                        taskEXIT_CRITICAL();
#elif defined(ESP32)
                        if (s.mode == MODE_ISR)
                                taskEXIT_CRITICAL_ISR(&detail::spinlock());
                        else if (s.mode == MODE_TASK)
                                taskEXIT_CRITICAL(&detail::spinlock());
                        else
                                taskENABLE_INTERRUPTS();
#else
                        if (s.mode == MODE_ISR)
                                taskEXIT_CRITICAL_FROM_ISR(s.saved);
                        else if (s.mode == MODE_TASK)
                                taskEXIT_CRITICAL();
                        else
                                __set_PRIMASK(s.saved);
#endif
                }

        private:
                enum : uint8_t
                {
                        MODE_TASK,
                        MODE_ISR,
                        MODE_NO_SCHEDULER
                };

                State m_state;
        };

        namespace detail
        {
                /**
                 *  Backing store for the FRT_CRITICAL_ENTER/EXIT macros, which
                 *  cannot carry state between the two calls. The stack is only
                 *  touched while inside the critical section, so it needs no
                 *  further protection.
                 */
                static const unsigned int CRITICAL_NESTING_MAX = 8;

                inline CriticalSection::State *criticalStack()
                {
                        static CriticalSection::State stack[CRITICAL_NESTING_MAX];
                        return stack;
                }

                inline unsigned int &criticalDepth()
                {
                        static unsigned int depth = 0;
                        return depth;
                }

                inline void criticalEnter()
                {
                        const CriticalSection::State s = CriticalSection::enter();
                        unsigned int &depth = criticalDepth();
                        configASSERT(depth < CRITICAL_NESTING_MAX);
                        criticalStack()[depth++] = s;
                }

                inline void criticalExit()
                {
                        unsigned int &depth = criticalDepth();
                        configASSERT(depth > 0);
                        const CriticalSection::State s = criticalStack()[--depth];
                        CriticalSection::exit(s);
                }
        }

        /**
         *  Start the FreeRTOS scheduler on platforms where the Arduino core does
         *  not do this on its own (STM32). On ESP32 and nRF52 this is a no-op.
         *  Never returns on STM32.
         */
        inline void spin() __attribute__((always_inline));
        inline void spin()
        {
#if defined(STM32) || defined(FRT_HOST)
                // Start the kernel scheduler
                assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
                vTaskStartScheduler();
#endif
        }
}

/**
 *  Legacy critical section macros. Prefer frt::CriticalSection, which is
 *  scope based and cannot be left unbalanced. Calls must be properly nested.
 */
#ifndef FRT_CRITICAL_ENTER
#define FRT_CRITICAL_ENTER() frt::detail::criticalEnter()
#endif

#ifndef FRT_CRITICAL_EXIT
#define FRT_CRITICAL_EXIT() frt::detail::criticalExit()
#endif

#endif // __FRT_H__
