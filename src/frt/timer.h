#ifndef __FRT_TIMER_H__
#define __FRT_TIMER_H__

#include <functional>
#include <type_traits>

#include "frt.h"

namespace frt
{
    class Timer
    {
    public:
        /**
         *  Construct a named timer.
         *  Timers are not active after they are created, you need to
         *  activate them via Start, Reset, etc.
         *
         *  @throws TimerCreateException
         *  @param TimerName Name of the timer for debug.
         *  @param PeriodInTicks When does the timer expire and run your Run()
         *         method.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(const char *const TimerName,
              TickType_t PeriodInTicks,
              bool Periodic = true)
        {
            handle = xTimerCreate(TimerName,
                                  PeriodInTicks,
                                  Periodic ? pdTRUE : pdFALSE,
                                  this,
                                  TimerCallbackFunctionAdapter);

            assert(handle != nullptr);
        }

        /**
         *  Construct an unnamed timer.
         *  Timers are not active after they are created, you need to
         *  activate them via Start, Reset, etc.
         *
         *  @throws TimerCreateException
         *  @param PeriodInTicks When does the timer expire and run your Run()
         *         method.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(TickType_t PeriodInTicks,
              bool Periodic = true)
        {
            handle = xTimerCreate(NULL,
                                  PeriodInTicks,
                                  Periodic ? pdTRUE : pdFALSE,
                                  this,
                                  TimerCallbackFunctionAdapter);

            assert(handle != nullptr);
        }

        /**
         *  Destructor
         */
        virtual ~Timer()
        {
            xTimerDelete(handle, portMAX_DELAY);
        }

        /**
         *  Is the timer currently active?
         *
         *  @return true if the timer is active, false otherwise.
         */
        bool IsActive()
        {
            return xTimerIsTimerActive(handle) == pdFALSE ? false : true;
        }

        /**
         *  Start a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool start(TickType_t CmdTimeout = portMAX_DELAY)
        {
            bool success = false;
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                success = xTimerStartFromISR(handle, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                success = xTimerStart(handle, CmdTimeout);
            }

            return success;
        }

        /**
         *  Stop a timer. This changes the state to inactive.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool stop(TickType_t CmdTimeout = portMAX_DELAY)
        {
            bool success = false;
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                success = xTimerStopFromISR(handle, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                success = xTimerStop(handle, CmdTimeout);
            }

            return success;
        }

        /**
         *  Reset a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool reset(TickType_t CmdTimeout = portMAX_DELAY)
        {
            bool success = false;
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                success = xTimerResetFromISR(handle, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                success = xTimerReset(handle, CmdTimeout);
            }

            return success;
        }

        /**
         *  Change a timer's period.
         *
         *  @param NewPeriod The period in ticks.
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool setPeriod(TickType_t NewPeriod,
                       TickType_t CmdTimeout = portMAX_DELAY)
        {
            bool success = false;
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                success = xTimerChangePeriodFromISR(handle, NewPeriod, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                success = xTimerChangePeriod(handle, NewPeriod, CmdTimeout);
            }

            return success;
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

        /////////////////////////////////////////////////////////////////////////
        //
        //  Protected API
        //  Available from inside your Thread implementation.
        //  You should make sure that you are only calling these methods
        //  from within your Run() method, or that your Run() method is on the
        //  callstack.
        //
        /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  Implementation of your actual timer code.
         *  You must override this function.
         */
        virtual void run() = 0;

        /////////////////////////////////////////////////////////////////////////
        //
        //  Private API
        //  The internals of this wrapper class.
        //
        /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Reference to the underlying timer handle.
         */
        TimerHandle_t handle;

        /**
         *  Adapter function that allows you to write a class
         *  specific Run() function that interfaces with FreeRTOS.
         *  Look at the implementation of the constructors and this
         *  code to see how the interface between C and C++ is performed.
         */
        static void TimerCallbackFunctionAdapter(TimerHandle_t xTimer)
        {
            Timer *timer = static_cast<Timer *>(pvTimerGetTimerID(xTimer));
            timer->run();
        }
    };

}

#endif // __FRT_TIMER_H__