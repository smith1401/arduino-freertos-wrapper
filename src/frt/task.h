#ifndef __FRT_TASK_H__
#define __FRT_TASK_H__

#include "frt.h"
#include "log.h"
#include "manager.h"

#define FLAG_TASK 0x00000001

namespace frt
{
    /**
     *  Type erased base of all frt tasks, used by the task registry.
     */
    class ITask
    {
    protected:
        TaskHandle_t m_handle;
        char m_name[configMAX_TASK_NAME_LEN];
        volatile bool m_running;
        volatile bool m_do_stop;

    public:
        ITask() : m_handle(nullptr),
                  m_running(false),
                  m_do_stop(false)
        {
            m_name[0] = '\0';
        }

        virtual ~ITask() {}

        /**
         *  Called by frt::task::suspendOtherTasks() / deleteOtherTasks() before
         *  the task is suspended or deleted. Override it to bring hardware
         *  into a safe state.
         */
        virtual void gracefulShutdown() {}

        /** @deprecated use getHandle() */
        const TaskHandle_t *handle() const
        {
            return &m_handle;
        }

        TaskHandle_t getHandle() const
        {
            return m_handle;
        }

        const char *name() const
        {
            return m_name;
        }

        bool isRunning() const
        {
            CriticalSection cs;
            return m_running;
        }

        /**
         *  Mark the task as gone after it was deleted behind our back
         *  (frt::task::deleteOtherTasks()).
         */
        void markDeleted()
        {
            CriticalSection cs;
            m_handle = nullptr;
            m_running = false;
            m_do_stop = false;
        }
    };

    /**
     *  Task base class. Derive from it using CRTP and implement run():
     *
     *  @code
     *  class Blinker : public frt::Task<Blinker, 1024>
     *  {
     *  public:
     *      bool run()
     *      {
     *          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
     *          msleep(500);
     *          return true;   // false ends the task
     *      }
     *  };
     *
     *  Blinker blinker;
     *  blinker.start(1, "blinker");
     *  @endcode
     *
     *  init() is called once in the context of the new task before the first
     *  run(). run() is called repeatedly until it returns false or stop() is
     *  called.
     *
     *  If you derive further, call stop() in the destructor of the most
     *  derived class: the base class destructor runs after the derived parts
     *  are already gone.
     *
     *  @tparam T The derived class
     *  @tparam STACK_SIZE_BYTES Stack size in bytes
     */
    // configMINIMAL_STACK_SIZE is too small on many ports, 1024 is a good guess.
    template <typename T, unsigned int STACK_SIZE_BYTES = 1024>
    class Task : public ITask
    {
        static_assert(STACK_SIZE_BYTES >= 256, "frt::Task stack size is too small");

    public:
        static const int NO_AFFINITY = -1;

        Task()
        {
        }

        virtual ~Task()
        {
            stop();
        }

        Task(const Task &other) = delete;
        Task &operator=(const Task &other) = delete;

        /**
         *  Create the FreeRTOS task. Can be called before the scheduler is
         *  started.
         *
         *  @param priority Task priority, clamped to configMAX_PRIORITIES - 1
         *  @param name Task name, copied (max configMAX_TASK_NAME_LEN - 1 chars)
         *  @param core CPU core to pin the task to (ESP32 only, ignored elsewhere)
         *  @return task handle, nullptr if the task could not be created or is
         *          already running
         */
        TaskHandle_t start(unsigned char priority = 0, const char *name = "", int core = NO_AFFINITY)
        {
            if (FRT_IS_ISR())
                return nullptr;

            // Reap a task that ended on its own (run() returned false)
            reap();

            if (m_handle != nullptr)
                return nullptr;

            strncpy(m_name, name ? name : "", sizeof(m_name) - 1);
            m_name[sizeof(m_name) - 1] = '\0';

            if (priority >= configMAX_PRIORITIES)
                priority = configMAX_PRIORITIES - 1;

            {
                CriticalSection cs;
                m_do_stop = false;
                m_running = false;
            }

            // Register before creating, the task might run (and end) right away
            Manager::getInstance()->addTask(this);

            TaskHandle_t handle = nullptr;

#if defined(ESP32)
            const BaseType_t affinity = core < 0 ? tskNO_AFFINITY : static_cast<BaseType_t>(core);
#if configSUPPORT_STATIC_ALLOCATION > 0
            handle = xTaskCreateStaticPinnedToCore(
                entryPoint,
                m_name,
                STACK_SIZE_BYTES / sizeof(StackType_t),
                this,
                priority,
                m_stack,
                &m_state,
                affinity);
#else
            if (xTaskCreatePinnedToCore(
                    entryPoint,
                    m_name,
                    STACK_SIZE_BYTES / sizeof(StackType_t),
                    this,
                    priority,
                    &handle,
                    affinity) != pdPASS)
            {
                handle = nullptr;
            }
#endif
#else
            FRT_UNUSED(core);
#if configSUPPORT_STATIC_ALLOCATION > 0
            handle = xTaskCreateStatic(
                entryPoint,
                m_name,
                STACK_SIZE_BYTES / sizeof(StackType_t),
                this,
                priority,
                m_stack,
                &m_state);
#else
            if (xTaskCreate(
                    entryPoint,
                    m_name,
                    STACK_SIZE_BYTES / sizeof(StackType_t),
                    this,
                    priority,
                    &handle) != pdPASS)
            {
                handle = nullptr;
            }
#endif
#endif

            if (handle == nullptr)
            {
                Manager::getInstance()->removeTask(this);
                return nullptr;
            }

            {
                CriticalSection cs;
                // The task may have finished already, only set if still unset
                if (m_handle == nullptr)
                    m_handle = handle;
            }

            return handle;
        }

        /**
         *  Ask the task to stop and wait until run() returned. If the task is
         *  blocked in a frt call (sleep, queue, ...) the call is aborted, so
         *  check return values in run().
         *
         *  Called from the task itself this only requests the stop, the task
         *  ends after the current run() returns.
         */
        bool stop()
        {
            return stop(false);
        }

        /**
         *  stop() variant for the idle hook (which must not block).
         */
        bool stopFromIdleTask()
        {
            return stop(true);
        }

        unsigned int getUsedStackSize() const
        {
            return STACK_SIZE_BYTES - getRemainingStackSize();
        }

        /** Minimum amount of free stack (in bytes) since the task started. */
        unsigned int getRemainingStackSize() const
        {
            if (m_handle == nullptr)
                return STACK_SIZE_BYTES;

            return uxTaskGetStackHighWaterMark(m_handle) * sizeof(StackType_t);
        }

        static constexpr unsigned int getStackSize() { return STACK_SIZE_BYTES; }

        /**
         *  Wake up the task if it waits in wait(). Can be called from an ISR.
         *  Each post() is counted, so the matching number of wait() calls
         *  return immediately.
         */
        void post()
        {
            TaskHandle_t handle = m_handle;

            if (handle == nullptr)
                return;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                vTaskNotifyGiveFromISR(handle, &taskWoken);
                detail::yieldFromIsr(taskWoken);
            }
            else
            {
                xTaskNotifyGive(handle);
            }
        }

        /**
         *  Send a notification value to the task. Can be called from an ISR.
         *  The task receives it with waitNotification().
         *
         *  Do not mix with post()/wait(): both use the same notification.
         */
        bool notify(uint32_t value, eNotifyAction action = eSetBits)
        {
            TaskHandle_t handle = m_handle;

            if (handle == nullptr)
                return false;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xTaskNotifyFromISR(handle, value, action, &taskWoken) == pdPASS;
                detail::yieldFromIsr(taskWoken);
                return success;
            }

            return xTaskNotify(handle, value, action) == pdPASS;
        }

#if (INCLUDE_uxTaskPriorityGet == 1)
        UBaseType_t getPriority() const
        {
            return m_handle ? uxTaskPriorityGet(m_handle) : 0;
        }
#endif

#if (INCLUDE_vTaskPrioritySet == 1)
        void setPriority(UBaseType_t priority)
        {
            if (m_handle)
                vTaskPrioritySet(m_handle, priority >= configMAX_PRIORITIES ? configMAX_PRIORITIES - 1 : priority);
        }
#endif

#if (INCLUDE_vTaskSuspend == 1)
        void suspend()
        {
            if (m_handle)
                vTaskSuspend(m_handle);
        }

        void resume()
        {
            TaskHandle_t handle = m_handle;

            if (handle == nullptr)
                return;

            if (FRT_IS_ISR())
            {
#if (INCLUDE_xTaskResumeFromISR == 1)
                BaseType_t yieldRequired = xTaskResumeFromISR(handle);
                detail::yieldFromIsr(yieldRequired);
#endif
            }
            else
                vTaskResume(handle);
        }
#endif

    protected:
        virtual void init() {}
        virtual bool run() = 0;

        /** true if stop() was requested, useful in long running run() methods. */
        bool shouldStop() const
        {
            CriticalSection cs;
            return m_do_stop;
        }

        void yield()
        {
            taskYIELD();
        }

        /** Sleep for at least one tick. */
        void msleep(unsigned int msecs)
        {
            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                const TickType_t ticks = detail::msToTicks(msecs);
                vTaskDelay(ticks > 0 ? ticks : 1);
            }
        }

        /**
         *  Sleep, carrying the part of msecs that is shorter than a tick over
         *  to the next call in remainder (start with remainder = 0).
         */
        void msleep(unsigned int msecs, unsigned int &remainder)
        {
            const TickType_t ticks = detail::msToTicksCarry(msecs, remainder);

            if (!FRT_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                vTaskDelay(ticks > 0 ? ticks : 1);
            }
        }

#if (INCLUDE_vTaskDelayUntil == 1) || (INCLUDE_xTaskDelayUntil == 1)
        /**
         *  Sleep until lastWakeTime + periodMs and update lastWakeTime. Gives
         *  a fixed execution frequency without drift. Initialize lastWakeTime
         *  with xTaskGetTickCount() once before the first call.
         *
         *  @code
         *  void init() override { m_lastWake = xTaskGetTickCount(); }
         *  bool run() override { control(); msleepUntil(m_lastWake, 10); return true; }
         *  @endcode
         */
        void msleepUntil(TickType_t &lastWakeTime, unsigned int periodMs)
        {
            const TickType_t ticks = detail::msToTicks(periodMs);
            vTaskDelayUntil(&lastWakeTime, ticks > 0 ? ticks : 1);
        }
#endif

        /** Wait for a post(). */
        bool wait(unsigned int msecs = FOREVER)
        {
            return waitTicks(detail::msToTicks(msecs));
        }

        bool wait(unsigned int msecs, unsigned int &remainder)
        {
            if (waitTicks(detail::msToTicksCarry(msecs, remainder)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        /**
         *  Wait for a notify(). The received value is stored in value.
         *
         *  @param clearOnEntry bits to clear before waiting
         *  @param clearOnExit bits to clear after a notification was received
         */
        bool waitNotification(uint32_t &value,
                              unsigned int msecs = FOREVER,
                              uint32_t clearOnEntry = 0,
                              uint32_t clearOnExit = 0xFFFFFFFFUL)
        {
            if (FRT_IS_ISR())
                return false;

            return xTaskNotifyWait(clearOnEntry, clearOnExit, &value, detail::msToTicks(msecs)) == pdTRUE;
        }

    private:
        bool waitTicks(TickType_t ticks)
        {
            if (FRT_IS_ISR())
                return false;

            return ulTaskNotifyTake(pdFALSE, ticks) > 0;
        }

        /**
         *  Delete a task that has left its run loop. Tasks do not delete
         *  themselves but suspend: deleting another task frees its resources
         *  immediately, while self deletion is deferred to the idle task and
         *  the static stack could otherwise be reused too early by start().
         */
        void reap()
        {
            TaskHandle_t handle;

            {
                CriticalSection cs;
                if (m_handle == nullptr || m_running)
                    return;

                // Never started running yet: only reap if a stop was requested
                if (!m_finished && !m_do_stop)
                    return;

                handle = m_handle;
                m_handle = nullptr;
                m_finished = false;
            }

            Manager::getInstance()->removeTask(this);

            if (handle != xTaskGetCurrentTaskHandle())
                vTaskDelete(handle);
        }

        bool stop(bool from_idle_task)
        {
            if (FRT_IS_ISR())
                return false;

            {
                CriticalSection cs;

                if (m_handle == nullptr)
                    return false;

                m_do_stop = true;

                // Stopping ourselves: run() returns to the loop which ends it
                if (m_handle == xTaskGetCurrentTaskHandle())
                    return true;
            }

            if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
            {
                while (isRunning())
                {
#if (INCLUDE_xTaskAbortDelay == 1)
                    // Kick the task out of a blocking call
                    xTaskAbortDelay(m_handle);
#endif
                    if (!from_idle_task)
                        vTaskDelay(1);
                    else
                        taskYIELD();
                }
            }

            reap();

            return true;
        }

        static void entryPoint(void *data)
        {
            Task *const self = static_cast<Task *>(data);

            bool do_stop;

            {
                CriticalSection cs;
                if (self->m_handle == nullptr)
                    self->m_handle = xTaskGetCurrentTaskHandle();
                self->m_running = true;
                do_stop = self->m_do_stop;
            }

            // Called through the base class, so init() and run() may be
            // protected or private in the derived class
            if (!do_stop)
                self->init();

            while (true)
            {
                {
                    CriticalSection cs;
                    do_stop = self->m_do_stop;
                }

                if (do_stop || !self->run())
                    break;
            }

            {
                CriticalSection cs;
                self->m_running = false;
                self->m_finished = true;
            }

            Manager::getInstance()->removeTask(self);

            // Wait to be deleted by stop() / start() / the destructor
            while (true)
            {
#if (INCLUDE_vTaskSuspend == 1)
                vTaskSuspend(NULL);
#else
                vTaskDelay(portMAX_DELAY);
#endif
            }
        }

        volatile bool m_finished = false;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StackType_t m_stack[STACK_SIZE_BYTES / sizeof(StackType_t)];
        StaticTask_t m_state;
#endif
    };

    namespace task
    {
        /**
         *  Suspend all frt tasks except the calling one (and the Arduino loop
         *  task on ESP32). gracefulShutdown() is called for each task first.
         */
        inline void suspendOtherTasks()
        {
            const TaskHandle_t self = xTaskGetCurrentTaskHandle();

            for (ITask *t : Manager::getInstance()->getTasks())
            {
                const TaskHandle_t handle = t->getHandle();

                if (handle == nullptr || handle == self)
                    continue;

                FRT_LOG_DEBUG("Task [%s] will be suspended", t->name());
                t->gracefulShutdown();
                vTaskSuspend(handle);
            }

#ifdef ESP32
            TaskHandle_t loopHandle = xTaskGetHandle("loopTask");

            if (loopHandle != NULL && loopHandle != self)
            {
                FRT_LOG_DEBUG("Task [loopTask] will be suspended");
                vTaskSuspend(loopHandle);
            }
#endif
        }

        /** Resume all frt tasks suspended by suspendOtherTasks(). */
        inline void resumeOtherTasks()
        {
            const TaskHandle_t self = xTaskGetCurrentTaskHandle();

            for (ITask *t : Manager::getInstance()->getTasks())
            {
                const TaskHandle_t handle = t->getHandle();

                if (handle == nullptr || handle == self)
                    continue;

                vTaskResume(handle);
            }

#ifdef ESP32
            TaskHandle_t loopHandle = xTaskGetHandle("loopTask");

            if (loopHandle != NULL && loopHandle != self)
            {
                FRT_LOG_DEBUG("Task [loopTask] will be resumed");
                vTaskResume(loopHandle);
            }
#endif
        }

        /**
         *  Delete all frt tasks except the calling one (and the Arduino loop
         *  task on ESP32). gracefulShutdown() is called for each task first.
         */
        inline void deleteOtherTasks()
        {
            Manager *man = Manager::getInstance();
            const TaskHandle_t self = xTaskGetCurrentTaskHandle();

            for (ITask *t : man->getTasks())
            {
                const TaskHandle_t handle = t->getHandle();

                if (handle == nullptr || handle == self)
                    continue;

                t->gracefulShutdown();
                man->removeTask(t);
                t->markDeleted();
                vTaskDelete(handle);
            }

#ifdef ESP32
            TaskHandle_t loopHandle = xTaskGetHandle("loopTask");

            if (loopHandle != NULL && loopHandle != self)
            {
                FRT_LOG_DEBUG("Task [loopTask] will be deleted");
                vTaskDelete(loopHandle);
            }
#endif
        }
    }
}
#endif // __FRT_TASK_H__
