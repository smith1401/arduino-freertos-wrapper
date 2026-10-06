#ifndef __FRT_MUTEX_H__
#define __FRT_MUTEX_H__

#include "frt.h"

namespace frt
{
    /**
     *  Mutex with priority inheritance. Must not be used from an ISR.
     *
     *  Satisfies the C++ Lockable requirements, so it can be used with
     *  frt::LockGuard as well as std::lock_guard / std::unique_lock.
     */
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
            configASSERT(handle);
        }

        ~Mutex()
        {
            vSemaphoreDelete(handle);
        }

        Mutex(const Mutex &other) = delete;
        Mutex &operator=(const Mutex &other) = delete;

        /** Block until the mutex is acquired. */
        void lock()
        {
            configASSERT(!FRT_IS_ISR());
            // Retry: a blocking take can be aborted (xTaskAbortDelay), and
            // lock() must not return without owning the mutex.
            while (xSemaphoreTake(handle, portMAX_DELAY) != pdTRUE)
            {
            }
        }

        /**
         *  Try to acquire the mutex within the given time.
         *  @return true if the mutex was acquired.
         */
        bool lock(unsigned int msecs)
        {
            if (FRT_IS_ISR())
                return false;

            return xSemaphoreTake(handle, detail::msToTicks(msecs)) == pdTRUE;
        }

        /** Acquire the mutex if it is free, never blocks. */
        bool try_lock()
        {
            return lock(0);
        }

        void unlock()
        {
            xSemaphoreGive(handle);
        }

        /** @return true if the calling task holds the mutex. */
        bool isLockedByCurrentTask() const
        {
#if (INCLUDE_xSemaphoreGetMutexHolder == 1)
            return xSemaphoreGetMutexHolder(handle) == xTaskGetCurrentTaskHandle();
#else
            return false;
#endif
        }

    private:
        SemaphoreHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticSemaphore_t buffer;
#endif
    };

#if (configUSE_RECURSIVE_MUTEXES == 1)
    /**
     *  Mutex that can be locked several times by the task owning it. It must
     *  be unlocked as many times as it was locked. Must not be used from an ISR.
     */
    class RecursiveMutex final
    {
    public:
        RecursiveMutex() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                               xSemaphoreCreateRecursiveMutexStatic(&buffer)
#else
                               xSemaphoreCreateRecursiveMutex()
#endif
                           )
        {
            configASSERT(handle);
        }

        ~RecursiveMutex()
        {
            vSemaphoreDelete(handle);
        }

        RecursiveMutex(const RecursiveMutex &other) = delete;
        RecursiveMutex &operator=(const RecursiveMutex &other) = delete;

        void lock()
        {
            configASSERT(!FRT_IS_ISR());
            while (xSemaphoreTakeRecursive(handle, portMAX_DELAY) != pdTRUE)
            {
            }
        }

        bool lock(unsigned int msecs)
        {
            if (FRT_IS_ISR())
                return false;

            return xSemaphoreTakeRecursive(handle, detail::msToTicks(msecs)) == pdTRUE;
        }

        bool try_lock()
        {
            return lock(0);
        }

        void unlock()
        {
            xSemaphoreGiveRecursive(handle);
        }

    private:
        SemaphoreHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticSemaphore_t buffer;
#endif
    };
#endif

    /**
     *  Binary or counting semaphore. post() may be called from an ISR,
     *  wait() may not.
     */
    class Semaphore final
    {
    public:
        enum class Type
        {
            BINARY,
            COUNTING
        };

        /**
         *  @param type BINARY (default) or COUNTING
         *  @param maxCount maximum count of a counting semaphore
         *  @param initialCount initial count of a counting semaphore
         */
        Semaphore(Type type = Type::BINARY,
                  UBaseType_t maxCount = static_cast<UBaseType_t>(-1),
                  UBaseType_t initialCount = 0) : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                                                      type == Type::BINARY
                                                          ? xSemaphoreCreateBinaryStatic(&buffer)
                                                          : xSemaphoreCreateCountingStatic(maxCount, initialCount, &buffer)
#else
                                                      type == Type::BINARY
                                                          ? xSemaphoreCreateBinary()
                                                          : xSemaphoreCreateCounting(maxCount, initialCount)
#endif
                                                  )
        {
            configASSERT(handle);
        }

        ~Semaphore()
        {
            vSemaphoreDelete(handle);
        }

        Semaphore(const Semaphore &other) = delete;
        Semaphore &operator=(const Semaphore &other) = delete;

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

        bool post()
        {
            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                const bool success = xSemaphoreGiveFromISR(handle, &taskWoken) == pdTRUE;

                if (success)
                    detail::yieldFromIsr(taskWoken);

                return success;
            }

            return xSemaphoreGive(handle) == pdTRUE;
        }

        /** Current count (0 or 1 for a binary semaphore). */
        unsigned int count() const
        {
            if (FRT_IS_ISR())
                return uxQueueMessagesWaitingFromISR(reinterpret_cast<QueueHandle_t>(handle));
            else
                return uxSemaphoreGetCount(handle);
        }

    private:
        bool waitTicks(TickType_t ticks)
        {
            // Do not allow taking semaphores inside an ISR context
            if (FRT_IS_ISR())
                return false;

            return xSemaphoreTake(handle, ticks) == pdTRUE;
        }

        SemaphoreHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        StaticSemaphore_t buffer;
#endif
    };

    /**
     *  Scope based lock for any frt mutex type.
     *
     *  @code
     *  frt::Mutex m;
     *  {
     *      frt::LockGuard lock(m);  // or frt::LockGuardT<frt::RecursiveMutex>
     *      ...
     *  }
     *  @endcode
     */
    template <typename M>
    class LockGuardT
    {
    public:
        /**
         *  @post The mutex is locked.
         *  @note There is an infinite timeout for acquiring the lock.
         */
        explicit LockGuardT(M &m) : mutex(&m)
        {
            mutex->lock();
        }

        LockGuardT(const LockGuardT &other) = delete;
        LockGuardT &operator=(const LockGuardT &other) = delete;

        /** @post The mutex is unlocked. */
        ~LockGuardT()
        {
            mutex->unlock();
        }

    private:
        M *mutex;
    };

    typedef LockGuardT<Mutex> LockGuard;
}
#endif // __FRT_MUTEX_H__
