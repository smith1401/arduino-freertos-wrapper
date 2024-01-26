#ifndef __RTTSTREAM_H__
#define __RTTSTREAM_H__

#include <Arduino.h>

#if defined(STM32) || defined(NRF52) || defined(NRF52840_XXAA)

class RTTStream : public Stream
{
public:
    RTTStream();
    ~RTTStream();

    /* mode */
    void blockUpBufferFull();   /* block if buffer to host is full */
    void skipUpBufferFull();    /* skip if buffer to host is full */
    void trimUpBufferFull();    /* trim if buffer to host is full */
    void blockDownBufferFull(); /* block if buffer from host is full */
    void skipDownBufferFull();  /* skip if buffer from host is full */
    void trimDownBufferFull();  /* trim if buffer from host is full */

    /* echo */
    void echo();   /* echo characters received from host */
    void noEcho(); /* do not echo characters received from host, default */

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
    int printf(const char *format, ...);

    /* compatibility with Serial */
    void begin(unsigned long);
    operator bool();

private:
    bool echo_on = false;
};

#endif

#endif // __RTTSTREAM_H__