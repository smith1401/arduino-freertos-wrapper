#ifndef __PID_SVC_H__
#define __PID_SVC_H__

#include <Arduino.h>
#include <vector>

#include "frt/frt.h"
#include "frt/log.h"
#include "output_control_svc.h"
#include "temperature_svc.h"
#include "frt/pid/pid.h"

#define RECORD_PID_TARGET "pid_target"
#define RECORD_PID_VALUES "pid_values"
#define RECORD_PID_ERRORS "pid_errors"

#define MAX_TEMP 300.0F

namespace frt
{
    class PIDService : public frt::Task<PIDService, 4096>
    {
    public:
        /**
         *  @param calc_pid Currently IGNORED: the pointer is stored but never
         *         read, and the PID output is calculated on every
         *         RECORD_CALC_PID event regardless of it. Code may rely on that
         *         (e.g. a keep-warm mode that only sets a target temperature),
         *         so honouring the flag would be a breaking change and is left
         *         for a major release.
         */
        PIDService(float p, float i, float d, bool *calc_pid);
        virtual ~PIDService();
        bool run() override;

        float getTarget();

    private:
        Publisher<OutputPower> *_output_pub;
        Publisher<msgs::PIDError> *_pid_err_pub;
        Subscriber<msgs::Temperature> *_input_sub;
        Subscriber<msgs::Temperature> *_target_sub;
        Subscriber<msgs::PIDInput> *_pid_sub;
        Subscriber<msgs::Message, 1> *_calc_sub;
        EventGroup *_sub_evt_sync;
        QueueSetHandle_t _sub_queue_set;
        PIDController<float> *_pid;

        float _input;
        float _output;
        uint32_t _last_tick_time;
        bool *_calc_pid; // stored but not used, see the constructor
    };

}

#endif // __PID_SVC_H__