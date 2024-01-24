#include "log.h"

using namespace frt;

Log *Log::instance = nullptr;

static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

Log *Log::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Log();
    }
    return instance;
}

void frt::Log::registerStream(Stream *s)
{
    streams.push_back(s);
}

void frt::Log::setLevel(LogLevel level)
{
    _level = level;
}

// bool frt::Log::run()
// {
//     char b[buffer.size()];
//     taskENTER_CRITICAL();
//     size_t received = buffer.receive((uint8_t *)b, sizeof(b));
//     taskEXIT_CRITICAL();

//     if (received > 0)
//     {
//         for (auto &p : printers)
//         {
//             p->write(b, received);
//             p->flush();
//         }
//     }

//     return true;
// }

void Log::log(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    // mutex.lock();

    // TaskStatus_t xTaskDetails;
    // xTaskDetails.pcTaskName = "";

    // if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    //     vTaskGetInfo(NULL, &xTaskDetails, pdFALSE, eInvalid);

    // bool task_valid = strcmp(xTaskDetails.pcTaskName, "") != 0;

    // const char* task_name = task_valid ? xTaskDetails.pcTaskName : "NONE";
    // int task_prio = task_valid ? xTaskDetails.uxCurrentPriority : -1;
    // int task_num= task_valid ? xTaskDetails.xTaskNumber : -1;

    LogEvent ev;
    ev.fmt = fmt;
    ev.file = file;
    ev.line = line;
    ev.level = level;

    if (!_quiet && level >= _level)
    {
        va_start(ev.ap, fmt);

        uint32_t ticks = xTaskGetTickCount();

#ifdef LOG_USE_COLOR
        int size = snprintf(buf, sizeof(buf), "%02lu:%02lu:%03lu %s%-5s \x1b[0m", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_colors[ev.level], level_strings[ev.level]);
        // int size = snprintf(buf, sizeof(buf), "%02lu:%02lu:%03lu [%s | %d] %s%-5s \x1b[0m", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, ev.file, ev.line, level_colors[ev.level], level_strings[ev.level]);
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_colors[ev.level], level_strings[ev.level], ev.file, ev.line);
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %s%-5s\x1b[0m \x1b[90mTask#%d %s[%d]\x1b[0m ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_colors[ev.level], level_strings[ev.level], task_num, task_name, task_prio);
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %s%-5s\x1b[0m \x1b[90m%s\x1b[0m ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_colors[ev.level], level_strings[ev.level], task_name);
#else
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %-5s %s:%d: ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_strings[ev.level], ev.file, ev.line);
        int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %-5s ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_strings[ev.level]);
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %-5s Task#%d %s[%d] ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_strings[ev.level], task_num, task_name, task_prio);
        // int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %-5s %s ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_strings[ev.level], task_name);
#endif

        size += vsnprintf(buf + size, sizeof(buf) - size, fmt, ev.ap);
        buf[size++] = '\r';
        buf[size++] = '\n';
        buf[size] = '\0';

        for (auto &s : streams)
        {
            s->write(buf, size);
            // s->flush();
        }

        // TODO: check if it can be done with message buffers. it isn't working at the moment. The advantage of this is that no logging task is needed.
        //  taskENTER_CRITICAL();
        //  buffer.send((uint8_t *)buf, strlen(buf), 0);
        //  taskEXIT_CRITICAL();

        va_end(ev.ap);
    }

    // mutex.unlock();
}

void frt::Log::log_buffer(LogLevel level, const char *name, uint8_t *buffer, size_t len)
{
    if (!_quiet && level >= _level)
    {
        uint32_t ticks = xTaskGetTickCount();

#ifdef LOG_USE_COLOR
        int size = snprintf(buf, sizeof(buf), "%02lu:%02lu:%03lu %s%-5s \x1b[0m", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_colors[level], level_strings[level]);
#else
        int size = snprintf(buf, sizeof(buf), "%02d:%02d:%03d %-5s ", ticks / 1000 / 60, (ticks / 1000) % 60, ticks % 1000, level_strings[ev.level]);
#endif

        size += snprintf(buf + size, sizeof(buf) - size, "%s[%d]: ", name, len);
        for (size_t i = 0; i < len; i++)
        {
            if (i % 16 == 0)
                size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
            size += snprintf(buf + size, sizeof(buf) - size, "%02X ", buffer[i]);
        }

        buf[size++] = '\r';
        buf[size++] = '\n';
        buf[size] = '\0';

        for (auto &s : streams)
        {
            s->write(buf, size);
            // s->flush();
        }
    }
}

void frt::Log::log_blank()
{
    if (!_quiet)
    {
        buf[0] = '\r';
        buf[1] = '\n';
        buf[2] = '\0';

        for (auto &s : streams)
        {
            s->write(buf, 3);
        }
    }
}