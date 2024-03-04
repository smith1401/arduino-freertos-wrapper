#ifndef __UDPSTREAM_H__
#define __UDPSTREAM_H__

#include <Arduino.h>

#ifdef ESP32
#include <WiFi.h>
#include <AsyncUDP.h>
#include <CircularBuffer.hpp>

#include "frt/frt.h"
#include "frt/log.h"

class UDPStream : public Stream
{
public:
    UDPStream(int connLedPin = -1);
    ~UDPStream();

    /* reading */
    virtual int available() override;
    virtual int read() override;
    virtual size_t readBytes(char *buffer, size_t length) override;
    virtual String readString() override;
    virtual int peek() override;

    /* writing */
    virtual size_t write(uint8_t ch) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual void flush() override;
    using Print::write;
    template <typename... Args>
    void printf(const char *f, Args... args)
    {
        m_udp.printf(f, args...);
    }

    /* compatibility with Serial */
    void begin(const char *ssid, const char *password, const char *serverAddress, const uint16_t serverPort = 9999);
    operator bool();

private:
    void error_handler();
    uint8_t m_conn_led_pin;
    IPAddress m_server_ip;
    AsyncUDP m_udp;
    // frt::Queue<AsyncUDPPacket *> m_packet_queue;
    // AsyncUDPPacket *m_current_packet;
    CircularBuffer<uint8_t, 1024> m_receive_buffer;
    // QueueHandle_t m_packet_queue;
};

#endif

#endif // __UDPSTREAM_H__