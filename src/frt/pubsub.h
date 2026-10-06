#ifndef __FRT_PUBSUB_H__
#define __FRT_PUBSUB_H__

#include <Arduino.h>
#include <vector>
#include <algorithm>

#include "msgs.h"
#include "queue.h"
#include "manager.h"
#include "event_group.h" // not needed here, kept for code relying on it

namespace frt
{
    /**
     *  Type erased subscriber of messages of type T. Lets one Publisher<T>
     *  deliver to subscribers with different queue sizes.
     */
    template <typename T>
    class ISubscriber
    {
    public:
        virtual ~ISubscriber() {}
        virtual bool send(const T &msg, unsigned int msecs = FOREVER) = 0;
    };

    /**
     *  Publisher of a topic. There is exactly one publisher object per topic,
     *  obtained with frt::pubsub::advertise<T>(topic). Several tasks may
     *  publish on the same topic.
     *
     *  publish() can be called from tasks and from ISRs. Subscribing and
     *  unsubscribing must not happen at the same time as publishing from an
     *  ISR (do it during setup).
     */
    template <typename T>
    class Publisher final : public IPublisher
    {
    private:
        std::vector<ISubscriber<T> *> _subscribers;
        Mutex _mutex;

        explicit Publisher(const char *topic) : IPublisher(topic, detail::typeId<T>())
        {
        }

        ~Publisher()
        {
        }

        Publisher(const Publisher &other) = delete;
        Publisher &operator=(const Publisher &other) = delete;

        void addSubscriber(ISubscriber<T> *sub)
        {
            LockGuard lock(_mutex);
            _subscribers.push_back(sub);
        }

        bool removeSubscriber(ISubscriber<T> *sub)
        {
            LockGuard lock(_mutex);
            auto it = std::find(_subscribers.begin(), _subscribers.end(), sub);

            if (it != _subscribers.end())
            {
                _subscribers.erase(it);
                return true;
            }

            return false;
        }

    public:
        /**
         *  Send a message to all subscribers of this topic.
         *
         *  If a subscriber queue is full, its oldest message is dropped.
         *  msecs limits how long to wait if a queue is still full after that
         *  (only possible if several publishers race). From an ISR the call
         *  never blocks.
         *
         *  @return number of subscribers that received the message
         */
        size_t publish(const T &msg, unsigned int msecs = FOREVER)
        {
            size_t delivered = 0;

            if (FRT_IS_ISR())
            {
                for (ISubscriber<T> *sub : _subscribers)
                    delivered += sub->send(msg, 0) ? 1 : 0;

                return delivered;
            }

            LockGuard lock(_mutex);

            for (ISubscriber<T> *sub : _subscribers)
                delivered += sub->send(msg, msecs) ? 1 : 0;

            return delivered;
        }

        size_t subscriberCount() const override
        {
            return _subscribers.size();
        }

        friend class Manager;
        template <typename U, unsigned int Q>
        friend class Subscriber;
    };

    /**
     *  Subscriber of a topic, owning a queue of QUEUE_SIZE messages.
     *  Obtained with frt::pubsub::subscribe<T, QUEUE_SIZE>(topic) and released
     *  with frt::pubsub::unsubscribe(sub).
     *
     *  With QUEUE_SIZE == 1 the subscriber behaves like a mailbox and always
     *  holds the latest message.
     */
    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Subscriber final : public ISubscriber<T>
    {
    private:
        Queue<T, QUEUE_SIZE> _queue;
        Publisher<T> *_pub;

        explicit Subscriber(Publisher<T> *pub) : _pub(pub)
        {
        }

        ~Subscriber()
        {
        }

        Subscriber(const Subscriber &other) = delete;
        Subscriber &operator=(const Subscriber &other) = delete;

        bool detach()
        {
            return _pub->removeSubscriber(this);
        }

        // QUEUE_SIZE == 1: overwrite the current value
        bool sendImpl(const T &msg, unsigned int, std::true_type)
        {
            return _queue.override(msg);
        }

        // QUEUE_SIZE > 1: drop the oldest message if the queue is full
        bool sendImpl(const T &msg, unsigned int msecs, std::false_type)
        {
            if (_queue.isFull())
            {
                T dropped;
                _queue.pop(dropped, 0);
            }

            return _queue.push(msg, msecs);
        }

    public:
        const char *topic() const { return _pub->topic(); }

        bool addToSet(QueueSetHandle_t &setHandle)
        {
            return _queue.addToSet(setHandle);
        }

        bool canReceive(QueueSetMemberHandle_t &memberHandle) const
        {
            return _queue.isMember(memberHandle);
        }

        /** Deliver a message to this subscriber only. */
        bool send(const T &msg, unsigned int msecs = FOREVER) override
        {
            return sendImpl(msg, msecs, std::integral_constant<bool, QUEUE_SIZE == 1>());
        }

        /** Wait for the next message. */
        bool receive(T &msg, unsigned int msecs = FOREVER)
        {
            return _queue.pop(msg, msecs);
        }

        bool receive(T &msg, unsigned int msecs, unsigned int &remainder)
        {
            return _queue.pop(msg, msecs, remainder);
        }

        /** Read the next message without removing it. */
        bool peek(T &msg, unsigned int msecs = 0)
        {
            return _queue.peek(msg, msecs);
        }

        /** Number of messages waiting. */
        unsigned int available() const
        {
            return _queue.available();
        }

        /** Discard all waiting messages. */
        void clear()
        {
            _queue.clear();
        }

        friend class Manager;
        friend class Publisher<T>;
        template <typename U, unsigned int Q>
        friend bool pubsubUnsubscribe(Subscriber<U, Q> *sub);
    };

    template <typename U, unsigned int Q>
    bool pubsubUnsubscribe(Subscriber<U, Q> *sub)
    {
        if (sub == nullptr)
            return false;

        const bool removed = sub->detach();
        delete sub;

        return removed;
    }

    namespace pubsub
    {
        /**
         *  Get the publisher of a topic. Returns nullptr if the topic is
         *  already in use with a different message type.
         *
         *  The second template parameter is ignored and only kept for source
         *  compatibility. The queue size is a property of each subscriber.
         */
        template <typename M, unsigned int UNUSED = 0>
        Publisher<M> *advertise(const char *topic)
        {
            return Manager::getInstance()->acquirePublisher<M>(topic);
        }

        /**
         *  Subscribe to a topic. Every subscriber gets its own copy of each
         *  message, buffered in a queue of QUEUE_SIZE messages. Returns nullptr
         *  if the topic is already in use with a different message type.
         */
        template <typename M, unsigned int QUEUE_SIZE = 10>
        Subscriber<M, QUEUE_SIZE> *subscribe(const char *topic)
        {
            return Manager::getInstance()->acquireSubscriber<M, QUEUE_SIZE>(topic);
        }

        /**
         *  Stop receiving messages and delete the subscriber. The pointer must
         *  not be used afterwards.
         */
        template <typename M, unsigned int QUEUE_SIZE>
        bool unsubscribe(Subscriber<M, QUEUE_SIZE> *sub)
        {
            return pubsubUnsubscribe(sub);
        }
    }
}

#endif // __FRT_PUBSUB_H__
