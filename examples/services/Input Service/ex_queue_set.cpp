#include <Arduino.h>
#include <frt/frt.h>
#include <frt/log.h>
#include <frt/pubsub.h>
#include <frt/task.h>

class PublisherTask : public frt::Task<PublisherTask>
{
public:
    PublisherTask(const char *topic) : _topic(topic)
    {
    }

    void init() override
    {
        _pub = frt::pubsub::advertise<uint32_t>(_topic);
    }

    bool run() override
    {
        unsigned int sleepTime = random(100, 2000);
        this->msleep(sleepTime);

        uint32_t data = random(1, 1000);
        _pub->publish(data);

        return true;
    }

    const char *topic() const { return _topic; }

private:
    frt::Publisher<uint32_t> *_pub;
    const char *_topic;
};

class SubscriberTask : public frt::Task<SubscriberTask, 2048>
{
public:
    SubscriberTask(uint32_t num_tasks) : _num_tasks(num_tasks)
    {
    }

    void init() override
    {
        randomSeed(analogRead(A0));

        for (size_t i = 0; i < _num_tasks; i++)
        {
            topics.push_back("topic" + String(i + 1));
        }

        queueSet = xQueueCreateSet(topics.size() * 10);

        for (auto &topic : topics)
        {
            tasksVec.push_back(new PublisherTask(topic.c_str()));
            subsVec.push_back(frt::pubsub::subscribe<uint32_t>(topic.c_str()));
            subsVec.back()->addToSet(queueSet);

            uint32_t prio = random(0, 3);
            tasksVec.back()->start(prio, tasksVec.back()->topic());
            FRT_LOG_INFO("PublisherTask started ['%8s', %d]", tasksVec.back()->topic(), prio);
        }
    }

    bool run() override
    {
        QueueSetMemberHandle_t setMember = xQueueSelectFromSet(queueSet, portMAX_DELAY);

        for (auto &sub : subsVec)
        {
            if (sub->canReceive(setMember))
            {
                uint32_t data;
                if (sub->receive(data))
                {
                    FRT_LOG_DEBUG("Subscriber with topic '%s' received %lu", sub->topic(), data);
                }
            }
        }

        return true;
    }

private:
    uint32_t _num_tasks;
    std::vector<String> topics;
    QueueSetHandle_t queueSet;

    std::vector<PublisherTask *> tasksVec;
    std::vector<frt::Subscriber<uint32_t> *> subsVec;
};

SubscriberTask *t;

void setup()
{
    Serial.begin(115200);

    FRT_LOG_REGISTER_STREAM(&Serial);
    FRT_LOG_LEVEL_DEBUG();

    FRT_LOG_INFO("--- FRT Examples: QueueSet ---");

    t = new SubscriberTask(10);
    t->start(tskIDLE_PRIORITY + 2, "sub_task");

    frt::spin();
}

void loop()
{
    suspendLoop();
}
