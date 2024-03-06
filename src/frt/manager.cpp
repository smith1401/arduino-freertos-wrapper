#include "manager.h"

using namespace frt;

Manager *Manager::instance{nullptr};
Mutex Manager::mutex;
// std::map<size_t, IPublisher *> Manager::publishers;
// std::map<size_t, ITask *> Manager::tasks;
std::unordered_map<size_t, IPublisher *> Manager::publishers;
std::unordered_map<size_t, ITask *> Manager::tasks;

Manager *Manager::getInstance()
{
    LockGuard lock(mutex);

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

bool frt::Manager::addTask(ITask *t, const char *name)
{
    size_t key = hash_cstr_gnu(name);

    // Try to emplace this publisher
    auto ret = tasks.emplace(key, t);

    return ret.second;
}

bool frt::Manager::removeTask(const char *name)
{
    size_t key = hash_cstr_gnu(name);
    size_t deleted_tasks = tasks.erase(key);

    return deleted_tasks > 0;
}
