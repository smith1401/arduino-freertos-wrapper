#ifndef __FRT_MANAGER_H__
#define __FRT_MANAGER_H__

#include <Arduino.h>
// #include <map>
#include <unordered_map>
#include <functional>
#include <cstring>

#include "mutex.h"

namespace frt
{
    class IPublisher;
    class ITask;

    template <typename T, unsigned int QUEUE_SIZE>
    class Publisher;

    template <typename T, unsigned int QUEUE_SIZE>
    class Subscriber;

    template <typename T, unsigned int STACK_SIZE_BYTES>
    class Task;

    class Manager
    {
    private:
        static Manager *instance;
        static Mutex mutex;
        // static std::map<size_t, IPublisher *> publishers;
        // static std::map<size_t, ITask *> tasks;

        static std::unordered_map<size_t, IPublisher *> publishers;
        static std::unordered_map<size_t, ITask *> tasks;

        Manager() {}

        size_t hash_cstr_gnu(const char *s);

    public:
        Manager(Manager &other) = delete;
        void operator=(const Manager &) = delete;

        static Manager *getInstance();
        bool removePublisher(const char *topic);

        // std::map<size_t, ITask *> *getTasks() { return &tasks; }
        std::unordered_map<size_t, ITask *> *getTasks() { return &tasks; }
        bool addTask(ITask *t, const char *name);
        bool removeTask(const char *name);

        template <typename T, unsigned int QUEUE_SIZE = 10>
        Publisher<T, QUEUE_SIZE> *aquirePublisher(const char *topic)
        {
            LockGuard lock(mutex);
            size_t key = hash_cstr_gnu(topic);

            // Create a temporary publisher on the heap
            Publisher<T, QUEUE_SIZE> *pub = new Publisher<T, QUEUE_SIZE>(topic);

            // Try to emplace this publisher
            auto ret = publishers.emplace(key, pub);

            // Delete temporary publisher if the publisher with this topic already exists
            if (!ret.second)
            {
                // Serial.printf("Publisher %s already aquired\r\n", pub->topic());
                delete pub;
            }

            return static_cast<Publisher<T, QUEUE_SIZE> *>((*ret.first).second);
        }
        // Publisher<T, QUEUE_SIZE> *aquirePublisher(const char *topic)
        // {
        //     LockGuard lock(mutex);
        //     size_t key = hash_cstr_gnu(topic);
        //     Publisher<T, QUEUE_SIZE> *pub;

        //     // Check if key exists in map. If true return the existing element, if false create a new one and return this.
        //     auto it = publishers.find(key);
        //     if (it != publishers.end())
        //     {
        //         pub = static_cast<Publisher<T, QUEUE_SIZE> *>(publishers[key]);
        //     }
        //     else
        //     {
        //         pub = new Publisher<T, QUEUE_SIZE>(topic);
        //         publishers[key] = pub;
        //     }

        //     return pub;
        // }

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