#ifndef __FRT_MANAGER_H__
#define __FRT_MANAGER_H__

#include <Arduino.h>
#include <map>
#include <functional>
#include <cstring>

namespace frt
{
    class IPublisher;

    template <typename T>
    class Publisher;

    // class Node;

    class Manager
    {
    private:
        static Manager *instance;
        std::map<size_t, IPublisher *> publishers;
        // std::map<const char *, Node *> nodes;

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

        template <typename T>
        Publisher<T> *aquirePublisher(const char *topic)
        {
            size_t key = hash_cstr_gnu(topic);

            uint32_t s = publishers.size();

            // Create a temporary publisher on the heap
            auto pub = new Publisher<T>(topic);

            // Try to emplace this publisher
            auto ret = publishers.emplace(key, pub);

            s = publishers.size();

            // Delete temporary publisher if the publisher with this topic already exists
            if (!ret.second)
            {
                delete pub;
            }

            return static_cast<Publisher<T> *>((*ret.first).second);
        }

        bool removePublisher(const char *topic);

        // bool addNode(const char *name, Node *node);
        // bool removeNode(const char *name);
        // Node *getNode(const char *name);
    };
}

#endif // __FRT_MANAGER_H__