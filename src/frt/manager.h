#ifndef __FRT_MANAGER_H__
#define __FRT_MANAGER_H__

#include <Arduino.h>
#include <map>
#include <functional>
#include <cstring>

namespace frt
{
    class IPublisher;

    template <typename T, unsigned int QUEUE_SIZE = 10>
    class Publisher;

    class Manager
    {
    private:
        static Manager *instance;
        std::map<size_t, IPublisher *> publishers;

        static size_t hash_cstr_gnu(const char *s)
        {
            const size_t seed = 0;
            return std::_Hash_bytes(s, std::strlen(s), seed);
        }

        Manager() {}

    public:
        Manager(Manager &other) = delete;
        void operator=(const Manager &) = delete;

        static Manager *getInstance();

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

        bool removePublisher(const char *topic);
    };
}

#endif // __FRT_MANAGER_H__