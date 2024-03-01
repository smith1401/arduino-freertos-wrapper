#ifdef ESP32
#include "UDPStream.h"
#include <stdarg.h>

UDPStream::UDPStream(int connLedPin)
{
    m_conn_led_pin = connLedPin < 0 ? LED_BUILTIN : connLedPin;
    pinMode(m_conn_led_pin, OUTPUT);
    // m_packet_queue = xQueueCreate(10, sizeof(AsyncUDPPacket));
}

UDPStream::~UDPStream()
{
    // vQueueDelete(m_packet_queue);
}

int UDPStream::available()
{
    return m_receive_buffer.size();
    // return m_packet_queue.available();
    // return uxQueueMessagesWaiting(m_packet_queue);
}

int UDPStream::read()
{
    return m_receive_buffer.shift();

    // AsyncUDPPacket *packet;
    // if (m_packet_queue.pop(packet, 10))
    // // if (xQueueReceive(m_packet_queue, packet, (TickType_t)10))
    // {
    //     FRT_LOG_DEBUG("Got new message with len=%d %08x", packet->length(), packet);
    //     FRT_LOG_DEBUG("Got new message with len=%d %08x", m_current_packet->length(), m_current_packet);
    //     return -1;
    //     // return packet->read();
    // }
    // else
    // {
    //     return 0;
    // }
}

size_t UDPStream::readBytes(char *buffer, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++)
    {
        if (m_receive_buffer.isEmpty())
            break;

        buffer[i] = m_receive_buffer.shift();
    }

    return i + 1;

    // AsyncUDPPacket *packet;
    // if (m_packet_queue.pop(packet, 10))
    // // if (xQueueReceive(m_packet_queue, packet, (TickType_t)10))
    // {
    //     packet->readBytes(buffer, length);
    //     return packet->length();
    // }
    // else
    // {
    //     return 0;
    // }
}

String UDPStream::readString()
{
    String str;
    char c;

    while (m_receive_buffer.available() && (c = (char)m_receive_buffer.shift()) != '\0')
    {
        str += c;
    }

    return str;
}

int UDPStream::peek()
{
    return m_receive_buffer.first();

    // AsyncUDPPacket *packet;
    // if (m_packet_queue.peek(packet, 10))
    // // if (xQueuePeek(m_packet_queue, packet, (TickType_t)10))
    // {
    //     return packet->length();
    // }
    // else
    // {
    //     return 0;
    // }
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
    m_receive_buffer.clear();
}

void UDPStream::begin(const char *ssid, const char *password, const char *serverAddress, const uint16_t serverPort)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        error_handler();
    }

    FRT_LOG_INFO("Connected to WiFi with address %s", WiFi.localIP().toString().c_str());

    m_server_ip.fromString(serverAddress);

    if (m_udp.connect(m_server_ip, serverPort))
    {
        m_udp.onPacket([](void *arg, AsyncUDPPacket &packet)
                       {
                           UDPStream *stream = (UDPStream *)arg;

                           for (size_t i = 0; i < packet.length(); i++)
                           {
                               char c = (char)packet.read();
                            //    Serial.print(c);
                               stream->m_receive_buffer.push(c);
                           }

                           //    stream->m_packet_queue.push(&packet, portMAX_DELAY);

                       },
                       this);

        // digitalWrite(m_conn_led_pin, HIGH);
    }
    else
    {
        error_handler();
    }

    FRT_LOG_INFO("Connected to UDP server with address %s on port %d", m_server_ip.toString().c_str(), serverPort);
}

UDPStream::operator bool()
{
    return m_udp.connected();
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