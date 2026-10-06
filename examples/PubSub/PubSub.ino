/*
 * frt example: typed publish / subscribe.
 *
 * One task publishes temperature messages on the "temperature" topic.
 * - LoggerTask subscribes with a queue of 10 and logs every message.
 * - The display task only cares about the latest value, so it subscribes
 *   with a queue of 1 (mailbox) and reads it at its own pace.
 *
 * Every subscriber gets its own copy of each message. A topic is bound to
 * one message type: subscribing with a different type returns nullptr.
 */
#include <Arduino.h>
#include <frt/frt.h>
#include <frt/task.h>
#include <frt/pubsub.h>
#include <frt/log.h>

#define TOPIC_TEMPERATURE "temperature"

class SensorTask final : public frt::Task<SensorTask, 2048>
{
protected:
    void init() override
    {
        m_pub = frt::pubsub::advertise<frt::msgs::Temperature>(TOPIC_TEMPERATURE);
        m_lastWake = xTaskGetTickCount();
    }

    bool run() override
    {
        frt::msgs::Temperature msg;
        msg.timestamp = millis();
        msg.temperature = 20.0f + (random(0, 100) / 10.0f);

        const size_t receivers = m_pub->publish(msg);
        FRT_LOG_DEBUG("Published %.1f C to %u subscribers", msg.temperature, (unsigned)receivers);

        msleepUntil(m_lastWake, 200);
        return true;
    }

private:
    frt::Publisher<frt::msgs::Temperature> *m_pub = nullptr;
    TickType_t m_lastWake = 0;
};

class LoggerTask final : public frt::Task<LoggerTask, 2048>
{
protected:
    void init() override
    {
        m_sub = frt::pubsub::subscribe<frt::msgs::Temperature, 10>(TOPIC_TEMPERATURE);
    }

    bool run() override
    {
        frt::msgs::Temperature msg;

        if (m_sub->receive(msg))
            FRT_LOG_INFO("[logger] %lu ms: %.1f C", (unsigned long)msg.timestamp, msg.temperature);

        return true;
    }

private:
    frt::Subscriber<frt::msgs::Temperature, 10> *m_sub = nullptr;
};

class DisplayTask final : public frt::Task<DisplayTask, 2048>
{
protected:
    void init() override
    {
        // Queue size 1: always holds the most recent value only
        m_sub = frt::pubsub::subscribe<frt::msgs::Temperature, 1>(TOPIC_TEMPERATURE);
    }

    bool run() override
    {
        frt::msgs::Temperature msg;

        if (m_sub->receive(msg, 0))
            FRT_LOG_INFO("[display] latest: %.1f C", msg.temperature);

        msleep(1000);
        return true;
    }

private:
    frt::Subscriber<frt::msgs::Temperature, 1> *m_sub = nullptr;
};

SensorTask sensor;
LoggerTask logger;
DisplayTask display;

void setup()
{
    Serial.begin(115200);

    FRT_LOG_REGISTER_STREAM(&Serial);
    FRT_LOG_LEVEL_INFO();
    FRT_LOG_INFO("--- FRT Examples: Publish / Subscribe ---");

    logger.start(2, "logger");
    display.start(1, "display");
    sensor.start(3, "sensor");

    frt::spin();
}

void loop()
{
#if defined(ESP32) || defined(NRF52)
    vTaskDelay(portMAX_DELAY);
#endif
}
