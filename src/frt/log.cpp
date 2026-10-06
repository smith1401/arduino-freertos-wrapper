#include "log.h"

#include <stdio.h>
#include <algorithm>

using namespace frt;

static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

// Room kept free at the end of the buffer for "\r\n\0"
static const size_t LINE_END_RESERVE = 3;

// snprintf returns the length that *would* have been written, clamp it to
// what actually fits
static size_t clampedAdd(size_t size, int written, size_t capacity)
{
    if (written > 0)
        size += static_cast<size_t>(written);

    return std::min(size, capacity);
}

Log *Log::getInstance()
{
    static Log instance;
    return &instance;
}

void frt::Log::registerStream(Stream *s)
{
    LockGuard lock(mutex);
    streams.push_back(s);
}

void frt::Log::unregisterStream(Stream *s)
{
    LockGuard lock(mutex);
    streams.erase(std::remove(streams.begin(), streams.end(), s), streams.end());
}

void frt::Log::setLevel(LogLevel level)
{
    _level = level;
}

size_t Log::formatHeader(LogLevel level)
{
    const size_t capacity = sizeof(buf) - LINE_END_RESERVE;
    const unsigned long ms = static_cast<unsigned long>(xTaskGetTickCount()) * 1000UL / configTICK_RATE_HZ;
    const unsigned long minutes = ms / 1000UL / 60UL;
    const unsigned long seconds = (ms / 1000UL) % 60UL;
    const unsigned long millis_ = ms % 1000UL;

#ifdef LOG_USE_COLOR
    // Get task name formatted (no current task before the scheduler started)
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();
    const char *taskName = current ? pcTaskGetName(current) : "-";

    const int written = snprintf(buf, capacity, "%02lu:%02lu:%03lu [ %-11.11s ] %s%-5s \x1b[0m",
                                 minutes, seconds, millis_, taskName, level_colors[level], level_strings[level]);
#else
    const int written = snprintf(buf, capacity, "%02lu:%02lu:%03lu %-5s ",
                                 minutes, seconds, millis_, level_strings[level]);
#endif

    return clampedAdd(0, written, capacity - 1);
}

void Log::writeLine(size_t size)
{
    buf[size++] = '\r';
    buf[size++] = '\n';
    buf[size] = '\0';

    for (Stream *s : streams)
        s->write(reinterpret_cast<const uint8_t *>(buf), size);
}

void Log::log(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(level, file, line, fmt, ap);
    va_end(ap);
}

void Log::vlog(LogLevel level, const char *file, int line, const char *fmt, va_list ap)
{
    FRT_UNUSED(file);
    FRT_UNUSED(line);

    if (!isEnabled(level) || FRT_IS_ISR())
        return;

    LockGuard lock(mutex);

    const size_t capacity = sizeof(buf) - LINE_END_RESERVE;
    size_t size = formatHeader(level);
    size = clampedAdd(size, vsnprintf(buf + size, capacity - size, fmt, ap), capacity - 1);

    writeLine(size);
}

void frt::Log::log_buffer(LogLevel level, const char *name, const uint8_t *buffer, size_t len)
{
    if (!isEnabled(level) || FRT_IS_ISR())
        return;

    LockGuard lock(mutex);

    const size_t capacity = sizeof(buf) - LINE_END_RESERVE;
    size_t size = formatHeader(level);
    size = clampedAdd(size, snprintf(buf + size, capacity - size, "%s[%u]: ", name, static_cast<unsigned int>(len)), capacity - 1);

    for (size_t i = 0; i < len && size < capacity - 1; i++)
    {
        if (i % 16 == 0)
            size = clampedAdd(size, snprintf(buf + size, capacity - size, "\r\n"), capacity - 1);
        size = clampedAdd(size, snprintf(buf + size, capacity - size, "%02X ", buffer[i]), capacity - 1);
    }

    writeLine(size);
}

void frt::Log::log_blank()
{
    if (_quiet || FRT_IS_ISR())
        return;

    LockGuard lock(mutex);
    writeLine(0);
}
