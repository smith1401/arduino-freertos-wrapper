#ifndef __FRT_MSGS_H__
#define __FRT_MSGS_H__

#include <Arduino.h>

namespace frt
{
    namespace msgs
    {
        struct Message
        {
            uint32_t timestamp;
        };

        struct Temperature : public Message
        {
            float temperature;
        };

        struct PID : public Message
        {
            float setpoint;
            float p;
            float i;
            float d;
        };
    }
}

#endif // __FRT_MSGS_H__