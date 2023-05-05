#ifndef __FRT_TASK_H__
#define __FRT_TASK_H__

#include "frt.h"

namespace frt
{
    template <typename T, unsigned int STACK_SIZE = configMINIMAL_STACK_SIZE * sizeof(StackType_t)>
    class Task
    {
    public:
        Task() : running(false),
                 do_stop(false),
                 handle(nullptr)
        {
#ifdef ESP32
            mtx = portMUX_INITIALIZER_UNLOCKED;
#endif
        }

        ~Task()
        {
            stop();
        }

        explicit Task(const Task &other) = delete;
        Task &operator=(const Task &other) = delete;

        bool start(unsigned char priority = 0, const char *name = "")
        {
            this->name = name;
            
            if (priority >= configMAX_PRIORITIES)
            {
                priority = configMAX_PRIORITIES - 1;
            }

            // assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

#if configSUPPORT_STATIC_ALLOCATION > 0
            handle = xTaskCreateStatic(
                entryPoint,
                name,
                STACK_SIZE / sizeof(StackType_t),
                this,
                priority,
                stack,
                &state);
            return handle;
#else
            return xTaskCreate(
                       entryPoint,
                       name,
                       STACK_SIZE / sizeof(StackType_t),
                       this,
                       priority,
                       &handle) == pdPASS;
#endif
        }

        bool stop()
        {
            return stop(false);
        }

        bool stopFromIdleTask()
        {
            return stop(true);
        }

        bool isRunning() const
        {
            bool res = false;

#ifdef ESP32
            taskENTER_CRITICAL(&mtx);
            res = running;
            taskEXIT_CRITICAL(&mtx);
#else
            taskENTER_CRITICAL();
            res = running;
            taskEXIT_CRITICAL();
#endif

            return res;
        }

        unsigned int getUsedStackSize() const
        {
            return STACK_SIZE - uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
        }

        void post()
        {
            xTaskNotifyGive(handle);
        }

        void preparePostFromInterrupt()
        {
            higher_priority_task_woken = 0;
        }

        void postFromInterrupt()
        {
            vTaskNotifyGiveFromISR(handle, &higher_priority_task_woken);
        }

        void finalizePostFromInterrupt() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (higher_priority_task_woken)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(higher_priority_task_woken);
#endif
        }

    protected:
        virtual bool run() = 0;
        
        void yield()
        {
            taskYIELD();
        }

        void msleep(unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            vTaskDelay(max(1U, (unsigned int)ticks));
        }

        void msleep(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            vTaskDelay(max(1U, (unsigned int)ticks));
        }

        void wait()
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        bool wait(unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return ulTaskNotifyTake(pdTRUE, max(1U, (unsigned int)ticks));
        }

        bool wait(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (ulTaskNotifyTake(pdTRUE, max(1U, (unsigned int)ticks)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void beginCriticalSection() __attribute__((always_inline))
        {
#ifdef ESP32
            portMUX_TYPE mtx = portMUX_INITIALIZER_UNLOCKED;
            taskENTER_CRITICAL(&mtx);
#else
            taskENTER_CRITICAL();
#endif
        }

        void endCriticalSection() __attribute__((always_inline))
        {
#ifdef ESP32
            portMUX_TYPE mtx = portMUX_INITIALIZER_UNLOCKED;
            taskEXIT_CRITICAL(&mtx);
#else
            taskEXIT_CRITICAL();
#endif
        }

    private:
        bool stop(bool from_idle_task)
        {
            if (!handle)
            {
                return false;
            }

#ifdef ESP32
            taskENTER_CRITICAL(&mtx);
#else
            taskENTER_CRITICAL();
#endif
            do_stop = true;
            while (running)
            {
#ifdef ESP32
                taskEXIT_CRITICAL(&mtx);
#else
                taskEXIT_CRITICAL();
#endif
                if (!from_idle_task)
                {
                    vTaskDelay(1);
                }
                else
                {
                    taskYIELD();
                }
#ifdef ESP32
                taskENTER_CRITICAL(&mtx);
#else
                taskENTER_CRITICAL();
#endif
            }
#ifdef ESP32
            taskEXIT_CRITICAL(&mtx);
#else
            taskEXIT_CRITICAL();
#endif

            return true;
        }

        static void entryPoint(void *data)
        {
            Task *const self = static_cast<Task *>(data);

            bool do_stop;

#ifdef ESP32
            taskENTER_CRITICAL(&self->mtx);
#else
            taskENTER_CRITICAL();
#endif
            self->running = true;
            do_stop = self->do_stop;
#ifdef ESP32
            taskEXIT_CRITICAL(&self->mtx);
#else
            taskEXIT_CRITICAL();
#endif

            while (!do_stop && static_cast<T *>(self)->run())
            {
#ifdef ESP32
                taskENTER_CRITICAL(&self->mtx);
#else
                taskENTER_CRITICAL();
#endif
                do_stop = self->do_stop;
#ifdef ESP32
                taskEXIT_CRITICAL(&self->mtx);
#else
                taskEXIT_CRITICAL();
#endif
            }
#ifdef ESP32
            taskENTER_CRITICAL(&self->mtx);
#else
            taskENTER_CRITICAL();
#endif
            self->do_stop = false;
            self->running = false;
#ifdef ESP32
            taskEXIT_CRITICAL(&self->mtx);
#else
            taskEXIT_CRITICAL();
#endif

            const TaskHandle_t handle_copy = self->handle;
            self->handle = nullptr;

            vTaskDelete(handle_copy);
        }

        volatile bool running;
        volatile bool do_stop;
        TaskHandle_t handle;
        const char* name;
        BaseType_t higher_priority_task_woken;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StackType_t stack[STACK_SIZE / sizeof(StackType_t)];
        StaticTask_t state;
#endif
#ifdef ESP32
        portMUX_TYPE mtx;
#endif
    };
}
#endif // __FRT_TASK_H__