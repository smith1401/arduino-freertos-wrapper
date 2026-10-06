#ifndef FRT_HOST_ARDUINO_H
#define FRT_HOST_ARDUINO_H

/*
 * Minimal stand-in for the Arduino core, just enough to build the frt
 * wrapper against the FreeRTOS POSIX port on a PC.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>

class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t n = 0;
        while (size--)
            n += write(*buffer++);
        return n;
    }
    size_t write(const char *buffer, size_t size)
    {
        return write(reinterpret_cast<const uint8_t *>(buffer), size);
    }
};

class Stream : public Print
{
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
};

/* Collects everything written to it, used to check the logger. */
class StringStream : public Stream
{
public:
    std::string data;
    using Print::write;
    size_t write(uint8_t c) override
    {
        data.push_back(static_cast<char>(c));
        return 1;
    }
};

#endif
