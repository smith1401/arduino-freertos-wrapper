#include "pid_svc.h"

#define INPUT_EVT_SLOT (1 << 0)
#define TARGET_EVT_SLOT (1 << 1)

frt::PIDService::PIDService(float p, float i, float d, bool *calc_pid) : _input(0.0),
                                                                         _output(0.0),
                                                                         _last_tick_time(0.0),
                                                                         _calc_pid(calc_pid)
{
    // Init publisher
    _output_pub = frt::pubsub::advertise<OutputPower>(RECORD_OUTPUT_POWER);
    _pid_err_pub = frt::pubsub::advertise<msgs::PIDError>(RECORD_PID_ERRORS);

    // Init subscribers
    _input_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_TEMPERATURE);
    _target_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_PID_TARGET);
    _pid_sub = frt::pubsub::subscribe<msgs::PIDInput>(RECORD_PID_VALUES);
    _calc_sub = frt::pubsub::subscribe<msgs::Message, 1>(RECORD_CALC_PID);

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
    _pid->setMaxIntegralCumulation(10000);

    // Init subscriber sync
    _sub_queue_set = xQueueCreateSet(50);
    _input_sub->addToSet(_sub_queue_set);
    _target_sub->addToSet(_sub_queue_set);
    _pid_sub->addToSet(_sub_queue_set);
    _calc_sub->addToSet(_sub_queue_set);
}

frt::PIDService::~PIDService()
{
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
            // Limit target temperature to 300 degrees
            if (target.temperature > MAX_TEMP)
            {
                FRT_LOG_DEBUG("Trying to set a temperature of %.1f C which is too high. Will be reduced to %.1f C", target.temperature, MAX_TEMP);
                target.temperature = MAX_TEMP;
            }

            FRT_LOG_DEBUG("Setpoint: %.1f", target.temperature);
            _pid->setTarget(target.temperature);
        }
    }

    // PID parameters
    if (_pid_sub->canReceive(queue_set_member))
    {
        frt::msgs::PIDInput pid;
        if (_pid_sub->receive(pid))
        {
            if (pid.setpoint > MAX_TEMP)
            {
                FRT_LOG_DEBUG("Trying to set a temperature of %.1f C which is too high. Will be reduced to %.1f C", pid.setpoint, MAX_TEMP);
                pid.setpoint = MAX_TEMP;
            }

            FRT_LOG_DEBUG("Setpoint: %.1f\tP: %2.3f\tI: %2.3f\tD: %2.3f", pid.setpoint, pid.p, pid.i, pid.d);
            _pid->setTarget(pid.setpoint);
            _pid->setPID(pid.p, pid.i, pid.d);
        }
    }

    // PID calculate
    if (_calc_sub->canReceive(queue_set_member))
    {
        frt::msgs::Message msg;
        if (_calc_sub->receive(msg))
        {
            OutputPower output;
            msgs::PIDError err;

            _pid->tick();

            err.error = _pid->getError();
            err.ep = _pid->getProportionalComponent();
            err.ei = _pid->getIntegralComponent();
            err.ed = _pid->getDerivativeComponent();
            // FRT_LOG_DEBUG("%.3f %.3f %.3f", _pid->getProportionalComponent(), _pid->getIntegralComponent(), _pid->getDerivativeComponent());
            _pid_err_pub->publish(err);

            output.power = static_cast<uint8_t>(_output);
            // FRT_LOG_DEBUG("New output power: %d", output.power);
            _output_pub->publish(output);
        }
    }

    return true;
}
