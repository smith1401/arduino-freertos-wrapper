#ifndef __FRT_MUTEX_H__
#define __FRT_MUTEX_H__

#include "frt.h"

namespace frt
{
    class Mutex final
    {
    public:
        Mutex() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                      xSemaphoreCreateMutexStatic(&buffer)
#else
                      xSemaphoreCreateMutex()
#endif
                  )
        {
        }

        ~Mutex()
        {
            vSemaphoreDelete(handle);
        }

        explicit Mutex(const Mutex &other) = delete;
        Mutex &operator=(const Mutex &other) = delete;

        void lock()
        {
            xSemaphoreTake(handle, portMAX_DELAY);
        }

        void lock(unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            xSemaphoreTake(handle, ticks);
        }

        void unlock()
        {
            xSemaphoreGive(handle);
        }

    private:
        SemaphoreHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticSemaphore_t buffer;
#endif
    };

    class Semaphore final
    {
    public:
        enum class Type
        {
            BINARY,
            COUNTING
        };

        Semaphore(Type type = Type::BINARY) : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                                                  type == Type::BINARY
                                                      ? xSemaphoreCreateBinaryStatic(&buffer)
                                                      : xSemaphoreCreateCountingStatic(static_cast<UBaseType_t>(-1), 0, &buffer)
#else
                                                  type == Type::BINARY
                                                      ? xSemaphoreCreateBinary()
                                                      : xSemaphoreCreateCounting(static_cast<UBaseType_t>(-1), 0)
#endif
                                              )
        {
        }

        ~Semaphore()
        {
            vSemaphoreDelete(handle);
        }

        explicit Semaphore(const Semaphore &other) = delete;
        Semaphore &operator=(const Semaphore &other) = delete;

        void wait()
        {
            xSemaphoreTake(handle, portMAX_DELAY);
        }

        bool wait(unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xSemaphoreTake(handle, max(1U, (unsigned int)ticks)) == pdTRUE;
        }

        bool wait(unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xSemaphoreTake(handle, max(1U, (unsigned int)ticks)) == pdTRUE)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void post()
        {
            xSemaphoreGive(handle);
        }

        void preparePostFromInterrupt()
        {
            higher_priority_task_woken = 0;
        }

        void postFromInterrupt()
        {
            xSemaphoreGiveFromISR(handle, &higher_priority_task_woken);
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

    private:
        SemaphoreHandle_t handle;
        BaseType_t higher_priority_task_woken;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticSemaphore_t buffer;
#endif
    };

    class LockGuard
    {
    public:
        /**
         *  Create a LockGuard with a specific Mutex.
         *
         *  @post The Mutex will be locked.
         *  @note There is an infinite timeout for acquiring the Lock.
         */
        explicit LockGuard(Mutex &m) : mutex(m)
        {
            mutex.lock();
        }

        explicit LockGuard(const LockGuard &other) = delete;
        LockGuard &operator=(const LockGuard &other) = delete;

        /**
         *  Destroy a LockGuard.
         *
         *  @post The Mutex will be unlocked.
         */
        ~LockGuard()
        {
            mutex.unlock();
        }

    private:
        Mutex &mutex;
    };

}
#endif // __FRT_MUTEX_H__