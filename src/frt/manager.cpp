#include "manager.h"

using namespace frt;

Manager *frt::Manager::instance = nullptr;

Manager *Manager::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Manager();
    }
    return instance;
}

bool Manager::removePublisher(const char *topic)
{
    size_t key = hash_cstr_gnu(topic);
    size_t deleted_pubs = publishers.erase(key);

    return deleted_pubs > 0;
}

// bool Manager::addNode(const char *name, Node *node)
// {
//     auto ret = nodes.emplace(name, node);

//     return ret.second;
// }

// bool Manager::removeNode(const char *name)
// {
//     size_t deleted_nodes = nodes.erase(name);

//     return deleted_nodes > 0;
// }

// Node *Manager::getNode(const char *name)
// {
//     return nodes[name];
// }