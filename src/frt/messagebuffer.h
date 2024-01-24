#ifndef __FRT_MESSAGEBUFFER_H__
#define __FRT_MESSAGEBUFFER_H__

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
#ifdef NRF52
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

            size_t xBytesSent = xMessageBufferSend(handle, (void *)data, len, portMAX_DELAY);
            return (xBytesSent == len);
        }

        bool send(const uint8_t *data, size_t len, unsigned int msecs)
        {

            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            size_t xBytesSent = xMessageBufferSend(handle, (void *)data, len, max(1U, (unsigned int)ticks));
            return (xBytesSent == len);
        }

        bool send(const uint8_t *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {

            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xMessageBufferSend(handle, (void *)data, len, max(1U, (unsigned int)ticks)) == len)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void prepareSendFromInterrupt()
        {
            higher_priority_task_woken_from_send = 0;
        }

        bool sendFromInterrupt(const uint8_t *data, size_t len)
        {
            size_t xBytesSent = xMessageBufferSendFromISR(handle, (void *)data, len, &higher_priority_task_woken_from_send);
            return xBytesSent == len;
        }

        void finalizeSendFromInterrupt() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (higher_priority_task_woken_from_send)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(higher_priority_task_woken_from_send);
#endif
        }

        size_t receive(uint8_t *data, size_t len)
        {
            return xMessageBufferReceive(handle, (void *)data, len, portMAX_DELAY);
        }

        size_t receive(uint8_t *data, size_t len, unsigned int msecs)
        {
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;

            return xMessageBufferReceive(handle, (void *)data, len, max(1U, (unsigned int)ticks));
        }

        size_t receive(uint8_t *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {
            msecs += remainder;
            const TickType_t ticks = msecs / portTICK_PERIOD_MS;
            remainder = msecs % portTICK_PERIOD_MS * static_cast<bool>(ticks);

            if (xMessageBufferReceive(handle, (void *)data, len, max(1U, (unsigned int)ticks)) > 0)
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        void prepareReceiveFromInterrupt()
        {
            higher_priority_task_woken_from_receive = 0;
        }

        size_t receiveFromInterrupt(uint8_t *data, size_t len)
        {
            return xMessageBufferReceiveFromISR(handle, (void *)data, len, &higher_priority_task_woken_from_receive);
        }

        void finalizeReceiveFromInterrupt() __attribute__((always_inline))
        {
#if defined(ESP32)
            if (higher_priority_task_woken_from_receive)
            {
                detail::yieldFromIsr();
            }
#else
            detail::yieldFromIsr(higher_priority_task_woken_from_receive);
#endif
        }

    private:
        MessageBufferHandle_t handle;
        BaseType_t higher_priority_task_woken_from_send;
        BaseType_t higher_priority_task_woken_from_receive;
#if configSUPPORT_STATIC_ALLOCATION > 0
        uint8_t buffer[BUFFER_SIZE];
        StaticMessageBuffer_t bufferStruct;
#endif
    };

}

#endif // __FRT_MESSAGEBUFFER_H__