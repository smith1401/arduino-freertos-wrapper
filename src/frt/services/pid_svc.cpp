#include "pid_svc.h"

#define INPUT_EVT_SLOT (1 << 0)
#define TARGET_EVT_SLOT (1 << 1)

frt::PIDService::PIDService(float p, float i, float d, bool *calc_pid) : _input(0.0),
                                                                         _output(0.0),
                                                                         _last_tick_time(0.0),
                                                                         _calc_pid(calc_pid)
{
    // Init publisher and subscribers
    _output_pub = frt::pubsub::advertise<OutputPower>(RECORD_OUTPUT_POWER);
    _input_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_TEMPERATURE);
    _target_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_PID_TARGET);
    _pid_sub = frt::pubsub::subscribe<msgs::PID>(RECORD_PID_VALUES);
    _calc_sub = frt::pubsub::subscribe<msgs::Message>(RECORD_CALC_PID, 1);

    // Init PID
    _pid = new PIDController<float>(
        p, i, d,
        // Get input
        [this]()
        { return _input; },
        // Set output
        [this](float output)
        { _output = output; });

    _pid->registerTimeFunction(millis);
    _pid->setOutputBounds(0.0, 100.0);
    _pid->setOutputBounded(true);

    // Init subscriber sync
    // _sub_evt_sync = new frt::EventGroup();
    // _input_sub->addEvent(_sub_evt_sync, INPUT_EVT_SLOT);
    // _target_sub->addEvent(_sub_evt_sync, TARGET_EVT_SLOT);
    _sub_queue_set = xQueueCreateSet(20);
    _input_sub->addToSet(_sub_queue_set);
    _target_sub->addToSet(_sub_queue_set);
    _pid_sub->addToSet(_sub_queue_set);
    _calc_sub->addToSet(_sub_queue_set);
}

frt::PIDService::~PIDService()
{
    delete _output_pub;
    delete _input_sub;
    delete _target_sub;
    delete _pid_sub;
    delete _pid;
}

bool frt::PIDService::run()
{
    QueueSetMemberHandle_t queue_set_member = xQueueSelectFromSet(_sub_queue_set, portMAX_DELAY);

    // Temperature
    if (_input_sub->canReceive(queue_set_member))
    {
        frt::msgs::Temperature input;
        if (_input_sub->receive(input))
            _input = _input == 0.0f ? input.temperature : (_input + input.temperature) / 2.0f;
    }

    // Target
    if (_target_sub->canReceive(queue_set_member))
    {
        frt::msgs::Temperature target;
        if (_target_sub->receive(target))
        {
            FRT_LOG_DEBUG("Setpoint: %.1f", target.temperature);
            _pid->setTarget(target.temperature);
        }
    }

    // PID parameters
    if (_pid_sub->canReceive(queue_set_member))
    {
        frt::msgs::PID pid;
        if (_pid_sub->receive(pid))
        {
            FRT_LOG_DEBUG("Setpoint: %.1f\tP: %2.3f\tI: %2.3f\tD: %2.3f", pid.setpoint, pid.p, pid.i, pid.d);
            _pid->setTarget(pid.setpoint);
            _pid->setPID(pid.p, pid.i, pid.d);
        }
    }

    // PID calculate
    if (_calc_sub->canReceive(queue_set_member))
    {
        // TODO: check if calc_pid is really necessary
        // frt::msgs::Message msg;
        // OutputPower output;
        // if (_calc_sub->receive(msg) && (*_calc_pid))
        // {
        //     _pid->tick();
        //     output.power = static_cast<uint8_t>(_output);
        //     _output_pub->publish(output);
        // }
        // else
        // {
        //     output.power = 0;
        //     _output_pub->publish(output);
        // }

        frt::msgs::Message msg;
        if (_calc_sub->receive(msg))
        {
            OutputPower output;

            _pid->tick();

            output.power = static_cast<uint8_t>(_output);
            // FRT_LOG_DEBUG("New output power: %d", output.power);
            _output_pub->publish(output);
        }
    }

    // uint32_t now = xTaskGetTickCount();
    // uint32_t elapsed_ms = (now - _last_tick_time) * portTICK_PERIOD_MS;

    // if (elapsed_ms > OUTPUT_RATE_MS)
    // {
    //     _pid->tick();

    //     OutputPower output;
    //     output.power = static_cast<uint8_t>(_output);
    //     _output_pub->publish(output);

    //     _last_tick_time = now;
    // }

    return true;
}

float frt::PIDService::getTarget()
{
    return 10.5;
}
