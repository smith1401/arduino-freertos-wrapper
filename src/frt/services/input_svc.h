#ifndef __INPUT_SVC_H__
#define __INPUT_SVC_H__

#include <Arduino.h>
#include <vector>
#include <BindArg.h>

#include <frt/frt.h>

namespace frt
{
#define RECORD_INPUT_EVENTS "input_events"
#define INPUT_SEQUENCE_SOURCE_HARDWARE (0u)
#define INPUT_SEQUENCE_SOURCE_SOFTWARE (1u)

/* Input Related Constants */
#define INPUT_DEBOUNCE_TICKS 4
#define INPUT_DEBOUNCE_TICKS_HALF (INPUT_DEBOUNCE_TICKS / 2)
#define INPUT_PRESS_TICKS 150
#define INPUT_LONG_PRESS_COUNTS 8

#define GPIO_Read(input_pin) (bool)((bool)digitalRead(input_pin.pin.gpio) ^ (input_pin.pin.inverted))

    class InputTimer;

    /* Input Keys */
    typedef enum
    {
        InputKeyUp,
        InputKeyDown,
        InputKeyRight,
        InputKeyLeft,
        InputKeyOk,
        InputKeyBack,
        InputKeyMAX, /**< Special value */
    } InputKey;

    typedef struct
    {
        uint8_t gpio;
        InputKey key;
        bool inverted;
        char *name;
    } InputPin;

    /** Input Types
     * Some of them are physical events and some logical
     */
    // typedef enum
    // {
    //     InputTypePress = (1 << 0),   /**< Press event, emitted after debounce */
    //     InputTypeRelease = (1 << 1), /**< Release event, emitted after debounce */
    //     InputTypeShort = (1 << 2),   /**< Short event, emitted after InputTypeRelease done within INPUT_LONG_PRESS interval */
    //     InputTypeLong = (1 << 3),    /**< Long event, emitted after INPUT_LONG_PRESS_COUNTS interval, asynchronous to InputTypeRelease  */
    //     InputTypeRepeat = (1 << 4),  /**< Repeat event, emitted with INPUT_LONG_PRESS_COUNTS period after InputTypeLong event */
    //     InputTypeMAX = 0xFFFFFFFF,   /**< Special value for exceptional */
    // } InputType;

    enum class InputType : uint32_t
    {
        Press = (1 << 0),   /**< Press event, emitted after debounce */
        Release = (1 << 1), /**< Release event, emitted after debounce */
        Short = (1 << 2),   /**< Short event, emitted after InputTypeRelease done within INPUT_LONG_PRESS interval */
        Long = (1 << 3),    /**< Long event, emitted after INPUT_LONG_PRESS_COUNTS interval, asynchronous to InputTypeRelease  */
        Repeat = (1 << 4),  /**< Repeat event, emitted with INPUT_LONG_PRESS_COUNTS period after InputTypeLong event */
        MAX = 0xFFFFFFFF,   /**< Special value for exceptional */
    };

    // Explicitly define operators for enum class
    inline InputType operator|(InputType a, InputType b)
    {
        return static_cast<InputType>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline bool operator&(InputType a, InputType b)
    {
        return static_cast<bool>(static_cast<int>(a) & static_cast<int>(b));
    }

    inline bool operator!(InputType flags)
    {
        return static_cast<int>(flags) == 0;
    }

    /** Input Event, dispatches with FuriPubSub */
    typedef struct
    {
        union
        {
            uint32_t sequence;
            struct
            {
                uint8_t sequence_source : 2;
                uint32_t sequence_counter : 30;
            };
        };
        uint32_t time;
        InputKey key;
        InputType type;
    } InputEvent;

    /** Input pin state */
    typedef struct
    {
        InputPin pin;
        // State
        volatile bool state;
        volatile uint8_t debounce;
        InputTimer *press_timer;
        volatile uint8_t press_counter;
        volatile uint32_t counter;
    } InputPinState;

    class InputTimer : public frt::Timer
    {
    private:
        InputPinState *_inputState;
        InputType _inputFilter;
        frt::Publisher<InputEvent> *_pub;

    public:
        InputTimer(InputPinState *inputState);
        virtual ~InputTimer();
        void Run() override;

        inline void setInputFilter(const InputType filter)
        {
            _inputFilter = filter;
        }
    };

    class InputService : public frt::Task<InputService, 1024>
    // class InputService : public frt::Task<InputService>
    {
    public:
        InputService(std::initializer_list<InputPin> pins);
        virtual ~InputService();
        bool run() override;
        void input_isr();

        inline void setInputFilter(const InputType filter)
        {
            _inputFilter = filter;

            for (auto &state : _inputPinStates)
            {
                state.press_timer->setInputFilter(filter);
            }
        }

        // inline const char *getKeyName(InputKey key)
        // {
        //     for (auto &state : _inputPinStates)
        //     {
        //         if (state.pin.key == key)
        //             return state.pin.name;
        //     }
        //     return "Unknown";
        // }

        static const char *getKeyName(InputKey key)
        {
            switch (key)
            {
            case InputKeyUp:
                return "Up";
            case InputKeyDown:
                return "Down";
            case InputKeyRight:
                return "Right";
            case InputKeyLeft:
                return "Left";
            case InputKeyOk:
                return "OK";
            case InputKeyBack:
                return "Back";
            default:
                return "Unknown";
            }
        }

        static const char *getTypeName(InputType type)
        {
            switch (type)
            {
            case InputType::Press:
                return "Press";
            case InputType::Release:
                return "Release";
            case InputType::Short:
                return "Short";
            case InputType::Long:
                return "Long";
            case InputType::Repeat:
                return "Repeat";
            default:
                return "Unknown";
            }
        }

    private:
        std::vector<InputPinState> _inputPinStates;
        volatile uint32_t _counter;
        frt::Publisher<InputEvent> *_pub;
        InputType _inputFilter;
        bindArgVoidFunc_t _intr_gate = nullptr;
    };
}
#endif // __INPUT_SVC_H__