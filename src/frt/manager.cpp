#include "manager.h"

using namespace frt;

Manager *Manager::instance{nullptr};
Mutex Manager::mutex;
std::map<size_t, IPublisher *> Manager::publishers;

Manager *Manager::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Manager();
    }
    return instance;
}

size_t Manager::hash_cstr_gnu(const char *s)
{
    const size_t seed = 0;
    return std::_Hash_bytes(s, std::strlen(s), seed);
}

bool Manager::removePublisher(const char *topic)
{
    size_t key = hash_cstr_gnu(topic);
    size_t deleted_pubs = publishers.erase(key);

    return deleted_pubs > 0;
}