#include "input_svc.h"

#ifdef ESP32
#include <FunctionalInterrupt.h>
#endif

using namespace frt;

InputTimer::InputTimer(InputPinState *inputState) : frt::Timer(inputState->pin.name, pdMS_TO_TICKS(INPUT_PRESS_TICKS)),
                                                    _inputState(inputState),
                                                    _inputFilter(InputTypeMAX)
{
    _pub = frt::pubsub::advertise<InputEvent>(RECORD_INPUT_EVENTS);
}

InputTimer::~InputTimer()
{
}

void InputTimer::Run()
{
    InputEvent event;
    event.sequence_source = INPUT_SEQUENCE_SOURCE_HARDWARE;
    event.sequence_counter = _inputState->counter;
    event.key = _inputState->pin.key;
    event.time = xTaskGetTickCount();
    _inputState->press_counter++;
    if (_inputState->press_counter == INPUT_LONG_PRESS_COUNTS)
    {
        event.type = InputTypeLong;

        if (_inputFilter & event.type)
            _pub->publish(event);
    }
    else if (_inputState->press_counter > INPUT_LONG_PRESS_COUNTS)
    {
        _inputState->press_counter--;
        event.type = InputTypeRepeat;

        if (_inputFilter & event.type)
            _pub->publish(event);
    }
}

InputService::InputService(std::initializer_list<InputPin> inputPins) : _counter(0),
                                                                        _inputFilter(InputTypeMAX)
{
    for (auto pin : inputPins)
    {
        pinMode(pin.gpio, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(pin.gpio), std::bind(&InputService::input_isr, this), CHANGE);

        InputPinState pinState{
            .pin = pin,
            .state = GPIO_Read(pinState),
            .debounce = INPUT_DEBOUNCE_TICKS_HALF,
            // .press_timer = new InputTimer(&pinState),
            .press_counter = 0};

        _inputPinStates.push_back(pinState);
    }

    for (auto &state : _inputPinStates)
    {
        state.press_timer = new InputTimer(&state);
    }

    // TODO: Remove after debug
    pinMode(GPIO_NUM_0, OUTPUT);
    digitalWrite(GPIO_NUM_0, LOW);

    _pub = frt::pubsub::advertise<InputEvent>(RECORD_INPUT_EVENTS);
}

InputService::~InputService()
{
    for (auto &state : _inputPinStates)
    {
        delete state.press_timer;
    }
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
                pinState.press_timer->SetPeriod(pdMS_TO_TICKS(INPUT_PRESS_TICKS));
            }
            else
            {
                event.sequence_counter = pinState.counter;
                pinState.press_timer->Stop();
                if (pinState.press_counter < INPUT_LONG_PRESS_COUNTS)
                {
                    event.type = InputTypeShort;

                    if (_inputFilter & event.type)
                        _pub->publish(event);
                }
                pinState.press_counter = 0;
            }

            // Send Press/Release event
            event.type = pinState.state ? InputTypePress : InputTypeRelease;

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
        // msleep(10);
    }

    return true;
}

#ifdef ESP32
void IRAM_ATTR InputService::input_isr()
#else
void InputService::input_isr()
#endif
{
    FRT_CRITICAL_ENTER();
    post();
    FRT_CRITICAL_EXIT();
    // preparePostFromInterrupt();
    // postFromInterrupt();
    // finalizePostFromInterrupt();

    // static int count = 0;
    // FRT_LOG_DEBUG("ISR #%d", ++count);
}
