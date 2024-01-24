#ifndef __FRT_LOG_H__
#define __FRT_LOG_H__

#include <Arduino.h>
#include <vector>
#include "mutex.h"
#include "task.h"
#include "messagebuffer.h"

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

#define MAX_LOG_SIZE 512UL

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

    typedef struct
    {
        va_list ap;
        const char *fmt;
        const char *file;
        void *udata;
        int line;
        LogLevel level;
    } LogEvent;

    class Log
    {
    private:
        static Log *instance;
        std::vector<Stream *> streams;
        bool _quiet;
        LogLevel _level;
        char buf[MAX_LOG_SIZE];

        Log() : _quiet(false), _level(LogLevel::ERROR)
        {
        }

    public:
        Log(Log &other) = delete;
        void operator=(const Log &) = delete;

        static Log *getInstance();
        void registerStream(Stream *s);
        void setLevel(LogLevel level);
        void log(LogLevel level, const char *file, int line, const char *fmt, ...);
        void log_buffer(LogLevel level, const char* name, uint8_t *buffer, size_t len);
        void log_blank();
    };
}

#endif // __FRT_LOG_H__