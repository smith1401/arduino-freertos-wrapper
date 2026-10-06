#ifndef __FRT_MANAGER_H__
#define __FRT_MANAGER_H__

#include <Arduino.h>
#include <vector>
#include <cstring>

#include "mutex.h"

#ifndef FRT_TOPIC_MAX_LEN
#define FRT_TOPIC_MAX_LEN 24
#endif

namespace frt
{
    class ITask;

    template <typename T>
    class Publisher;

    template <typename T, unsigned int QUEUE_SIZE>
    class Subscriber;

    namespace detail
    {
        /**
         *  Compile time type identifier that works without RTTI: every
         *  instantiation has its own static member, and therefore its own
         *  unique address.
         */
        template <typename T>
        struct TypeTag
        {
            static const char id;
        };

        template <typename T>
        const char TypeTag<T>::id = 0;

        template <typename T>
        inline const void *typeId()
        {
            return &TypeTag<T>::id;
        }
    }

    /**
     *  Type erased base of all publishers. Holds the topic name and the
     *  message type, so a topic can never be used with two different types.
     */
    class IPublisher
    {
    public:
        virtual ~IPublisher() {}

        const char *topic() const { return _topic; }
        const void *type() const { return _type; }
        virtual size_t subscriberCount() const = 0;

    protected:
        IPublisher(const char *topic, const void *type) : _type(type)
        {
            strncpy(_topic, topic, sizeof(_topic) - 1);
            _topic[sizeof(_topic) - 1] = '\0';
        }

    private:
        char _topic[FRT_TOPIC_MAX_LEN];
        const void *_type;
    };

    /**
     *  Global registry of running tasks and pub/sub topics.
     *  All methods are thread safe, but must not be called from an ISR.
     */
    class Manager
    {
    private:
        Mutex _mutex;
        std::vector<IPublisher *> _publishers;
        std::vector<ITask *> _tasks;

        Manager() {}

        // Must be called with _mutex held
        IPublisher *findPublisherLocked(const char *topic) const;

    public:
        Manager(const Manager &other) = delete;
        Manager &operator=(const Manager &) = delete;

        static Manager *getInstance();

        // ---- Tasks ----
        bool addTask(ITask *t);
        bool removeTask(ITask *t);

        /** Snapshot of all registered (started) frt tasks. */
        std::vector<ITask *> getTasks();

        /** Find a registered task by name, nullptr if not found. */
        ITask *findTask(const char *name);

        // ---- Pub/Sub ----
        /**
         *  Remove a topic. This only succeeds if the topic has no subscribers
         *  left. Existing Publisher pointers to the topic become invalid.
         */
        bool removePublisher(const char *topic);

        /** Number of topics that have been advertised or subscribed to. */
        size_t topicCount();

        /**
         *  Get the publisher of a topic, creating it if it does not exist yet.
         *  Returns nullptr if the topic already exists with another message type.
         */
        template <typename T>
        Publisher<T> *acquirePublisher(const char *topic)
        {
            LockGuard lock(_mutex);
            return acquirePublisherLocked<T>(topic);
        }

        /**
         *  Create a new subscriber for a topic (and the topic, if needed).
         *  Returns nullptr if the topic already exists with another message type.
         */
        template <typename T, unsigned int QUEUE_SIZE>
        Subscriber<T, QUEUE_SIZE> *acquireSubscriber(const char *topic)
        {
            LockGuard lock(_mutex);
            Publisher<T> *pub = acquirePublisherLocked<T>(topic);

            if (pub == nullptr)
                return nullptr;

            Subscriber<T, QUEUE_SIZE> *sub = new Subscriber<T, QUEUE_SIZE>(pub);
            pub->addSubscriber(sub);

            return sub;
        }

        /** @deprecated misspelled, use acquirePublisher() */
        template <typename T, unsigned int QUEUE_SIZE = 10>
        Publisher<T> *aquirePublisher(const char *topic)
        {
            return acquirePublisher<T>(topic);
        }

        /** @deprecated misspelled, use acquireSubscriber() */
        template <typename T, unsigned int QUEUE_SIZE = 10>
        Subscriber<T, QUEUE_SIZE> *aquireSubscriber(const char *topic)
        {
            return acquireSubscriber<T, QUEUE_SIZE>(topic);
        }

    private:
        template <typename T>
        Publisher<T> *acquirePublisherLocked(const char *topic)
        {
            IPublisher *existing = findPublisherLocked(topic);

            if (existing != nullptr)
            {
                // Same topic used with two different message types
                if (existing->type() != detail::typeId<T>())
                    return nullptr;

                return static_cast<Publisher<T> *>(existing);
            }

            Publisher<T> *pub = new Publisher<T>(topic);
            _publishers.push_back(pub);

            return pub;
        }
    };
}

#endif // __FRT_MANAGER_H__
