#include "manager.h"
#include "task.h"

using namespace frt;

Manager *Manager::getInstance()
{
    // Constructed on first use, so it is safe to use from other static
    // initializers. The first call should happen before tasks run
    // concurrently (e.g. in setup()), which Task::start() takes care of.
    static Manager instance;
    return &instance;
}

IPublisher *Manager::findPublisherLocked(const char *topic) const
{
    for (IPublisher *pub : _publishers)
    {
        if (strncmp(pub->topic(), topic, FRT_TOPIC_MAX_LEN - 1) == 0)
            return pub;
    }

    return nullptr;
}

bool Manager::removePublisher(const char *topic)
{
    LockGuard lock(_mutex);

    for (auto it = _publishers.begin(); it != _publishers.end(); ++it)
    {
        if (strncmp((*it)->topic(), topic, FRT_TOPIC_MAX_LEN - 1) == 0)
        {
            if ((*it)->subscriberCount() > 0)
                return false;

            delete *it;
            _publishers.erase(it);
            return true;
        }
    }

    return false;
}

size_t Manager::topicCount()
{
    LockGuard lock(_mutex);
    return _publishers.size();
}

bool Manager::addTask(ITask *t)
{
    LockGuard lock(_mutex);

    for (ITask *existing : _tasks)
    {
        if (existing == t)
            return false;
    }

    _tasks.push_back(t);
    return true;
}

bool Manager::removeTask(ITask *t)
{
    LockGuard lock(_mutex);

    for (auto it = _tasks.begin(); it != _tasks.end(); ++it)
    {
        if (*it == t)
        {
            _tasks.erase(it);
            return true;
        }
    }

    return false;
}

std::vector<ITask *> Manager::getTasks()
{
    LockGuard lock(_mutex);
    return _tasks;
}

ITask *Manager::findTask(const char *name)
{
    LockGuard lock(_mutex);

    for (ITask *t : _tasks)
    {
        if (strncmp(t->name(), name, configMAX_TASK_NAME_LEN) == 0)
            return t;
    }

    return nullptr;
}
