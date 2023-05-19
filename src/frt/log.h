#ifndef __FRT_LOG_H__
#define __FRT_LOG_H__

#include <Arduino.h>
#include <vector>
#include "mutex.h"
#include "task.h"
#include "messagebuffer.h"

#define FRT_LOG_REGISTER_STREAM(stream) frt::Log::getInstance()->registerStream(stream);
// #define FRT_LOG_ENABLE() frt::Log::getInstance()->start(tskIDLE_PRIORITY + 1, "svc_log");

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

    static const char *level_strings[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
    static const char *level_colors[] = {
        "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

    class Log //: public frt::Task<Log>
    {
    private:
        static Log *instance;
        // MessageBuffer<MAX_LOG_SIZE> buffer;
        Mutex mutex;
        std::vector<Stream *> streams;
        LogLevel _level;
        bool _quiet;
        char buf[MAX_LOG_SIZE];

        Log() : _quiet(false), _level(LogLevel::ERROR)
        {
        }

    public:
        Log(Log &other) = delete;
        void operator=(const Log &) = delete;

        static Log *getInstance();
        // virtual bool run() override;
        void registerStream(Stream *s);
        void setLevel(LogLevel level);
        void log(LogLevel level, const char *file, int line, const char *fmt, ...);
    };
}

#endif // __FRT_LOG_H__