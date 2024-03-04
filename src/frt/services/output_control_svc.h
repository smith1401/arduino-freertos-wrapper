#ifndef __OUTPUT_CONTROL_SVC_H__
#define __OUTPUT_CONTROL_SVC_H__

#include "frt/frt.h"
#include "frt/log.h"
#include "frt/task.h"
#include "frt/pubsub.h"

#define RECORD_OUTPUT_POWER "output_power"
#define RECORD_CALC_PID "calc_pid"
#define OUTPUT_RES_HZ 1
#define OUTPUT_RATE_MS (1 / (float)OUTPUT_RES_HZ) * 1000

namespace frt
{
    struct OutputPower
    {
        uint8_t power;
    };

    class OutputControlService : public Task<OutputControlService, 2048>
    {
    protected:
        Subscriber<OutputPower, 1> *_sub_output_power;
        Publisher<msgs::Message> *_pub_pid_calc_event;
    };

    class ZeroCrossOutputControlService : public OutputControlService
    {
    protected:
        virtual void zero_cross_isr() = 0;
    };
}

#endif // __OUTPUT_CONTROL_SVC_H__