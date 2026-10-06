#ifndef __FRT_LOG_H__
#define __FRT_LOG_H__

#include <Arduino.h>
#include <stdarg.h>
#include <vector>

#include "mutex.h"

#define FRT_LOG_REGISTER_STREAM(stream) frt::Log::getInstance()->registerStream(stream);

#define FRT_LOG_LEVEL_TRACE() frt::Log::getInstance()->setLevel(frt::LogLevel::TRACE);
#define FRT_LOG_LEVEL_DEBUG() frt::Log::getInstance()->setLevel(frt::LogLevel::DEBUG);
#define FRT_LOG_LEVEL_INFO() frt::Log::getInstance()->setLevel(frt::LogLevel::INFO);
#define FRT_LOG_LEVEL_WARN() frt::Log::getInstance()->setLevel(frt::LogLevel::WARN);
#define FRT_LOG_LEVEL_ERROR() frt::Log::getInstance()->setLevel(frt::LogLevel::ERROR);
#define FRT_LOG_LEVEL_FATAL() frt::Log::getInstance()->setLevel(frt::LogLevel::FATAL);

#define FRT_LOG_TRACE(...) frt::Log::getInstance()->log(frt::LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define FRT_LOG_DEBUG(...) frt::Log::getInstance()->log(frt::LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define FRT_LOG_INFO(...) frt::Log::getInstance()->log(frt::LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define FRT_LOG_WARN(...) frt::Log::getInstance()->log(frt::LogLevel::WARN, __FILE__, __LINE__, __VA_ARGS__)
#define FRT_LOG_ERROR(...) frt::Log::getInstance()->log(frt::LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define FRT_LOG_FATAL(...) frt::Log::getInstance()->log(frt::LogLevel::FATAL, __FILE__, __LINE__, __VA_ARGS__)

#define FRT_LOG_BUFFER(buf, n) frt::Log::getInstance()->log_buffer(frt::LogLevel::TRACE, #buf, buf, n)
#define FRT_LOG_BLANK() frt::Log::getInstance()->log_blank();

#ifndef MAX_LOG_SIZE
#define MAX_LOG_SIZE 512UL
#endif

namespace frt
{
    typedef enum
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    } LogLevel;

    /**
     *  Thread safe logger writing to any number of Arduino Streams.
     *
     *  Lines look like "MM:SS:mmm LEVEL message". Define LOG_USE_COLOR to
     *  get ANSI colors and the name of the logging task.
     *
     *  Logging from an ISR is not supported, such calls are dropped.
     *  Messages longer than MAX_LOG_SIZE are truncated.
     */
    class Log
    {
    private:
        Mutex mutex;
        std::vector<Stream *> streams;

        volatile bool _quiet;
        volatile LogLevel _level;
        char buf[MAX_LOG_SIZE];

        Log() : _quiet(false), _level(LogLevel::ERROR)
        {
        }

        // All of these must be called with the mutex held
        size_t formatHeader(LogLevel level);
        void writeLine(size_t size);

    public:
        Log(const Log &other) = delete;
        Log &operator=(const Log &) = delete;

        static Log *getInstance();
        void registerStream(Stream *s);
        void unregisterStream(Stream *s);
        void setLevel(LogLevel level);
        LogLevel getLevel() const { return _level; }
        void setQuiet(bool quiet) { _quiet = quiet; }
        bool isEnabled(LogLevel level) const { return !_quiet && level >= _level; }

        void log(LogLevel level, const char *file, int line, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
        void vlog(LogLevel level, const char *file, int line, const char *fmt, va_list ap);
        void log_buffer(LogLevel level, const char *name, const uint8_t *buffer, size_t len);
        void log_blank();
    };
}

// Kept for compatibility: log.h used to pull in task.h
#include "task.h"

#endif // __FRT_LOG_H__
