#ifndef __FRT_MANAGER_H__
#define __FRT_MANAGER_H__

#include <Arduino.h>
#include <map>
#include <functional>
#include <cstring>

#include "mutex.h"

namespace frt
{
    class IPublisher;

    template <typename T, unsigned int QUEUE_SIZE>
    class Publisher;

    template <typename T, unsigned int QUEUE_SIZE>
    class Subscriber;

    class Manager
    {
    private:
        static Manager *instance;
        static Mutex mutex;
        static std::map<size_t, IPublisher *> publishers;

        Manager() {}

        size_t hash_cstr_gnu(const char *s);

    public:
        Manager(Manager &other) = delete;
        void operator=(const Manager &) = delete;

        static Manager *getInstance();
        bool removePublisher(const char *topic);

        template <typename T, unsigned int QUEUE_SIZE = 10>
        Publisher<T, QUEUE_SIZE> *aquirePublisher(const char *topic)
        {
            size_t key = hash_cstr_gnu(topic);

            // Create a temporary publisher on the heap
            auto pub = new Publisher<T, QUEUE_SIZE>(topic);

            // Try to emplace this publisher
            auto ret = publishers.emplace(key, pub);

            // Delete temporary publisher if the publisher with this topic already exists
            if (!ret.second)
            {
                delete pub;
            }

            return static_cast<Publisher<T, QUEUE_SIZE> *>((*ret.first).second);
        }

        template <typename T, unsigned int QUEUE_SIZE = 10>
        Subscriber<T, QUEUE_SIZE> *aquireSubscriber(const char *topic)
        {
            Publisher<T, QUEUE_SIZE> *pub = aquirePublisher<T, QUEUE_SIZE>(topic);
            Subscriber<T, QUEUE_SIZE> *sub = new Subscriber<T, QUEUE_SIZE>(topic);

            pub->addSubscriber(sub);

            return sub;
        }
    };
}

#endif // __FRT_MANAGER_H__