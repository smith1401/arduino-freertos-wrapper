#ifndef __FRT_EVENT_GROUP_H__
#define __FRT_EVENT_GROUP_H__

#include "frt.h"

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
            return xEventGroupSetBits(handle, bitsToSet);
        }

        EventBits_t setBitsFromISR(const EventBits_t bitsToSet)
        {
            return xEventGroupSetBitsFromISR(handle, bitsToSet, &tasks_woken_from_set_bits);
        }

        EventBits_t getBits()
        {
            return xEventGroupGetBits(handle);
        }

        EventBits_t getBitsFromISR()
        {
            return xEventGroupGetBitsFromISR(handle);
        }

        EventBits_t clearBits(const EventBits_t bitsToClear)
        {
            return xEventGroupClearBits(handle, bitsToClear);
        }

        EventBits_t clearBitsFromISR(const EventBits_t bitsToClear)
        {
            return xEventGroupClearBitsFromISR(handle, bitsToClear);
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits)
        {
            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, portMAX_DELAY);
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, max(1U, (unsigned int)ticks));
        }

        EventBits_t waitBits(const EventBits_t bitsToWaitFor, const bool clearOnExit, const bool waitForAllBits, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            return xEventGroupWaitBits(handle, bitsToWaitFor, clearOnExit, waitForAllBits, max(1U, (unsigned int)ticks));
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor)
        {
            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, portMAX_DELAY);
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, max(1U, (unsigned int)ticks));
        }

        EventBits_t sync(const EventBits_t bitsToSet, const EventBits_t bitsToWaitFor, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            return xEventGroupSync(handle, bitsToSet, bitsToWaitFor, max(1U, (unsigned int)ticks));
        }

        void prepareSetBitsFromISR()
        {
            tasks_woken_from_set_bits = 0;
        }

        void finalizeSetBitsFromISR() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (tasks_woken_from_set_bits)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(tasks_woken_from_set_bits);
#endif
        }

    private:
        EventGroupHandle_t handle;
        BaseType_t tasks_woken_from_set_bits;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticEventGroup_t buffer;
#endif
    };
}

#endif // __FRT_EVENT_GROUP_H__