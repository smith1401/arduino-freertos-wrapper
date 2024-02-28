#include "burst_firing_output_svc.h"

#ifdef ESP32
#include <FunctionalInterrupt.h>
#endif

using namespace frt;

BurstFiringOutputControlService::BurstFiringOutputControlService(const uint8_t output_pin, const uint8_t zero_cross_pin) : _last_output_time(0),
                                                                                                                           _last_pid_evt_count(0),
                                                                                                                           _output_pin(output_pin),
                                                                                                                           _zero_cross_pin(zero_cross_pin),
                                                                                                                           _burst_count(1),
                                                                                                                           _zero_cross_count(0)
{
    // Init zero-crossing input
    pinMode(_zero_cross_pin, INPUT);

#ifdef ESP32
    attachInterrupt(digitalPinToInterrupt(_zero_cross_pin), std::bind(&BurstFiringOutputControlService::zero_cross_isr, this), RISING);
#else
    _intr_gate = bindArgGateThisAllocate(&BurstFiringOutputControlService::zero_cross_isr, this);
    attachInterrupt(digitalPinToInterrupt(_zero_cross_pin), _intr_gate, RISING);
#endif

    // Init timer
    init_pulse_timer();

    // Init output power subscriber
    _sub_output_power = frt::pubsub::subscribe<OutputPower>(RECORD_OUTPUT_POWER, 1);

    // Init pid calc event publisher
    _pub_pid_calc_event = frt::pubsub::advertise<frt::msgs::Message>(RECORD_CALC_PID);
}

BurstFiringOutputControlService::~BurstFiringOutputControlService()
{
}

bool BurstFiringOutputControlService::run()
{
    frt::OutputPower output_power;

    if (_sub_output_power->receive(output_power, 1))
    {
        uint32_t bursts = map(output_power.power, 0, 100, 0, MAX_BURST_COUNT);

        FRT_CRITICAL_ENTER();
        _burst_count = bursts;
        FRT_CRITICAL_EXIT();
    }

    if (_zero_cross_count >= (_last_pid_evt_count + MAX_BURST_COUNT))
    {
        frt::msgs::Message msg;
        msg.timestamp = xTaskGetTickCount();
        _pub_pid_calc_event->publish(msg);

        _last_pid_evt_count = _zero_cross_count;
    }

    // FRT_LOG_DEBUG("New output power: %d -> %d bursts", output_power.power, bursts);
    // FRT_LOG_DEBUG("Bursts: %d\tPulses: %d", _burst_count, _zero_cross_count);

    // uint32_t now = xTaskGetTickCount();
    // uint32_t sleep_time_ms = (1 / (float)OUTPUT_RES_HZ) * 1000;
    // uint32_t elapsed_ms = (now - _last_output_time) * portTICK_PERIOD_MS;
    // _last_output_time = now;

    // if (elapsed_ms < sleep_time_ms)
    //     msleep(sleep_time_ms - elapsed_ms);

    return true;
}

#ifdef ESP32
void IRAM_ATTR BurstFiringOutputControlService::zero_cross_isr()
#else
void BurstFiringOutputControlService::zero_cross_isr()
#endif
{
    // If the number of bursts is greater than 0 -> pulse
    FRT_CRITICAL_ENTER();

    if (_burst_count > 0)
    {
        pulse_output();
        _burst_count--;
    }

    _zero_cross_count++;

    FRT_CRITICAL_EXIT();

    // post();
}

void BurstFiringOutputControlService::init_pulse_timer()
{
#if defined(ESP32)
    if ((_pulse_timer = rmtInit(_output_pin, RMT_TX_MODE, RMT_MEM_128)) == NULL)
    {
        FRT_LOG_ERROR("Pulse Timer initialization failed");
    }

    // Set 1 µs resolution
    float realTick = rmtSetTick(_pulse_timer, 1000);
    FRT_LOG_TRACE("RMT tick set to: %fns", realTick);

    // Set pulse length in µs
    _pulse_data.level0 = 1;
    _pulse_data.duration0 = PULSE_TIME_US;
    _pulse_data.level1 = 0;
    _pulse_data.duration1 = 1;
#elif defined(STM32)
    // Init output
    pinMode(_output_pin, OUTPUT);

    TIM_TypeDef *tim_instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(_output_pin), PinMap_PWM);
    _pulse_timer_channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(_output_pin), PinMap_PWM));
    _pulse_timer = new HardwareTimer(tim_instance);

    _pulse_timer->setMode(_pulse_timer_channel, TIMER_OUTPUT_COMPARE_PWM2, _output_pin);

    tim_instance->PSC = _pulse_timer->getTimerClkFreq() / 1000000 - 1; // Set prescaler
    tim_instance->ARR = PULSE_TIME_US - 1;                             // Set pulse width
    tim_instance->CCR2 = PULSE_DELAY_US - 1;                           // Set delay
    tim_instance->CR1 |= TIM_CR1_OPM;                                  // One pulse mode

    _pulse_timer->refresh();
#elif defined(NRF52) || defined(NRF52840_XXAA)
#warning "Pulse timer is not implemented yet for NRF52"
#endif
}

void BurstFiringOutputControlService::pulse_timer_start()
{
#if defined(ESP32)
#elif defined(STM32)
    if (!_pulse_timer->isRunning())
    {
        _pulse_timer->pause();
        _pulse_timer->resume();
    }
#elif defined(NRF52) || defined(NRF52840_XXAA)
#endif
}

void BurstFiringOutputControlService::pulse_timer_stop()
{
#if defined(ESP32)
#elif defined(STM32)
    if (_pulse_timer->isRunning())
        _pulse_timer->pause();
#elif defined(NRF52) || defined(NRF52840_XXAA)
#endif
}

void BurstFiringOutputControlService::pulse_output()
{
#if defined(ESP32)
    rmtWrite(_pulse_timer, &_pulse_data, 1);
#elif defined(STM32)
    _pulse_timer->pause();
    _pulse_timer->resume();
#elif defined(NRF52) || defined(NRF52840_XXAA)
#endif
}
