#include "pid_svc.h"

#define INPUT_EVT_SLOT (1 << 0)
#define TARGET_EVT_SLOT (1 << 1)

frt::PIDService::PIDService(float p, float i, float d) : _input(0.0),
                                                         _output(0.0),
                                                         _last_tick_time(0.0)
{
    // Init publisher and subscribers
    _output_pub = frt::pubsub::advertise<OutputPower>(RECORD_OUTPUT_POWER);
    _input_sub = frt::pubsub::subscribe<msgs::Temperature>("temperature");
    _target_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_PID_TARGET);

    // Init PID
    _pid = new PIDController<float>(
        p, i, d,
        // Get input
        [this]()
        { return _input; },
        // Set output
        [this](float output)
        { _output = output; });

    _pid->registerTimeFunction(xTaskGetTickCount);
    _pid->setOutputBounds(0.0, 100.0);
    _pid->setOutputBounded(true);

    // Init subscriber sync
    // _sub_evt_sync = new frt::EventGroup();
    // _input_sub->addEvent(_sub_evt_sync, INPUT_EVT_SLOT);
    // _target_sub->addEvent(_sub_evt_sync, TARGET_EVT_SLOT);
    _sub_queue_set = xQueueCreateSet(20);
    _input_sub->addToSet(_sub_queue_set);
    _target_sub->addToSet(_sub_queue_set);
}

frt::PIDService::~PIDService()
{
    delete _output_pub;
    delete _input_sub;
    delete _target_sub;
    delete _pid;
}

bool frt::PIDService::run()
{
    // EventBits_t slots = _sub_evt_sync->waitBits(INPUT_EVT_SLOT | TARGET_EVT_SLOT, true, false);

    // if ((slots & INPUT_EVT_SLOT) != 0)
    // {
    //     // Calculate the mean of the input variable
    //     msgs::Temperature input = _input_sub->receive();
    //     _input = _input == 0.0f ? input.temperature : (_input + input.temperature) / 2.0f;
    // }

    // if ((slots & TARGET_EVT_SLOT) != 0)
    // {
    //     // Set target temperature
    //     msgs::Temperature target = _target_sub->receive();
    //     _pid->setTarget(target.temperature);
    // }

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
            _pid->setTarget(target.temperature);
    }

    msgs::Temperature input, target;

    if (_input_sub->receive(input, 1))
        _input = _input == 0.0f ? input.temperature : (_input + input.temperature) / 2.0f;

    if (_target_sub->receive(target, 1))
        _pid->setTarget(target.temperature);

    uint32_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = (now - _last_tick_time) * portTICK_PERIOD_MS;

    if (elapsed_ms > OUTPUT_RATE_MS)
    {
        _pid->tick();

        OutputPower output;
        output.power = static_cast<uint8_t>(_output);
        _output_pub->publish(output);

        _last_tick_time = now;
    }

    return true;
}
