#include <Arduino.h>
#include <frt/frt.h>
#include <frt/services/input_svc.h>

// Define Buttons for input
#define PIN_BUTTON_1 26
#define PIN_BUTTON_2 27

class InputHandlerTask : public frt::Task<InputHandlerTask, 2048>
{
public:
    InputHandlerTask()
    {
        input_sub = new frt::Subscriber<frt::InputEvent>(RECORD_INPUT_EVENTS);
        FRT_LOG_INFO("Input Handler started with topic [%s]", RECORD_INPUT_EVENTS);
    }

    bool run()
    {
        frt::InputEvent evt;

        if (input_sub->receive(evt))
        {
            FRT_LOG_INFO("Button [%s]: %s", frt::InputService::getKeyName(evt.key), frt::InputService::getTypeName(evt.type));
        }

        return true;
    }

private:
    frt::Subscriber<frt::InputEvent> *input_sub;
};

frt::InputService *inp_svc;
InputHandlerTask *inp_hdl_svc;

void setup()
{
    Serial.begin(115200);

    FRT_LOG_REGISTER_STREAM(&Serial);
    FRT_LOG_LEVEL_DEBUG();

    FRT_LOG_INFO("--- FRT Examples: Input Service ---");

    inp_svc = new frt::InputService{
        {.gpio = PIN_BUTTON_1, .key = frt::InputKeyOk, .inverted = true, .name = "USER"},
        {.gpio = PIN_BUTTON_2, .key = frt::InputKeyBack, .inverted = true, .name = "USER"},
    };

    /*
    Filter by button press event type. Possible types are:

        frt::InputTypeRelease < Press event, emitted after debounce 
        frt::InputTypeRelease < Release event, emitted after debounce 
        frt::InputTypeShort   < Short event, emitted after InputTypeRelease done within INPUT_LONG_PRESS interval 
        frt::InputTypeLong    < Long event, emitted after INPUT_LONG_PRESS_COUNTS interval, asynchronous to InputTypeRelease  
        frt::InputTypeRepeat  < Repeat event, emitted with INPUT_LONG_PRESS_COUNTS period after InputTypeLong event 
    */ 

    inp_svc->setInputFilter(frt::InputTypeRelease);
    inp_svc->start(2, "inp_svc");

    inp_hdl_svc = new InputHandlerTask();
    inp_hdl_svc->start(2, "inp_hdl");
}

void loop()
{
}