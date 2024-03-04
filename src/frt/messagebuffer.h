#ifndef __FRT_MESSAGEBUFFER_H__
#define __FRT_MESSAGEBUFFER_H__

#include <functional>

#include "frt.h"

namespace frt
{
    template <unsigned int BUFFER_SIZE = 512>
    class MessageBuffer
    {
        typedef std::function<void(MessageBufferHandle_t, BaseType_t, BaseType_t *const)> MessageBufferCallback;

    public:
        MessageBuffer() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                              xMessageBufferCreateStatic(BUFFER_SIZE, buffer, &bufferStruct)
#else
                              xMessageBufferCreate(BUFFER_SIZE)
#endif
                          )
        {
        }

        ~MessageBuffer()
        {
            vMessageBufferDelete(handle);
        }

        explicit MessageBuffer(const MessageBuffer &other) = delete;
        MessageBuffer &operator=(const MessageBuffer &other) = delete;

        unsigned int getFillLevel() const
        {
#if defined(NRF52) || defined(NRF52840_XXAA)
            return BUFFER_SIZE - xMessageBufferSpaceAvailable(handle);
#else
            return BUFFER_SIZE - xMessageBufferSpacesAvailable(handle);
#endif
        }

        unsigned int size() const
        {
            return BUFFER_SIZE;
        }

        bool send(const uint8_t *data, size_t len)
        {
            size_t xBytesSent;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesSent = xMessageBufferSendFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesSent = xMessageBufferSend(handle, (void *)data, len, portMAX_DELAY);

            return (xBytesSent == len);
        }

        bool send(const uint8_t *data, size_t len, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);

            size_t xBytesSent;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesSent = xMessageBufferSendFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesSent = xMessageBufferSend(handle, (void *)data, len, max(1U, (unsigned int)ticks));

            return (xBytesSent == len);
        }

        bool send(const uint8_t *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            size_t xBytesSent;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesSent = xMessageBufferSendFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesSent = xMessageBufferSend(handle, (void *)data, len, max(1U, (unsigned int)ticks));

            if (xBytesSent == len)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        size_t receive(uint8_t *data, size_t len)
        {
            size_t xBytesReceived;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesReceived = xMessageBufferReceiveFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesReceived = xMessageBufferReceive(handle, (void *)data, len, portMAX_DELAY);

            return xBytesReceived;
        }

        size_t receive(uint8_t *data, size_t len, unsigned int msecs)
        {
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            size_t xBytesReceived;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesReceived = xMessageBufferReceiveFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesReceived = xMessageBufferReceive(handle, (void *)data, len, max(1U, (unsigned int)ticks));

            return xBytesReceived;
        }

        size_t receive(uint8_t *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = pdMS_TO_TICKS(msecs);
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);
            size_t xBytesReceived;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesReceived = xMessageBufferReceiveFromISR(handle, (void *)data, len, &taskWoken);

                detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesReceived = xMessageBufferReceive(handle, (void *)data, len, max(1U, (unsigned int)ticks));

            if (xBytesReceived > 0)
                remainder = 0;

            return xBytesReceived;
        }

    private:
        MessageBufferHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        uint8_t buffer[BUFFER_SIZE];
        StaticMessageBuffer_t bufferStruct;
#endif
    };
}

#endif // __FRT_MESSAGEBUFFER_H__