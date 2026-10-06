#ifndef __FRT_EVENT_GROUP_H__
#define __FRT_EVENT_GROUP_H__

#include "frt.h"

#if (configUSE_TRACE_FACILITY == 1) && (INCLUDE_xTimerPendFunctionCall == 1) && (configUSE_TIMERS == 1)
#define FRT_HAS_EVENT_GROUP_SET_BITS_FROM_ISR 1
#else
#define FRT_HAS_EVENT_GROUP_SET_BITS_FROM_ISR 0
#endif

namespace frt
{
    /**
     *  Event group. Timeouts are in milliseconds: 0 does not block,
     *  frt::FOREVER blocks until the condition is met.
     *
     *  The wait functions return the value of the event bits at the time the
     *  call returned. Check those against the bits you waited for to know
     *  whether the wait succeeded or timed out.
     */
    class EventGroup final
    {
    public:
        EventGroup() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                           xEventGroupCreateStatic(&buffer)
#else
                           xEventGroupCreate()
#endif
                       )
        {
            configASSERT(handle);
        }

        ~EventGroup()
        {
            vEventGroupDelete(handle);
        }

        EventGroup(const EventGroup &other) = delete;
        EventGroup &operator=(const EventGroup &other) = delete;

        /**
         *  Set bits. From a task this returns the bits after setting.
         *  From an ISR the request is deferred to the timer task, the return
         *  value is then non-zero if the request was queued successfully.
         *  Setting bits from an ISR needs configUSE_TRACE_FACILITY,
         *  INCLUDE_xTimerPendFunctionCall and configUSE_TIMERS enabled.
         */
        EventBits_t setBits(const EventBits_t bitsToSet)
        {
            if (FRT_IS_ISR())
            {
#if FRT_HAS_EVENT_GROUP_SET_BITS_FROM_ISR
                BaseType_t taskWoken = pdFALSE;
                if (xEventGroupSetBitsFromISR(handle, bitsToSet, &taskWoken) != pdPASS)
                    return 0;

                detail::yieldFromIsr(taskWoken);
                return bitsToSet;
#else
                configASSERT(false && "xEventGroupSetBitsFromISR is not available with this FreeRTOS config");
                return 0;
#endif
            }

            return xEventGroupSetBits(handle, bitsToSet);
        }

        EventBits_t getBits() const
        {
            if (FRT_IS_ISR())
                return xEventGroupGetBitsFromISR(handle);
            else
                return xEventGroupGetBits(handle);
        }

        /**
         *  Clear bits. Returns the bits before they were cleared (task context).
         *  From an ISR the request is deferred to the timer task.
         */
        EventBits_t clearBits(const EventBits_t bitsToClear)
        {
            if (FRT_IS_ISR())
            {
#if (INCLUDE_xTimerPendFunctionCall == 1) && (configUSE_TIMERS == 1)
                const EventBits_t before = xEventGroupGetBitsFromISR(handle);
                xEventGroupClearBitsFromISR(handle, bitsToClear);
                return before;
#else
                configASSERT(false && "xEventGroupClearBitsFromISR is not available with this FreeRTOS config");
                return 0;
#endif
            }

            return xEventGroupClearBits(handle, bitsToClear);
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs = FOREVER)
        {
            return waitBitsTicks(bitsToWaitFor, clearOnExit, waitForAllBits, detail::msToTicks(msecs));
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs, unsigned int &remainder)
        {
            const EventBits_t bits = waitBitsTicks(bitsToWaitFor, clearOnExit, waitForAllBits, detail::msToTicksCarry(msecs, remainder));
            const bool success = waitForAllBits ? (bits & bitsToWaitFor) == bitsToWaitFor
                                                : (bits & bitsToWaitFor) != 0;
            if (success)
                remainder = 0;

            return bits;
        }

        /** Wait until any of the given bits is set. */
        bool waitAny(const EventBits_t bits, unsigned int msecs = FOREVER, bool clearOnExit = true)
        {
            return (waitBits(bits, clearOnExit, false, msecs) & bits) != 0;
        }

        /** Wait until all of the given bits are set. */
        bool waitAll(const EventBits_t bits, unsigned int msecs = FOREVER, bool clearOnExit = true)
        {
            return (waitBits(bits, clearOnExit, true, msecs) & bits) == bits;
        }

        /**
         *  Rendezvous: atomically set bitsToSet and wait for bitsToWaitFor.
         */
        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs = FOREVER)
        {
            configASSERT(!FRT_IS_ISR());
            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, detail::msToTicks(msecs));
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs, unsigned int &remainder)
        {
            configASSERT(!FRT_IS_ISR());
            const EventBits_t bits = xEventGroupSync(handle, bitsToSet, bitsToWaitFor, detail::msToTicksCarry(msecs, remainder));
            if ((bits & bitsToWaitFor) == bitsToWaitFor)
                remainder = 0;

            return bits;
        }

    private:
        EventBits_t waitBitsTicks(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, TickType_t ticks)
        {
            configASSERT(!FRT_IS_ISR());
            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit ? pdTRUE : pdFALSE, waitForAllBits ? pdTRUE : pdFALSE, ticks);
        }

        EventGroupHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticEventGroup_t buffer;
#endif
    };
}

#endif // __FRT_EVENT_GROUP_H__
