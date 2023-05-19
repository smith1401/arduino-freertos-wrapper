#ifndef __BURST_FIRING_OUTPUT_SVC_H__
#define __BURST_FIRING_OUTPUT_SVC_H__

#include "output_control_svc.h"

#define PULSE_DELAY_US 2 // Has to be greater than 1
#define PULSE_TIME_US 500
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

        uint8_t _output_pin;
        uint8_t _zero_cross_pin;

        volatile uint32_t _burst_count;
        volatile uint32_t _zero_cross_count;

#ifdef ESP32
    rmt_obj_t *_pulse_timer;
    rmt_data_t _pulse_data;
#else
        HardwareTimer *_pulse_timer;
#endif
        uint32_t _pulse_timer_channel;
    };
}

#endif // __BURST_FIRING_OUTPUT_SVC_H__