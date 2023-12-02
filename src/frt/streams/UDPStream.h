#ifndef __UDPSTREAM_H__
#define __UDPSTREAM_H__

#include <Arduino.h>

#ifdef ESP32
#include <WiFi.h>
#include <AsyncUDP.h>

class UDPStream : public Stream
{
public:
    UDPStream(int connLedPin = -1);
    ~UDPStream();

    /* reading */
    virtual int available();
    virtual int read();
    virtual size_t readBytes(char *buffer, size_t length);
    virtual int peek();

    /* writing */
    virtual size_t write(uint8_t ch);
    virtual size_t write(const uint8_t *buffer, size_t size);
    virtual void flush();
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
    QueueHandle_t m_packet_queue;
};

#endif

#endif // __UDPSTREAM_H__