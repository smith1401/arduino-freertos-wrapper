#ifndef __BURST_FIRING_OUTPUT_SVC_H__
#define __BURST_FIRING_OUTPUT_SVC_H__

#include "output_control_svc.h"

#if defined(STM32) || defined(NRF52)
#include <BindArg.h>
#endif

#define PULSE_DELAY_US 2 // Has to be greater than 1
#define PULSE_TIME_US 1000
#define AC_FREQUENCY 50
#define MAX_BURST_COUNT (AC_FREQUENCY * 2) / OUTPUT_RES_HZ

namespace frt
{
    class BurstFiringOutputControlService final : public ZeroCrossOutputControlService
    {
    public:
        BurstFiringOutputControlService(const uint8_t output_pin, const uint8_t zero_cross_pin);
        virtual ~BurstFiringOutputControlService();
        bool run() override;

    protected:
        void zero_cross_isr() override;

    private:
        void init_pulse_timer();
        void pulse_timer_start();
        void pulse_timer_stop();
        void pulse_output();

        uint32_t _last_output_time;
        uint32_t _last_pid_evt_count;

        uint8_t _output_pin;
        uint8_t _zero_cross_pin;

        volatile uint32_t _burst_count;
        volatile uint32_t _zero_cross_count;
#if defined(STM32) || defined(NRF52)
        bindArgVoidFunc_t _intr_gate = nullptr;
#endif

#if defined(ESP32)
        rmt_obj_t *_pulse_timer;
        rmt_data_t _pulse_data;
#elif defined(STM32)
        HardwareTimer *_pulse_timer;
#elif defined(NRF52) || defined(NRF52840_XXAA)
#warning "Pulse timer is not implemented yet for NRF52"
#endif

        uint32_t _pulse_timer_channel;
    };
}

#endif // __BURST_FIRING_OUTPUT_SVC_H__