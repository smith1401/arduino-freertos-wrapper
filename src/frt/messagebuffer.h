#ifndef __FRT_MESSAGEBUFFER_H__
#define __FRT_MESSAGEBUFFER_H__

#include "frt.h"

namespace frt
{
    /**
     *  Message buffer for variable length messages.
     *
     *  Like the underlying FreeRTOS object it is only safe for a single
     *  writer and a single reader. Use a Mutex if several tasks write or
     *  read. Every message additionally needs sizeof(size_t) bytes of space
     *  for its length.
     *
     *  Timeouts are in milliseconds: 0 does not block, frt::FOREVER blocks.
     */
    template <unsigned int BUFFER_SIZE = 512>
    class MessageBuffer
    {
    public:
        MessageBuffer() : handle(
#if configSUPPORT_STATIC_ALLOCATION > 0
                              xMessageBufferCreateStatic(BUFFER_SIZE, buffer, &bufferStruct)
#else
                              xMessageBufferCreate(BUFFER_SIZE)
#endif
                          )
        {
            configASSERT(handle);
        }

        ~MessageBuffer()
        {
            vMessageBufferDelete(handle);
        }

        MessageBuffer(const MessageBuffer &other) = delete;
        MessageBuffer &operator=(const MessageBuffer &other) = delete;

        /** Number of bytes that can still be written (including length headers). */
        unsigned int availableForWrite() const
        {
#if defined(NRF52) || defined(NRF52840_XXAA)
            return xMessageBufferSpaceAvailable(handle);
#else
            return xMessageBufferSpacesAvailable(handle);
#endif
        }

        /**
         *  @deprecated This returns the free space, not the amount of data to
         *  read. Use availableForWrite() (same value) or getFillLevel().
         */
        __attribute__((deprecated("returns free space; use availableForWrite()")))
        unsigned int available() const
        {
            return availableForWrite();
        }

        /** Number of bytes in use (including length headers). */
        unsigned int getFillLevel() const
        {
            return BUFFER_SIZE - availableForWrite();
        }

        /** Length of the next message, 0 if the buffer is empty. */
        size_t nextMessageSize() const
        {
#if defined(xMessageBufferNextLengthBytes) || (tskKERNEL_VERSION_MAJOR > 10) || (tskKERNEL_VERSION_MAJOR == 10 && tskKERNEL_VERSION_MINOR >= 2)
            return xMessageBufferNextLengthBytes(handle);
#else
            return 0;
#endif
        }

        bool isEmpty() const
        {
            return xMessageBufferIsEmpty(handle) == pdTRUE;
        }

        bool isFull() const
        {
            return xMessageBufferIsFull(handle) == pdTRUE;
        }

        /** Remove all messages. Only allowed if no task is blocked on the buffer. */
        bool clear()
        {
            return xMessageBufferReset(handle) == pdPASS;
        }

        unsigned int size() const
        {
            return BUFFER_SIZE;
        }

        /** Send one message. Returns true if the complete message was written. */
        bool send(const void *data, size_t len, unsigned int msecs = FOREVER)
        {
            return sendTicks(data, len, detail::msToTicks(msecs));
        }

        bool send(const void *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {
            if (sendTicks(data, len, detail::msToTicksCarry(msecs, remainder)))
            {
                remainder = 0;
                return true;
            }

            return false;
        }

        /**
         *  Receive one message into data (capacity len). Returns the size of the
         *  message, or 0 on timeout or if the next message is larger than len.
         */
        size_t receive(void *data, size_t len, unsigned int msecs = FOREVER)
        {
            return receiveTicks(data, len, detail::msToTicks(msecs));
        }

        size_t receive(void *data, size_t len, unsigned int msecs, unsigned int &remainder)
        {
            const size_t xBytesReceived = receiveTicks(data, len, detail::msToTicksCarry(msecs, remainder));

            if (xBytesReceived > 0)
                remainder = 0;

            return xBytesReceived;
        }

    private:
        bool sendTicks(const void *data, size_t len, TickType_t ticks)
        {
            size_t xBytesSent;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesSent = xMessageBufferSendFromISR(handle, data, len, &taskWoken);

                if (xBytesSent > 0)
                    detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesSent = xMessageBufferSend(handle, data, len, ticks);

            return (xBytesSent == len);
        }

        size_t receiveTicks(void *data, size_t len, TickType_t ticks)
        {
            size_t xBytesReceived;

            if (FRT_IS_ISR())
            {
                BaseType_t taskWoken = pdFALSE;
                xBytesReceived = xMessageBufferReceiveFromISR(handle, data, len, &taskWoken);

                if (xBytesReceived > 0)
                    detail::yieldFromIsr(taskWoken);
            }
            else
                xBytesReceived = xMessageBufferReceive(handle, data, len, ticks);

            return xBytesReceived;
        }

        MessageBufferHandle_t handle;
#if configSUPPORT_STATIC_ALLOCATION > 0
        // Older FreeRTOS versions need one byte more than the buffer size.
        uint8_t buffer[BUFFER_SIZE + 1];
        StaticMessageBuffer_t bufferStruct;
#endif
    };
}

#endif // __FRT_MESSAGEBUFFER_H__
