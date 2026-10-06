#include <Arduino.h>
#include <frt/frt.h>
#include <frt/log.h>
#include <frt/services/input_svc.h>

/*
 * frt example: debounced buttons with short / long / repeat events.
 */
#if defined(PIN_BUTTON)
#define TEST_BTN PIN_BUTTON
#elif defined(USER_BTN)
#define TEST_BTN USER_BTN
#elif defined(KEY_BUILTIN)
#define TEST_BTN KEY_BUILTIN
#else
#define TEST_BTN 0 // Change to the pin of your button
#endif

class InputHandlerTask : public frt::Task<InputHandlerTask>
{
public:
    InputHandlerTask()
    {
        input_sub = frt::pubsub::subscribe<frt::InputEvent>(RECORD_INPUT_EVENTS);
        FRT_LOG_INFO("Input Handler started with topic [%s]", RECORD_INPUT_EVENTS);
    }

    bool run()
    {
        frt::InputEvent evt;

        if (input_sub->receive(evt))
        {
            FRT_LOG_INFO("Button [%s]: %s", frt::InputService::getKeyName(evt.key), frt::InputService::getTypeName(evt.type));
            FRT_LOG_INFO("Remaining stack size: %u", getRemainingStackSize());
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
        {.gpio = TEST_BTN, .key = frt::InputKeyOk, .inverted = true, .name = "USER"},
    };

    /*
    Filter by button press event type. Possible types are:

        frt::InputType::Press   < Press event, emitted after debounce
        frt::InputType::Release < Release event, emitted after debounce
        frt::InputType::Short   < Short event, emitted after InputTypeRelease done within INPUT_LONG_PRESS interval
        frt::InputType::Long    < Long event, emitted after INPUT_LONG_PRESS_COUNTS interval, asynchronous to InputTypeRelease
        frt::InputType::Repeat  < Repeat event, emitted with INPUT_LONG_PRESS_COUNTS period after InputTypeLong event
    */

    inp_svc->setInputFilter(frt::InputType::Release | frt::InputType::Repeat);
    inp_svc->start(2, "inp_svc");

    inp_hdl_svc = new InputHandlerTask();
    inp_hdl_svc->start(2, "inp_hdl");

    frt::spin();
}

void loop()
{
#if defined(ESP32) || defined(NRF52)
    vTaskDelay(portMAX_DELAY);
#endif
    // On STM32 loop() is called from the idle task and must never block
}