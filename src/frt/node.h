#ifndef __FRT_NODE_H__
#define __FRT_NODE_H__

// #include <Arduino.h>
// #include <string>
// #include "frt_task.h"
// #include "frt_pubsub.h"

// namespace frt
// {
//     class Node : public frt::Task<Node>
//     {
//     public:
//         Node(const char *name = "") : _name(name)
//         {
//             auto man = Manager::getInstance();
//             man->addNode(_name, this);
//         }

//         ~Node()
//         {
//             auto man = Manager::getInstance();
//             man->removeNode(_name);
//         }

//         template <typename M>
//         Publisher<M> advertise(const char *topic, uint32_t queue_size = 10)
//         {
//             return Publisher<M>(topic, queue_size);
//         }

//         template <typename M>
//         Subscriber<M> subscribe(const char *topic, uint32_t queue_size = 10)
//         {
//             return Subscriber<M>(topic, queue_size);
//         }

//     private:
//         const char *_name;
//     };
// }

#endif // __FRT_NODE_H__