#ifdef ESP32
#include "UDPStream.h"
#include <stdarg.h>

UDPStream::UDPStream(int connLedPin)
{
    m_conn_led_pin = connLedPin < 0 ? LED_BUILTIN : connLedPin;
    pinMode(m_conn_led_pin, OUTPUT);

    m_packet_queue = xQueueCreate(10, sizeof(AsyncUDPPacket));
}

UDPStream::~UDPStream()
{
    vQueueDelete(m_packet_queue);
}

int UDPStream::available()
{
    return uxQueueMessagesWaiting(m_packet_queue);
}

int UDPStream::read()
{
    AsyncUDPPacket *packet;
    if (xQueueReceive(m_packet_queue, packet, (TickType_t)10))
    {
        return packet->read();
    }
    else
    {
        return 0;
    }
}

size_t UDPStream::readBytes(char *buffer, size_t length)
{
    AsyncUDPPacket *packet;
    if (xQueueReceive(m_packet_queue, packet, (TickType_t)10))
    {
        packet->readBytes(buffer, length);
        return packet->length();
    }
    else
    {
        return 0;
    }
}

int UDPStream::peek()
{
    AsyncUDPPacket *packet;
    if (xQueuePeek(m_packet_queue, packet, (TickType_t)10))
    {
        return packet->length();
    }
    else
    {
        return 0;
    }
}

size_t UDPStream::write(uint8_t ch)
{
    return m_udp.write(ch);
}

size_t UDPStream::write(const uint8_t *buffer, size_t size)
{
    return m_udp.write(buffer, size);
}

void UDPStream::flush()
{
}

void UDPStream::begin(const char *ssid, const char *password, const char *serverAddress, const uint16_t serverPort)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        error_handler();
    }

    Serial.printf("Connected to WiFi with address %s\n", WiFi.localIP().toString().c_str());

    m_server_ip.fromString(serverAddress);

    if (m_udp.connect(m_server_ip, serverPort))
    {
        m_udp.onPacket([](void *arg, AsyncUDPPacket &packet)
                       {
                           UDPStream *stream = (UDPStream *)arg;

                           if (stream->m_packet_queue != 0)
                           {
                               xQueueSend(stream->m_packet_queue, &packet, 0);
                           } },
                       this);

        digitalWrite(m_conn_led_pin, HIGH);
    }
    else
    {
        error_handler();
    }

    Serial.printf("Connected to UDP server with address %s on port %d\n", m_server_ip.toString().c_str(), serverPort);
}

void UDPStream::error_handler()
{
    noInterrupts();

    while (1)
    {
        digitalWrite(m_conn_led_pin, HIGH);
        delay(100);
        digitalWrite(m_conn_led_pin, LOW);
        delay(100);
    }
}
#endif