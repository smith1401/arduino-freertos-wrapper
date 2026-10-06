/*
 * frt example: tasks, a queue, a mutex protected counter and a timer.
 *
 * - SensorTask samples a value every 100 ms (drift free, msleepUntil)
 *   and pushes it into a queue.
 * - PrinterTask pops values from the queue and logs them.
 * - A CallbackTimer toggles the LED every 500 ms.
 */
#include <Arduino.h>
#include <frt/frt.h>
#include <frt/task.h>
#include <frt/queue.h>
#include <frt/mutex.h>
#include <frt/timer.h>
#include <frt/log.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

struct Reading
{
    uint32_t timestamp;
    uint16_t value;
};

frt::Queue<Reading, 8> readings;
frt::Mutex statsMutex;
uint32_t totalReadings = 0;

class SensorTask final : public frt::Task<SensorTask, 2048>
{
protected:
    void init() override
    {
        m_lastWake = xTaskGetTickCount();
    }

    bool run() override
    {
        Reading r;
        r.timestamp = millis();
        r.value = analogRead(A0);

        // Do not block forever if the printer can not keep up
        if (!readings.push(r, 10))
            FRT_LOG_WARN("Queue full, reading dropped");

        msleepUntil(m_lastWake, 100);
        return true;
    }

private:
    TickType_t m_lastWake = 0;
};

class PrinterTask final : public frt::Task<PrinterTask, 2048>
{
protected:
    bool run() override
    {
        Reading r;

        if (readings.pop(r, 1000))
        {
            uint32_t total;
            {
                frt::LockGuard lock(statsMutex);
                total = ++totalReadings;
            }

            FRT_LOG_INFO("#%lu t=%lu value=%u", (unsigned long)total, (unsigned long)r.timestamp, r.value);
        }
        else
        {
            FRT_LOG_WARN("No reading for one second");
        }

        return true;
    }
};

SensorTask sensor;
PrinterTask printer;

frt::CallbackTimer blink("blink", pdMS_TO_TICKS(500), true, [] {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
});

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    FRT_LOG_REGISTER_STREAM(&Serial);
    FRT_LOG_LEVEL_INFO();
    FRT_LOG_INFO("--- FRT Examples: Basic Tasks ---");

    printer.start(1, "printer");
    sensor.start(2, "sensor");
    blink.start();

    // Starts the scheduler where the core does not (STM32), no-op elsewhere
    frt::spin();
}

void loop()
{
#if defined(ESP32) || defined(NRF52)
    // Nothing to do here, let the Arduino loop task sleep
    vTaskDelay(portMAX_DELAY);
#endif
    // On STM32 loop() is called from the idle task and must never block
}
