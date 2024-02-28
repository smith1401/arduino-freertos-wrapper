#ifndef __FRT_PUBSUB_H__
#define __FRT_PUBSUB_H__

#include <vector>
#include <functional>
#include <algorithm>
#include <Arduino.h>
#include "msgs.h"
#include "queue.h"
#include "manager.h"
#include "event_group.h"

namespace frt
{
    class IPublisher
    {
    public:
        virtual ~IPublisher() {}
    };

    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Subscriber;

    template <typename T, unsigned int QUEUE_SIZE>
    class Publisher : public IPublisher
    {
    private:
        const char *_topic;
        std::vector<Subscriber<T, QUEUE_SIZE> *> _subscribers;

    public:
        Publisher(const char *topic) : _topic(topic)
        {
        }

        ~Publisher()
        {
            // for (auto &sub : _subscribers)
            // {
            //     delete sub;
            // }
        }

        explicit Publisher(const Publisher &other) = delete;
        Publisher &operator=(const Publisher &other) = delete;

        void addSubscriber(Subscriber<T, QUEUE_SIZE> *sub)
        {
            _subscribers.push_back(sub);
        }

        bool removeSubscriber(Subscriber<T, QUEUE_SIZE> *sub)
        {
            auto it = std::find(_subscribers.begin(), _subscribers.end(), sub);

            if (it != _subscribers.end())
            {
                _subscribers.erase(it);
                return true;
            }

            return false;
        }

        void publish(const T msg)
        {
            for (auto sub : _subscribers)
            {
                sub->send(msg);
            }
        }
    };

    template <typename T, unsigned int QUEUE_SIZE>
    class Subscriber final
    {
    private:
        typedef std::function<void(const T *)> SubscriberCallback;
        Queue<T, QUEUE_SIZE> _queue;
        const char *_topic;
        EventGroup *_evt_grp;
        EventBits_t _evt_bit;

    public:
        Subscriber(const char *topic) : _topic(topic),
                                        _evt_grp(nullptr)
        {
            auto man = Manager::getInstance();
            auto pub = man->aquirePublisher<T, QUEUE_SIZE>(_topic);
            pub->addSubscriber(this);
        }

        ~Subscriber()
        {
            auto man = Manager::getInstance();
            auto pub = man->aquirePublisher<T>(_topic);
            pub->removeSubscriber(this);
        }

        void addEvent(EventGroup *evt_grp, const EventBits_t evt_bit)
        {
            _evt_grp = evt_grp;
            _evt_bit = evt_bit;
        }

        void addToSet(QueueSetHandle_t &setHandle)
        {
            _queue.addToSet(setHandle);
        }

        bool canReceive(QueueSetMemberHandle_t &memberHandle)
        {
            return _queue.isMember(memberHandle);
        }

        void send(const T &msg)
        {
            // If the size is just one, then override
            if (QUEUE_SIZE == 1)
            {
                _queue.override(msg);
            }
            // If the queue is full, pop the oldest elements and push the new one
            else if (_queue.getFillLevel() == QUEUE_SIZE)
            {
                T temp;
                _queue.pop(temp);
                _queue.push(msg);
            }
            // Else just push the element
            else
            {
                _queue.push(msg);
            }

            // If there is a synchronization queue available, set the corresponding bit
            if (_evt_grp != nullptr)
            {
                _evt_grp->setBits(_evt_bit);
            }
        }

        bool receive(T &msg)
        {
            return _queue.pop(msg);
        }

        bool receive(T &msg, unsigned int msecs)
        {
            return _queue.pop(msg, msecs);
        }

        bool receive(T &msg, unsigned int msecs, unsigned int &remainder)
        {
            return _queue.pop(msg, msecs, remainder);
        }

        explicit Subscriber(const Subscriber &other) = delete;
        Subscriber &operator=(const Subscriber &other) = delete;
    };

    namespace pubsub
    {
        template <typename M, unsigned int QUEUE_SIZE = 10>
        Publisher<M, QUEUE_SIZE> *advertise(const char *topic)
        {
            frt::Manager *man = Manager::getInstance();
            Publisher<M, QUEUE_SIZE> *pub = man->aquirePublisher<M, QUEUE_SIZE>(topic);

            return pub;
        }

        template <typename M, unsigned int QUEUE_SIZE = 10>
        Subscriber<M, QUEUE_SIZE> *subscribe(const char *topic)
        {
            Subscriber<M, QUEUE_SIZE> *sub = new Subscriber<M, QUEUE_SIZE>(topic);

            frt::Manager *man = Manager::getInstance();
            Publisher<M, QUEUE_SIZE> *pub = man->aquirePublisher<M, QUEUE_SIZE>(topic);

            pub->addSubscriber(sub);

            return sub;
        }
    }
}

#endif // __FRT_PUBSUB_H__