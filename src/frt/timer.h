#ifndef __FRT_TIMER_H__
#define __FRT_TIMER_H__

#include <functional>
#include <type_traits>
#include <string.h>

#include "frt.h"

#define portZERO_DELAY pdMS_TO_TICKS(0)

namespace frt
{
    namespace detail
    {
#if (INCLUDE_xTimerPendFunctionCall == 1) && (configUSE_TIMERS == 1)
#ifndef configTIMER_SERVICE_TASK_NAME
#define configTIMER_SERVICE_TASK_NAME "Tmr Svc"
#endif

        inline bool isTimerTask()
        {
#if (INCLUDE_xTimerGetTimerDaemonTaskHandle == 1)
            return xTaskGetCurrentTaskHandle() == xTimerGetTimerDaemonTaskHandle();
#else
            return strcmp(pcTaskGetName(NULL), configTIMER_SERVICE_TASK_NAME) == 0;
#endif
        }

        inline void giveSemaphore(void *sem, uint32_t)
        {
            xSemaphoreGive(static_cast<SemaphoreHandle_t>(sem));
        }

        /**
         *  Block until the timer task has processed every command queued so far.
         *  Timer commands are handled in order, so once a function pended
         *  after them has run, they are done.
         */
        inline void flushTimerCommands()
        {
            if (FRT_IS_ISR() || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
                return;

            // Called from a timer callback: we would wait for ourselves.
            if (isTimerTask())
                return;

#if configSUPPORT_STATIC_ALLOCATION > 0
            StaticSemaphore_t buf;
            SemaphoreHandle_t done = xSemaphoreCreateBinaryStatic(&buf);
#else
            SemaphoreHandle_t done = xSemaphoreCreateBinary();
            if (done == nullptr)
                return;
#endif

            if (xTimerPendFunctionCall(giveSemaphore, done, 0, portMAX_DELAY) == pdPASS)
                xSemaphoreTake(done, portMAX_DELAY);

            vSemaphoreDelete(done);
        }
#define FRT_TIMER_CAN_FLUSH 1
#else
        inline void flushTimerCommands() {}
#define FRT_TIMER_CAN_FLUSH 0
#endif
    }

    /**
     *  Software timer. Derive from it and implement run(), or use
     *  frt::CallbackTimer to pass a function / lambda.
     *
     *  run() is executed in the context of the FreeRTOS timer task: keep it
     *  short and never block in it.
     *
     *  All methods can be called from tasks and ISRs. Command timeouts are
     *  in ticks (they are directly passed to FreeRTOS).
     *
     *  Do not destroy a timer from inside its own run() method. If you derive
     *  from Timer, call destroy() in your destructor so run() cannot be called
     *  on a partly destroyed object.
     */
    class Timer
    {
    public:
        /**
         *  Construct a named timer.
         *  Timers are not active after they are created, you need to
         *  activate them via start(), reset(), etc.
         *
         *  @param TimerName Name of the timer for debug.
         *  @param PeriodInTicks When does the timer expire and run your run()
         *         method. Use pdMS_TO_TICKS() to convert from milliseconds.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(const char *const TimerName,
              TickType_t PeriodInTicks,
              bool Periodic = true)
        {
            create(TimerName, PeriodInTicks, Periodic);
        }

        /**
         *  Construct an unnamed timer.
         *
         *  @param PeriodInTicks When does the timer expire and run your run()
         *         method.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(TickType_t PeriodInTicks,
              bool Periodic = true)
        {
            create(NULL, PeriodInTicks, Periodic);
        }

        virtual ~Timer()
        {
            destroy();
        }

        Timer(const Timer &other) = delete;
        Timer &operator=(const Timer &other) = delete;

        /**
         *  Is the timer currently active?
         *
         *  @return true if the timer is active, false otherwise.
         */
        bool isActive() const
        {
            return xTimerIsTimerActive(handle) != pdFALSE;
        }

        /**
         *  Start a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait (ticks) to send this command to the
         *         timer task.
         *  @returns true if this command will be sent to the timer task,
         *           false if it will not (i.e. timeout).
         */
        bool start(TickType_t CmdTimeout = portMAX_DELAY)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xTimerStartFromISR(handle, &taskWoken) == pdPASS;

                if (success)
                    detail::yieldFromIsr(taskWoken);

                return success;
            }

            return xTimerStart(handle, CmdTimeout) == pdPASS;
        }

        /**
         *  Stop a timer. This changes the state to inactive.
         */
        bool stop(TickType_t CmdTimeout = portMAX_DELAY)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xTimerStopFromISR(handle, &taskWoken) == pdPASS;

                if (success)
                    detail::yieldFromIsr(taskWoken);

                return success;
            }

            return xTimerStop(handle, CmdTimeout) == pdPASS;
        }

        /**
         *  Reset (restart) a timer. This changes the state to active.
         */
        bool reset(TickType_t CmdTimeout = portMAX_DELAY)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xTimerResetFromISR(handle, &taskWoken) == pdPASS;

                if (success)
                    detail::yieldFromIsr(taskWoken);

                return success;
            }

            return xTimerReset(handle, CmdTimeout) == pdPASS;
        }

        /**
         *  Change a timer's period. This also starts a dormant timer.
         *
         *  @param NewPeriod The period in ticks.
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer task.
         */
        bool setPeriod(TickType_t NewPeriod,
                       TickType_t CmdTimeout = portMAX_DELAY)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xTimerChangePeriodFromISR(handle, NewPeriod, &taskWoken) == pdPASS;

                if (success)
                    detail::yieldFromIsr(taskWoken);

                return success;
            }

            return xTimerChangePeriod(handle, NewPeriod, CmdTimeout) == pdPASS;
        }

        /** Change the period in milliseconds. This also starts a dormant timer. */
        bool setPeriodMs(unsigned int msecs, TickType_t CmdTimeout = portMAX_DELAY)
        {
            return setPeriod(detail::msToTicks(msecs), CmdTimeout);
        }

        /** Current period in ticks. */
        TickType_t period() const
        {
            return xTimerGetPeriod(handle);
        }

        /** Tick count at which the timer expires next (only valid if active). */
        TickType_t expiryTime() const
        {
            return xTimerGetExpiryTime(handle);
        }

        const char *name() const
        {
            return pcTimerGetName(handle);
        }

        TimerHandle_t getHandle() const
        {
            return handle;
        }

#if (INCLUDE_xTimerGetTimerDaemonTaskHandle == 1)
        /**
         *  If you need it, obtain the task handle of the FreeRTOS
         *  task that is running the timers.
         *
         *  @return Task handle of the FreeRTOS timer task.
         */
        static TaskHandle_t getTimerDaemonHandle()
        {
            return xTimerGetTimerDaemonTaskHandle();
        }
#endif

    protected:
        /**
         *  Implementation of your actual timer code.
         *  You must override this function.
         */
        virtual void run() = 0;

        /**
         *  Delete the FreeRTOS timer and wait until the timer task has handled
         *  that. Afterwards run() is guaranteed not to be called anymore, and
         *  the timer task no longer touches this object (the callback context
         *  and, with static allocation, the timer buffer live inside it).
         */
        void destroy()
        {
            if (handle == nullptr)
                return;

            xTimerDelete(handle, portMAX_DELAY);
            handle = nullptr;
            detail::flushTimerCommands();
        }

    private:
        TimerHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticTimer_t buffer;
#endif

        void create(const char *const name, TickType_t period, bool periodic)
        {
#if configSUPPORT_STATIC_ALLOCATION > 0
            handle = xTimerCreateStatic(name,
                                        period,
                                        periodic ? pdTRUE : pdFALSE,
                                        this,
                                        TimerCallbackFunctionAdapter,
                                        &buffer);
#else
            handle = xTimerCreate(name,
                                  period,
                                  periodic ? pdTRUE : pdFALSE,
                                  this,
                                  TimerCallbackFunctionAdapter);
#endif
            configASSERT(handle);
        }

        /**
         *  Adapter that forwards the C callback to the virtual run() method of
         *  the timer object stored in the timer ID.
         */
        static void TimerCallbackFunctionAdapter(TimerHandle_t xTimer)
        {
            Timer *timer = static_cast<Timer *>(pvTimerGetTimerID(xTimer));
            timer->run();
        }
    };

    /**
     *  Timer that calls a function, functor or lambda.
     *
     *  @code
     *  frt::CallbackTimer blink("blink", pdMS_TO_TICKS(500), true, [] {
     *      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
     *  });
     *  blink.start();
     *  @endcode
     */
    class CallbackTimer final : public Timer
    {
    public:
        typedef std::function<void()> Callback;

        CallbackTimer(const char *const TimerName,
                      TickType_t PeriodInTicks,
                      bool Periodic,
                      Callback callback) : Timer(TimerName, PeriodInTicks, Periodic),
                                           m_callback(callback)
        {
        }

        CallbackTimer(TickType_t PeriodInTicks,
                      bool Periodic,
                      Callback callback) : Timer(PeriodInTicks, Periodic),
                                           m_callback(callback)
        {
        }

        ~CallbackTimer()
        {
            // Before m_callback is destroyed
            destroy();
        }

    protected:
        void run() override
        {
            if (m_callback)
                m_callback();
        }

    private:
        Callback m_callback;
    };
}

#endif // __FRT_TIMER_H__
