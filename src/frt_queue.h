#ifndef __FRT_QUEUE_H__
#define __FRT_QUEUE_H__

namespace frt
{
    template <typename T, unsigned int ITEMS>
    class Queue final
    {
    public:
        Queue() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                      xQueueCreateStatic(ITEMS, sizeof(T), buffer, &state)
#else
                      xQueueCreate(ITEMS, sizeof(T))
#endif
                  )
        {
        }

        ~Queue()
        {
            vQueueDelete(handle);
        }

        explicit Queue(const Queue &other) = delete;
        Queue &operator=(const Queue &other) = delete;

        unsigned int getFillLevel() const
        {
            return ITEMS - uxQueueSpacesAvailable(handle);
        }

        void push(const T &item)
        {
            xQueueSend(handle, &item, portMAX_DELAY);
        }

        bool push(const T &item, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xQueueSend(handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE;
        }

        bool push(const T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xQueueSend(handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
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
            return xQueueSendFromISR(handle, &item, &higher_priority_task_woken_from_push) == pdTRUE;
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

        void pop(T &item)
        {
            xQueueReceive(handle, &item, portMAX_DELAY);
        }

        bool pop(T &item, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xQueueReceive(handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE;
        }

        bool pop(T &item, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xQueueReceive(handle, &item, max(1U, (unsigned int)ticks)) == pdTRUE)
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

        bool popFromInterrupt(const T &item)
        {
            return xQueueReceiveFromISR(handle, &item, &higher_priority_task_woken_from_pop);
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
        QueueHandle_t handle;
        BaseType_t higher_priority_task_woken_from_push;
        BaseType_t higher_priority_task_woken_from_pop;
#if configSUPPORT_STATIC_ALLOCATION > 0
        uint8_t buffer[ITEMS * sizeof(T)];
        StaticQueue_t state;
#endif
    };

}

#endif // __FRT_QUEUE_H__