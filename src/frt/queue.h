#ifndef __FRT_QUEUE_H__
#define __FRT_QUEUE_H__

#include "frt.h"

namespace frt
{
    template <typename T>
    class Queue final
    {
    public:
        Queue(uint32_t queue_size = 10)
        {
#if configSUPPORT_STATIC_ALLOCATION > 0
            buffer = (uint8_t *)pvPortMalloc(queue_size * sizeof(T));
            _handle = xQueueCreateStatic(queue_size, sizeof(T), buffer, &state);
#else
            _handle = xQueueCreate(queue_size, sizeof(T));
#endif
            _queue_size = queue_size;
        }

        ~Queue()
        {
#if configSUPPORT_STATIC_ALLOCATION > 0
            delete[] buffer;
#endif
            vQueueDelete(_handle);
        }

        explicit Queue(const Queue &other) = delete;
        Queue &operator=(const Queue &other) = delete;

        unsigned int getFillLevel() const
        {
            return _queue_size - uxQueueSpacesAvailable(_handle);
        }

        void addToSet(QueueSetHandle_t &sethandle)
        {
            xQueueAddToSet(_handle, sethandle);
        }

        bool isMember(QueueSetMemberHandle_t &memberHandle)
        {
            return _handle == memberHandle;
        }

        QueueHandle_t *handle()
        {
            return &_handle;
        }

        void override(const T &item)
        {
            xQueueOverwrite(_handle, &item);
        }

        void push(const T &item)
        {
            xQueueSend(_handle, &item, portMAX_DELAY);
        }

        bool push(const T &item, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xQueueSend(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE;
        }

        bool push(const T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xQueueSend(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void preparePushFromInterrupt()
        {
            higher_priority_task_woken_from_push = 0;
        }

        bool pushFromInterrupt(const T &item)
        {
            return xQueueSendFromISR(_handle, &item, &higher_priority_task_woken_from_push) == pdTRUE;
        }

        void finalizePushFromInterrupt() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (higher_priority_task_woken_from_push)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(higher_priority_task_woken_from_push);
#endif
        }

        bool pop(T &item)
        {
            return xQueueReceive(_handle, &item, portMAX_DELAY);
        }

        bool pop(T &item, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xQueueReceive(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE;
        }

        bool pop(T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xQueueReceive(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void preparePopFromInterrupt()
        {
            higher_priority_task_woken_from_pop = 0;
        }

        bool popFromInterrupt(T &item)
        {
            return xQueueReceiveFromISR(_handle, &item, &higher_priority_task_woken_from_pop);
        }

        void finalizePopFromInterrupt() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (higher_priority_task_woken_from_pop)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(higher_priority_task_woken_from_pop);
#endif
        }

    private:
        QueueHandle_t _handle;
        BaseType_t higher_priority_task_woken_from_push;
        BaseType_t higher_priority_task_woken_from_pop;
        uint32_t _queue_size;
#if configSUPPORT_STATIC_ALLOCATION > 0
        // uint8_t buffer[ITEMS * sizeof(T)];
        uint8_t *buffer;
        StaticQueue_t state;
#endif
    };

}

#endif // __FRT_QUEUE_H__