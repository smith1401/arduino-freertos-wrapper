#include "temperature_svc.h"
// TODO: add implementation for STM32 and NRF52
#ifdef ESP32
frt::TemperatureService::TemperatureService(int pin, uint8_t resolution_bits, float nominal_resistance, float reference_resistance, float beta, float nominal_temperature, float corr_factor) : _pin(pin),
                                                                                                                                                                                                _resolution_bits(resolution_bits),
                                                                                                                                                                                                _nominal_resistance(nominal_resistance),
                                                                                                                                                                                                _reference_resistance(reference_resistance),
                                                                                                                                                                                                _beta(beta),
                                                                                                                                                                                                _nominal_temperature(nominal_temperature),
                                                                                                                                                                                                _corr_factor(corr_factor)

{
#ifdef DMA_MODE
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = 10000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = DMA_BUF_SIZE,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    // Install and start i2s driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 2, _i2s_queue.handle());

    // Init ADC pad
    // TODO: find a way to get the channel and unit from the IO pin
    i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_6);

    // Enable the ADC
    // i2s_adc_enable(I2S_NUM_0);
#else
#ifdef ESP32
    pinMode(_pin, INPUT);
#else
    pinMode(_pin, INPUT_ANALOG);
#endif
    analogReadResolution(_resolution_bits);

    _samples.reserve(TEMP_SENS_OVERSAMPLING);
#endif

    // _evt_pin = 2;
    // pinMode(_evt_pin, OUTPUT);

    _pub = frt::pubsub::advertise<frt::msgs::Temperature>(RECORD_TEMPERATURE);
}

bool frt::TemperatureService::run()
{
#ifdef DMA_MODE
    i2s_event_t evt;
    if (_i2s_queue.pop(evt))
    {
        // I2S_EVENT_DMA_ERROR,
        // I2S_EVENT_TX_DONE,     /*!< I2S DMA finish sent 1 buffer*/
        // I2S_EVENT_RX_DONE,     /*!< I2S DMA finish received 1 buffer*/
        // I2S_EVENT_TX_Q_OVF,    /*!< I2S DMA sent queue overflow*/
        // I2S_EVENT_RX_Q_OVF,    /*!< I2S DMA receive queue overflow*/
        // I2S_EVENT_MAX,         /*!< I2S event max index*/
        if (evt.type == I2S_EVENT_RX_DONE)
        {
            size_t bytes_read;

            // digitalWrite(_evt_pin, HIGH);
            i2s_adc_enable(I2S_NUM_0);
            i2s_read(I2S_NUM_0, _samples, DMA_BUF_SIZE * 2, &bytes_read, portMAX_DELAY);
            i2s_adc_disable(I2S_NUM_0);
            // digitalWrite(_evt_pin, LOW);
            // digitalWrite(_evt_pin, !digitalRead(_evt_pin));

            // uint32_t mean = std::accumulate(_samples.begin(), _samples.end(), 0);
            // uint32_t mean = 0;
            // for (size_t i = 0; i < DMA_BUF_SIZE; i++)
            // {
            //     // calculate mean and convert 16 bit value to 12 bit value
            //     mean += _samples[i] - 0x6000;
            // }
            // mean /= DMA_BUF_SIZE;

            float median;
            // Step 1: Sort the array
            std::sort(_samples, _samples + DMA_BUF_SIZE);

            // Step 2: Check if the size is odd or even
            if (DMA_BUF_SIZE % 2 == 1)
            {
                // If the size is odd, return the middle element
                median = _samples[DMA_BUF_SIZE / 2];
            }
            else
            {
                // If the size is even, return the average of the two middle elements
                median = (_samples[DMA_BUF_SIZE / 2 - 1] + _samples[DMA_BUF_SIZE / 2]) / 2.0;
            }

            // Convert 16 bit number to 12 bit number by subtracting 4095
            median -= 0x6000;

            frt::msgs::Temperature t;
            t.timestamp = xTaskGetTickCount();
            t.temperature = convert_to_deg_c(median);

            FRT_LOG_DEBUG("I2S bytes read: %d -> %.2f [%.2f C]", bytes_read, (median / 4095.0) * 1023.0, t.temperature);
            if (t.temperature < 0.0f)
                t.temperature = 0.0f;

            _pub->publish(t);

        }
    }
#else
    msleep(TEMP_SENS_INTERVAL_MS);
    // FRT_LOG_DEBUG("TEMP TIMER");
    for (size_t i = 0; i < TEMP_SENS_OVERSAMPLING; i++)
    {
        _samples.push_back(analogRead(_pin));
        digitalWrite(_evt_pin, !digitalRead(_evt_pin));
    }

    auto const count = static_cast<uint32_t>(_samples.size());
    uint32_t mean = std::accumulate(_samples.begin(), _samples.end(), 0.0F) / count;
    _samples.clear();

    frt::msgs::Temperature t;
    t.timestamp = xTaskGetTickCount();
    t.temperature = convert_to_deg_c(mean);
    _pub->publish(t);

// FRT_LOG_DEBUG("RAW: %d TEMP: %.2f V: %d", mean, t.temperature, analogReadMilliVolts(_pin));
#endif

    return true;
}

#endif