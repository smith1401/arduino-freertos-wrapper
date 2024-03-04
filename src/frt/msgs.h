#ifndef __FRT_MSGS_H__
#define __FRT_MSGS_H__

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

        struct PIDInput : public Message
        {
            float setpoint;
            float p;
            float i;
            float d;
        };

        struct PIDError : public Message
        {
            float error;
            float ep;
            float ei;
            float ed;
        };
    }
}

#endif // __FRT_MSGS_H__