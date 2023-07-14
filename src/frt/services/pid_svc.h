#ifndef __PID_SVC_H__
#define __PID_SVC_H__

#include <Arduino.h>
#include <vector>

#ifdef ESP32
#else
#include <STM32FreeRTOS.h>
#endif

#include <frt/frt.h>
#include <frt/services/output_control_svc.h>
#include <pid.h>

#define RECORD_PID_TARGET "pid_target"

namespace frt
{
    class PIDService : public frt::Task<PIDService>
    {
    public:
        PIDService(float p, float i, float d);
        virtual ~PIDService();
        bool run() override;

    private:
        Publisher<OutputPower> *_output_pub;
        Subscriber<msgs::Temperature> *_input_sub;
        Subscriber<msgs::Temperature> *_target_sub;
        EventGroup *_sub_evt_sync;
        QueueSetHandle_t _sub_queue_set;
        PIDController<float> *_pid;

        float _input;
        float _output;

        uint32_t _last_tick_time;
    };

}

#endif // __PID_SVC_H__