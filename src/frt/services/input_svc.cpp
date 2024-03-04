#include "input_svc.h"

#ifdef ESP32
#include <FunctionalInterrupt.h>
#endif

using namespace frt;

InputTimer::InputTimer(InputPinState *inputState, Publisher<InputEvent> *pub) : Timer(inputState->pin.name, pdMS_TO_TICKS(INPUT_PRESS_TICKS)),
                                                                                _pub(pub),
                                                                                _inputState(inputState),
                                                                                _inputFilter(InputType::MAX)
{
}

InputTimer::~InputTimer()
{
}

void InputTimer::run()
{
    InputEvent event;
    event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
    event.sequence_counter = _inputState->counter;
    event.key = _inputState->pin.key;
    event.time = xTaskGetTickCount();
    _inputState->press_counter++;
    if (_inputState->press_counter == INPUT_LONG_PRESS_COUNTS)
    {
        event.type = InputType::Long;

        if (_inputFilter & event.type)
            _pub->publish(event);
    }
    else if (_inputState->press_counter > INPUT_LONG_PRESS_COUNTS)
    {
        _inputState->press_counter--;
        event.type = InputType::Repeat;

        if (_inputFilter & event.type)
            _pub->publish(event);
    }
}

InputService::InputService(std::initializer_list<InputPin> inputPins) : _counter(0),
                                                                        _inputFilter(InputType::MAX)
{
    _pub = frt::pubsub::advertise<InputEvent>(RECORD_INPUT_EVENTS);

    for (auto pin : inputPins)
    {
        if (pin.inverted)
            pinMode(pin.gpio, INPUT_PULLUP);
        else
            pinMode(pin.gpio, INPUT_PULLDOWN);

#ifdef ESP32
        attachInterrupt(digitalPinToInterrupt(pin.gpio), std::bind(&InputService::input_isr, this), CHANGE);
#else
        _intr_gate = bindArgGateThisAllocate(&InputService::input_isr, this);
        attachInterrupt(pin.gpio, _intr_gate, CHANGE);
#endif
        InputPinState pinState;
        pinState.pin = pin;
        pinState.state = GPIO_Read(pinState);
        pinState.debounce = INPUT_DEBOUNCE_TICKS_HALF;
        pinState.press_counter = 0;

        _inputPinStates.push_back(pinState);
    }

    for (auto &state : _inputPinStates)
    {
        state.press_timer = new InputTimer(&state, _pub);
    }
}

InputService::~InputService()
{
    _inputPinStates.clear();
}

bool InputService::run()
{
    bool is_changing = false;

    for (auto &pinState : _inputPinStates)
    {
        bool state = GPIO_Read(pinState);
        if (state)
        {
            if (pinState.debounce < INPUT_DEBOUNCE_TICKS)
                pinState.debounce += 1;
        }
        else
        {
            if (pinState.debounce > 0)
                pinState.debounce -= 1;
        }

        if (pinState.debounce > 0 &&
            pinState.debounce < INPUT_DEBOUNCE_TICKS)
        {
            is_changing = true;
        }
        else if (pinState.state != state)
        {
            pinState.state = state;

            // Common state info
            InputEvent event;
            event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
            event.key = pinState.pin.key;
            event.time = xTaskGetTickCount();

            // Short / Long / Repeat timer routine
            if (state)
            {
                _counter++;
                pinState.counter = _counter;
                event.sequence_counter = pinState.counter;
                pinState.press_timer->setPeriod(pdMS_TO_TICKS(INPUT_PRESS_TICKS));
            }
            else
            {
                event.sequence_counter = pinState.counter;
                pinState.press_timer->stop();
                if (pinState.press_counter < INPUT_LONG_PRESS_COUNTS)
                {
                    event.type = InputType::Short;

                    if (_inputFilter & event.type)
                        _pub->publish(event);
                }
                pinState.press_counter = 0;
            }

            // Send Press/Release event
            event.type = pinState.state ? InputType::Press : InputType::Release;

            if (_inputFilter & event.type)
                _pub->publish(event);
        }
    }

    if (is_changing)
    {
        msleep(1);
    }
    else
    {
        wait();
    }

    return true;
}

#ifdef ESP32
void IRAM_ATTR InputService::input_isr()
#else
void InputService::input_isr()
#endif
{
    post();
}
