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
        bool Start(TickType_t CmdTimeout = portMAX_DELAY)
        {
            return xTimerStart(handle, CmdTimeout) == pdFALSE ? false : true;
        }

        /**
         *  Start a timer from ISR context. This changes the state to active.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool StartFromISR(BaseType_t *pxHigherPriorityTaskWoken)
        {
            return xTimerStartFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE ? false : true;
        }

        /**
         *  Stop a timer. This changes the state to inactive.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool Stop(TickType_t CmdTimeout = portMAX_DELAY)
        {
            return xTimerStop(handle, CmdTimeout) == pdFALSE ? false : true;
            // assert(xTimerStop(handle, CmdTimeout) == pdTRUE);
            // while (xTimerIsTimerActive(handle) == pdTRUE)
            //     vTaskDelay(1);
            // return true;
        }

        /**
         *  Stop a timer from ISR context. This changes the state to inactive.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool StopFromISR(BaseType_t *pxHigherPriorityTaskWoken)
        {
            return xTimerStopFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE ? false : true;
        }

        /**
         *  Reset a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool Reset(TickType_t CmdTimeout = portMAX_DELAY)
        {
            return xTimerReset(handle, CmdTimeout) == pdFALSE ? false : true;
        }

        /**
         *  Reset a timer from ISR context. This changes the state to active.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool ResetFromISR(BaseType_t *pxHigherPriorityTaskWoken)
        {
            return xTimerResetFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE ? false : true;
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
        bool SetPeriod(TickType_t NewPeriod,
                       TickType_t CmdTimeout = portMAX_DELAY)
        {
            return xTimerChangePeriod(handle, NewPeriod, CmdTimeout) == pdFALSE ? false : true;
        }

        /**
         *  Change a timer's period from ISR context.
         *
         *  @param NewPeriod The period in ticks.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool SetPeriodFromISR(TickType_t NewPeriod,
                              BaseType_t *pxHigherPriorityTaskWoken)
        {
            return xTimerChangePeriodFromISR(handle, NewPeriod, pxHigherPriorityTaskWoken) == pdFALSE ? false : true;
        }

#if (INCLUDE_xTimerGetTimerDaemonTaskHandle == 1)
        /**
         *  If you need it, obtain the task handle of the FreeRTOS
         *  task that is running the timers.
         *
         *  @return Task handle of the FreeRTOS timer task.
         */
        static TaskHandle_t GetTimerDaemonHandle()
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
        virtual void Run() = 0;

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
            timer->Run();
        }
    };
}

#endif // __FRT_TIMER_H__