#ifndef __FRT_TASK_H__
#define __FRT_TASK_H__

#include "frt.h"

#define FLAG_TASK 0x00000001

namespace frt
{
    // TODO: This should work in theory. However vertain FreeRTOS implementation do not define enough memory as configMINIMAL_STACK_SIZE, 1024 is a good guess!
    // template <typename T, unsigned int STACK_SIZE_BYTES = configMINIMAL_STACK_SIZE * sizeof(StackType_t)>
    template <typename T, unsigned int STACK_SIZE_BYTES = 1024>
    class Task
    {
    public:
        Task() : m_running(false),
                 m_do_stop(false),
                 m_handle(nullptr),
                 m_name(""),
                 m_higher_priority_task_woken(pdFALSE)
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
            this->m_name = name;

            if (priority >= configMAX_PRIORITIES)
            {
                priority = configMAX_PRIORITIES - 1;
            }

#if configSUPPORT_STATIC_ALLOCATION > 0
            m_handle = xTaskCreateStatic(
                entryPoint,
                name,
                STACK_SIZE_BYTES / sizeof(StackType_t),
                this,
                priority,
                m_stack,
                &m_state);
            return m_handle;
#else
            return xTaskCreate(
                       entryPoint,
                       name,
                       STACK_SIZE_BYTES / sizeof(StackType_t),
                       this,
                       priority,
                       &m_handle) == pdPASS;
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
            res = m_running;
            FRT_CRITICAL_EXIT();

            return res;
        }

        unsigned int getUsedStackSize() const
        {
            return STACK_SIZE_BYTES - getRemainingStackSize();
        }

        unsigned int getRemainingStackSize() const
        {
            return uxTaskGetStackHighWaterMark(m_handle) * sizeof(StackType_t);
        }

        void post()
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                vTaskNotifyGiveFromISR(m_handle, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                xTaskNotifyGive(m_handle);
            }
        }

        const TaskHandle_t *handle() const
        {
            return &m_handle;
        }

    protected:
        virtual void init() {}
        virtual bool run() = 0;

        void yield()
        {
            taskYIELD();
        }

        void msleep(unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);

            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                vTaskDelay(max(1U, (unsigned int)ticks));
            }
        }

        void msleep(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                vTaskDelay(max(1U, (unsigned int)ticks));
            }
        }

        bool wait()
        {
            return ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        }

        bool wait(unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);

            return ulTaskNotifyTake(pdFALSE, max(1U, (unsigned int)ticks));
        }

        bool wait(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            uint32_t notifiedValue;
            if (ulTaskNotifyTake(pdFALSE, max(1U, (unsigned int)ticks)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

    private:
        bool stop(bool from_idle_task)
        {
            if (!m_handle)
            {
                return false;
            }

            FRT_CRITICAL_ENTER();
            m_do_stop = true;

            while (m_running)
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
                self->m_running = true;
                do_stop = self->m_do_stop;
                FRT_CRITICAL_EXIT();
            }

            static_cast<T *>(self)->init();

            while (!do_stop && static_cast<T *>(self)->run())
            {
                {
                    FRT_CRITICAL_ENTER();
                    do_stop = self->m_do_stop;
                    FRT_CRITICAL_EXIT();
                }
            }

            {
                FRT_CRITICAL_ENTER();
                self->m_do_stop = false;
                self->m_running = false;
                FRT_CRITICAL_EXIT();
            }

            const TaskHandle_t handle_copy = self->m_handle;
            self->m_handle = nullptr;

            vTaskDelete(handle_copy);
        }

        volatile bool m_running;
        volatile bool m_do_stop;
        TaskHandle_t m_handle;
        const char *m_name;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StackType_t m_stack[STACK_SIZE_BYTES / sizeof(StackType_t)];
        StaticTask_t m_state;
#endif
    };
}
#endif // __FRT_TASK_H__