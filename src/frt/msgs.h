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

        struct TemperatureMessage : public Message
        {
            float temperature;
        };
    }
}

#endif // __FRT_MSGS_H__