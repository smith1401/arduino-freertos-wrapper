#ifndef __TEMPERATURE_SVC_H__
#define __TEMPERATURE_SVC_H__

#include <Arduino.h>
#include <vector>
// #include <algorithm>
// #include <functional>
#include <algorithm>
#include <numeric>

#ifdef ESP32
#else
#include <STM32FreeRTOS.h>
#endif

#include <frt/frt.h>

#include "driver/i2s.h"
#include "driver/rtc_io.h"

#define RECORD_TEMPERATURE "temperature"
#define TEMP_SENS_OVERSAMPLING 16
#define TEMP_SENS_INTERVAL_MS 250
#define DMA_MODE 1
#define DMA_BUF_SIZE 1000

namespace frt
{
    class TemperatureService : public frt::Task<TemperatureService, 2048>
    {
    private:
        frt::Publisher<frt::msgs::Temperature> *_pub;
        uint16_t _samples[DMA_BUF_SIZE];
        frt::Queue<i2s_event_t> _i2s_queue;

        int _pin;
        int _evt_pin;
        uint8_t _resolution_bits;
        float _nominal_resistance;
        float _reference_resistance;
        float _beta;
        float _nominal_temperature;
        float _corr_factor;

        float convert_to_deg_c(uint32_t sample)
        {
            float temperature;
            float average = (float)sample;

            average = ((1 << _resolution_bits) - 1) / average - 1.0;
            average = _reference_resistance / average;
            temperature = average / _nominal_resistance;          // (R/Ro)
            temperature = log(temperature);                       // ln(R/Ro)
            temperature /= _beta;                                 // 1/B * ln(R/Ro)
            temperature += 1.0 / (_nominal_temperature + 273.15); // + (1/To)
            temperature = 1.0 / temperature;                      // Invert
            temperature -= 273.15;                                // convert absolute temp to C
            temperature *= _corr_factor;

            return temperature;
        }

    public:
        TemperatureService(int pin, uint8_t resolution_bits, float nominal_resistance, float reference_resistance, float beta, float nominal_temperature, float corr_factor = 1.0F);
        virtual ~TemperatureService() {}
        bool run() override;
    };
}
#endif // __TEMPERATURE_SVC_H__