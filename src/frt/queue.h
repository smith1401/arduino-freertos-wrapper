#ifndef __FRT_QUEUE_H__
#define __FRT_QUEUE_H__

#include "frt.h"

namespace frt
{
    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Queue final
    {
    public:
        Queue()
        {
#if configSUPPORT_STATIC_ALLOCATION > 0
            _handle = xQueueCreateStatic(QUEUE_SIZE, sizeof(T), buffer, &state);
#else
            _handle = xQueueCreate(QUEUE_SIZE, sizeof(T));
#endif
        }

        ~Queue()
        {
            vQueueDelete(_handle);
        }

        explicit Queue(const Queue &other) = delete;
        Queue &operator=(const Queue &other) = delete;

        unsigned int getFillLevel() const
        {
            return QUEUE_SIZE - uxQueueSpacesAvailable(_handle);
        }

        unsigned int available() const
        {
            if (FRT_IS_ISR())
                return uxQueueMessagesWaitingFromISR(_handle);
            else
                return uxQueueMessagesWaiting(_handle);
        }

        bool addToSet(QueueSetHandle_t &sethandle)
        {
            return xQueueAddToSet(_handle, sethandle);
        }

        bool isMember(QueueSetMemberHandle_t &memberHandle)
        {
            return _handle == memberHandle;
        }

        QueueHandle_t *handle()
        {
            return &_handle;
        }

        bool override(const T &item)
        {
            BaseType_t taskWoken = pdFALSE;
            if (FRT_IS_ISR())
            {
                if (xQueueOverwriteFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
                return xQueueOverwrite(_handle, &item);

            return true;
        }

        bool push(const T &item)
        {
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueSendFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueueSend(_handle, &item, portMAX_DELAY) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool push(const T &item, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueSendFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueueSend(_handle, &item, max(1U, (unsigned int)ticks)) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool push(const T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;

            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueSendFromISR(_handle, &item, &taskWoken) == pdTRUE)
                {
                    remainder = 0;
                    detail::yieldFromIsr(taskWoken);
                    return true;
                }
            }
            else
            {
                if (xQueueSend(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
                {
                    remainder = 0;
                    return true;
                }
            }

            return false;
        }

        bool pop(T &item)
        {
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueReceiveFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueueReceive(_handle, &item, portMAX_DELAY) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool pop(T &item, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueReceiveFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueueReceive(_handle, &item, max(1U, (unsigned int)ticks)) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool pop(T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;

            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueueReceiveFromISR(_handle, &item, &taskWoken) == pdTRUE)
                {
                    remainder = 0;
                    detail::yieldFromIsr(taskWoken);
                    return true;
                }
            }
            else
            {
                if (xQueueReceive(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
                {
                    remainder = 0;
                    return true;
                }
            }

            return false;
        }

        bool peek(T &item)
        {
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueuePeekFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueuePeek(_handle, &item, portMAX_DELAY) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool peek(T &item, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueuePeekFromISR(_handle, &item, &taskWoken) != pdTRUE)
                {
                    return false;
                }
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                if (xQueuePeek(_handle, &item, max(1U, (unsigned int)ticks)) != pdTRUE)
                {
                    return false;
                }
            }

            return true;
        }

        bool peek(T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;

            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);
            BaseType_t taskWoken = pdFALSE;

            if (FRT_IS_ISR())
            {
                if (xQueuePeekFromISR(_handle, &item, &taskWoken) == pdTRUE)
                {
                    remainder = 0;
                    detail::yieldFromIsr(taskWoken);
                    return true;
                }
            }
            else
            {
                if (xQueuePeek(_handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
                {
                    remainder = 0;
                    return true;
                }
            }

            return false;
        }

    private:
        QueueHandle_t _handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        uint8_t buffer[QUEUE_SIZE * sizeof(T)];
        StaticQueue_t state;
#endif
    };
}

#endif // __FRT_QUEUE_H__