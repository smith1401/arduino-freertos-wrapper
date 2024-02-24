#ifndef __FRT_TASK_H__
#define __FRT_TASK_H__

#include "frt.h"

#define FLAG_TASK 0x00000001

namespace frt
{
    // template <typename T, unsigned int STACK_SIZE = configMINIMAL_STACK_SIZE * sizeof(StackType_t)>
    template <typename T, unsigned int STACK_SIZE = 1024>
    class Task
    {
    public:
        Task() : running(false),
                 do_stop(false),
                 handle(nullptr),
                 name(""),
                 higher_priority_task_woken(pdFALSE)
        {
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
                       //    STACK_SIZE,
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

            FRT_CRITICAL_ENTER();
            res = running;
            FRT_CRITICAL_EXIT();

            return res;
        }

        unsigned int getUsedStackSize() const
        {
            return STACK_SIZE - getRemainingStackSize();
        }

        unsigned int getRemainingStackSize() const
        {
            return uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
        }

        void post()
        {
            // uint32_t start = micros();

            if (FRT_IS_ISR())
            {
                higher_priority_task_woken = pdFALSE;
                vTaskNotifyGiveFromISR(handle, &higher_priority_task_woken);
                // detail::yieldFromIsr(higher_priority_task_woken);
                // portYIELD_FROM_ISR(higher_priority_task_woken)
            }
            else
            {
                xTaskNotifyGive(handle);
            }

            // uint32_t duration = micros() - start;
            // uint32_t stop = micros();
        }

        const TaskHandle_t *taskHandle() const
        {
            return &handle;
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

            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                vTaskDelay(max(1U, (unsigned int)ticks));
            }
        }

        void msleep(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                vTaskDelay(max(1U, (unsigned int)ticks));
            }
        }

        bool wait()
        {
            // return ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            return ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
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

            uint32_t notifiedValue;
            if (ulTaskNotifyTake(pdTRUE, max(1U, (unsigned int)ticks)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

    private:
        bool stop(bool from_idle_task)
        {
            if (!handle)
            {
                return false;
            }

            FRT_CRITICAL_ENTER();
            do_stop = true;

            while (running)
            {
                FRT_CRITICAL_EXIT();
                if (!from_idle_task)
                {
                    vTaskDelay(1);
                }
                else
                {
                    taskYIELD();
                }
                FRT_CRITICAL_ENTER();
            }

            FRT_CRITICAL_EXIT();

            return true;
        }

        static void entryPoint(void *data)
        {
            Task *const self = static_cast<Task *>(data);

            bool do_stop;

            {
                FRT_CRITICAL_ENTER();
                self->running = true;
                do_stop = self->do_stop;
                FRT_CRITICAL_EXIT();
            }

            while (!do_stop && static_cast<T *>(self)->run())
            {
                {
                    FRT_CRITICAL_ENTER();
                    do_stop = self->do_stop;
                    FRT_CRITICAL_EXIT();
                }
            }

            {
                FRT_CRITICAL_ENTER();
                self->do_stop = false;
                self->running = false;
                FRT_CRITICAL_EXIT();
            }

            const TaskHandle_t handle_copy = self->handle;
            self->handle = nullptr;

            vTaskDelete(handle_copy);
        }

        volatile bool running;
        volatile bool do_stop;
        TaskHandle_t handle;
        const char *name;
        BaseType_t higher_priority_task_woken;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StackType_t stack[STACK_SIZE / sizeof(StackType_t)];
        StaticTask_t state;
#endif
    };
}
#endif // __FRT_TASK_H__