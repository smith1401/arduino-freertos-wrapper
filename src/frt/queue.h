#ifndef __FRT_QUEUE_H__
#define __FRT_QUEUE_H__

#include <type_traits>

#include "frt.h"

namespace frt
{
    /**
     *  Type safe FreeRTOS queue holding up to QUEUE_SIZE items of type T.
     *
     *  Items are copied byte-wise into the queue, therefore T must be
     *  trivially copyable (no String, std::vector, ... - pass pointers or
     *  fixed size structs instead).
     *
     *  All methods can be called from tasks and from ISRs. In an ISR the
     *  timeout is ignored and the call never blocks.
     *
     *  Timeouts are in milliseconds: 0 does not block, frt::FOREVER blocks
     *  until the operation succeeds.
     */
    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Queue final
    {
        static_assert(QUEUE_SIZE > 0, "frt::Queue needs room for at least one item");
#if !defined(__GNUC__) || __GNUC__ >= 5
        static_assert(std::is_trivially_copyable<T>::value,
                      "frt::Queue copies items with memcpy, T must be trivially copyable");
#endif

    public:
        Queue()
        {
#if configSUPPORT_STATIC_ALLOCATION > 0
            _handle = xQueueCreateStatic(QUEUE_SIZE, sizeof(T), buffer, &state);
#else
            _handle = xQueueCreate(QUEUE_SIZE, sizeof(T));
#endif
            configASSERT(_handle);
        }

        ~Queue()
        {
            vQueueDelete(_handle);
        }

        Queue(const Queue &other) = delete;
        Queue &operator=(const Queue &other) = delete;

        /** Number of free slots. */
        unsigned int availableForWrite() const
        {
            if (FRT_IS_ISR())
                return QUEUE_SIZE - uxQueueMessagesWaitingFromISR(_handle);
            else
                return uxQueueSpacesAvailable(_handle);
        }

        /** Number of items waiting to be read. */
        unsigned int available() const
        {
            if (FRT_IS_ISR())
                return uxQueueMessagesWaitingFromISR(_handle);
            else
                return uxQueueMessagesWaiting(_handle);
        }

        bool isEmpty() const
        {
            if (FRT_IS_ISR())
                return xQueueIsQueueEmptyFromISR(_handle) != pdFALSE;
            else
                return available() == 0;
        }

        bool isFull() const
        {
            if (FRT_IS_ISR())
                return xQueueIsQueueFullFromISR(_handle) != pdFALSE;
            else
                return availableForWrite() == 0;
        }

        static constexpr unsigned int capacity() { return QUEUE_SIZE; }

        /** Remove all items. Must not be called from an ISR. */
        void clear()
        {
            xQueueReset(_handle);
        }

        bool addToSet(QueueSetHandle_t &sethandle)
        {
            return xQueueAddToSet(_handle, sethandle) == pdPASS;
        }

        bool removeFromSet(QueueSetHandle_t &sethandle)
        {
            return xQueueRemoveFromSet(_handle, sethandle) == pdPASS;
        }

        bool isMember(QueueSetMemberHandle_t &memberHandle) const
        {
            return _handle == memberHandle;
        }

        QueueHandle_t *handle()
        {
            return &_handle;
        }

        /**
         *  Overwrite the item in a queue of length one (mailbox semantics).
         *  Never blocks.
         */
        bool override(const T &item)
        {
            static_assert(QUEUE_SIZE == 1, "override() is only valid for queues of size 1");

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                if (xQueueOverwriteFromISR(_handle, &item, &taskWoken) != pdTRUE)
                    return false;
                detail::yieldFromIsr(taskWoken);
                return true;
            }

            return xQueueOverwrite(_handle, &item) == pdTRUE;
        }

        /** Append an item to the back of the queue. */
        bool push(const T &item, unsigned int msecs = FOREVER)
        {
            return pushTicks(item, detail::msToTicks(msecs));
        }

        bool push(const T &item, unsigned int msecs, unsigned int &remainder)
        {
            if (pushTicks(item, detail::msToTicksCarry(msecs, remainder)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        /** Insert an item at the front of the queue (it is read next). */
        bool pushFront(const T &item, unsigned int msecs = FOREVER)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                if (xQueueSendToFrontFromISR(_handle, &item, &taskWoken) != pdTRUE)
                    return false;
                detail::yieldFromIsr(taskWoken);
                return true;
            }

            return xQueueSendToFront(_handle, &item, detail::msToTicks(msecs)) == pdTRUE;
        }

        /** Remove the oldest item from the queue. */
        bool pop(T &item, unsigned int msecs = FOREVER)
        {
            return popTicks(item, detail::msToTicks(msecs));
        }

        bool pop(T &item, unsigned int msecs, unsigned int &remainder)
        {
            if (popTicks(item, detail::msToTicksCarry(msecs, remainder)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        /** Read the oldest item without removing it. */
        bool peek(T &item, unsigned int msecs = FOREVER)
        {
            return peekTicks(item, detail::msToTicks(msecs));
        }

        bool peek(T &item, unsigned int msecs, unsigned int &remainder)
        {
            if (peekTicks(item, detail::msToTicksCarry(msecs, remainder)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

    private:
        bool pushTicks(const T &item, TickType_t ticks)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                if (xQueueSendToBackFromISR(_handle, &item, &taskWoken) != pdTRUE)
                    return false;
                detail::yieldFromIsr(taskWoken);
                return true;
            }

            return xQueueSendToBack(_handle, &item, ticks) == pdTRUE;
        }

        bool popTicks(T &item, TickType_t ticks)
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                if (xQueueReceiveFromISR(_handle, &item, &taskWoken) != pdTRUE)
                    return false;
                detail::yieldFromIsr(taskWoken);
                return true;
            }

            return xQueueReceive(_handle, &item, ticks) == pdTRUE;
        }

        bool peekTicks(T &item, TickType_t ticks)
        {
            if (FRT_IS_ISR())
                return xQueuePeekFromISR(_handle, &item) == pdTRUE;

            return xQueuePeek(_handle, &item, ticks) == pdTRUE;
        }

        QueueHandle_t _handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        uint8_t buffer[QUEUE_SIZE * sizeof(T)];
        StaticQueue_t state;
#endif
    };
}

#endif // __FRT_QUEUE_H__
