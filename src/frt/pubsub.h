#ifndef __FRT_PUBSUB_H__
#define __FRT_PUBSUB_H__

#include <Arduino.h>
#include <vector>
#include <functional>
#include <algorithm>

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

    template <typename T, unsigned int QUEUE_SIZE>
    class Subscriber;

    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Publisher : public IPublisher
    {
    private:
        const char *_topic;
        std::vector<Subscriber<T, QUEUE_SIZE> *> _subscribers;

        Publisher(const char *topic) : _topic(topic)
        {
        }

        ~Publisher()
        {
            _subscribers.clear();
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

    public:
        void publish(const T msg)
        {
            for (auto sub : _subscribers)
            {
                sub->send(msg);
            }
        }

        friend class Manager;
    };

    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Subscriber final
    {
    private:
        typedef std::function<void(const T *)> SubscriberCallback;
        Queue<T, QUEUE_SIZE> _queue;
        const char *_topic;

        Subscriber(const char *topic) : _topic(topic)
        {
        }

        ~Subscriber()
        {
        }

        explicit Subscriber(const Subscriber &other) = delete;
        Subscriber &operator=(const Subscriber &other) = delete;

    public:
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
                _queue.push(msg, 10);
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

        friend class Manager;
        friend class Publisher<T, QUEUE_SIZE>;
    };

    namespace pubsub
    {
        template <typename M, unsigned int QUEUE_SIZE = 10>
        Publisher<M, QUEUE_SIZE> *advertise(const char *topic)
        {
            Manager *man = Manager::getInstance();
            Publisher<M, QUEUE_SIZE> *pub = man->aquirePublisher<M, QUEUE_SIZE>(topic);

            return pub;
        }

        template <typename M, unsigned int QUEUE_SIZE = 10>
        Subscriber<M, QUEUE_SIZE> *subscribe(const char *topic)
        {
            Manager *man = Manager::getInstance();
            Subscriber<M, QUEUE_SIZE> *sub = man->aquireSubscriber<M, QUEUE_SIZE>(topic);

            return sub;
        }
    }
}

#endif // __FRT_PUBSUB_H__