#ifndef __FRT_EVENT_GROUP_H__
#define __FRT_EVENT_GROUP_H__

#include "frt.h"
#include "log.h"
namespace frt
{

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

        EventBits_t setBits(const EventBits_t bitsToSet)
        {
            if (FRT_IS_ISR())
            {
                EventBits_t bitsSet;
#if ((configUSE_TRACE_FACILITY == 1) && (INCLUDE_xTimerPendFunctionCall == 1) && (configUSE_TIMERS == 1))
                BaseType_t taskWoken = pdFALSE;
                bitsSet = xEventGroupSetBitsFromISR(handle, bitsToSet, &taskWoken);

                if (bitsSet != pdFAIL)
                {
                    detail::yieldFromIsr(taskWoken);
                }
#else
#warning "xEventGroupSetBitsFromISR not implemented for this platform"
#endif
                return bitsSet;
            }
            else
                return xEventGroupSetBits(handle, bitsToSet);
        }

        EventBits_t getBits()
        {
            if (FRT_IS_ISR())
                return xEventGroupGetBitsFromISR(handle);
            else
                return xEventGroupGetBits(handle);
        }

        EventBits_t clearBits(const EventBits_t bitsToClear)
        {
            if (FRT_IS_ISR())
                return xEventGroupClearBitsFromISR(handle, bitsToClear);
            else
                return xEventGroupClearBits(handle, bitsToClear);
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits)
        {
            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, portMAX_DELAY);
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);

            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, max(1U, (unsigned int)ticks));
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            // remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, max(1U, (unsigned int)ticks));
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor)
        {
            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, portMAX_DELAY);
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);

            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, max(1U, (unsigned int)ticks));
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            // remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, max(1U, (unsigned int)ticks));
        }

    private:
        EventGroupHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticEventGroup_t buffer;
#endif
    };
}

#endif // __FRT_EVENT_GROUP_H__