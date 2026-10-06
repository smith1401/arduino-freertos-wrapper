/*
 * Host tests for the frt wrapper, running on the FreeRTOS POSIX port.
 * All tests run inside a FreeRTOS task, one after the other.
 */
#include <Arduino.h>

#include <frt/frt.h>
#include <frt/task.h>
#include <frt/queue.h>
#include <frt/mutex.h>
#include <frt/timer.h>
#include <frt/event_group.h>
#include <frt/messagebuffer.h>
#include <frt/pubsub.h>
#include <frt/log.h>

#include <stdio.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                       \
    do                                                                    \
    {                                                                     \
        g_checks++;                                                       \
        if (!(cond))                                                      \
        {                                                                 \
            g_failures++;                                                 \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do                                                                                 \
    {                                                                                  \
        g_checks++;                                                                    \
        const long long _a = static_cast<long long>(a);                                \
        const long long _b = static_cast<long long>(b);                                \
        if (_a != _b)                                                                  \
        {                                                                              \
            g_failures++;                                                              \
            printf("    FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__,    \
                   #a, #b, _a, _b);                                                    \
        }                                                                              \
    } while (0)

static void sleepMs(unsigned int ms)
{
    vTaskDelay(frt::detail::msToTicks(ms));
}

// Poll a condition for up to timeoutMs
static bool waitFor(std::function<bool()> cond, unsigned int timeoutMs = 2000)
{
    const TickType_t start = xTaskGetTickCount();

    while (!cond())
    {
        if (xTaskGetTickCount() - start > frt::detail::msToTicks(timeoutMs))
            return false;
        vTaskDelay(1);
    }

    return true;
}

static const unsigned int TEST_STACK = 64 * 1024;

// Task running a std::function, exposes the protected helpers to the tests
class FnTask : public frt::Task<FnTask, TEST_STACK>
{
public:
    explicit FnTask(std::function<bool(FnTask &)> fn) : m_fn(fn) {}
    ~FnTask() { stop(); }

    using Task::msleep;
    using Task::msleepUntil;
    using Task::shouldStop;
    using Task::wait;
    using Task::waitNotification;

    bool run() override { return m_fn(*this); }

private:
    std::function<bool(FnTask &)> m_fn;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_ms_to_ticks()
{
    using frt::detail::msToTicks;
    CHECK_EQ(msToTicks(0), 0);
    CHECK(msToTicks(frt::FOREVER) == portMAX_DELAY);
    CHECK_EQ(msToTicks(1), 1);
    CHECK_EQ(msToTicks(1500), 1500);
    CHECK(msToTicks(frt::FOREVER - 1) != portMAX_DELAY);

    using frt::detail::msToTicksCarry;
    unsigned int remainder = 0;
    CHECK_EQ(msToTicksCarry(0, remainder), 0);
    CHECK_EQ(remainder, 0);
    CHECK_EQ(msToTicksCarry(25, remainder), 25);
    CHECK_EQ(remainder, 0);
    // A pending remainder is added, FOREVER does not overflow
    remainder = 3;
    CHECK_EQ(msToTicksCarry(7, remainder), 10);
    remainder = 3;
    CHECK(msToTicksCarry(frt::FOREVER, remainder) == portMAX_DELAY);
    CHECK_EQ(remainder, 0);
}

static void test_critical_section()
{
    {
        frt::CriticalSection a;
        frt::CriticalSection b;
    }

    FRT_CRITICAL_ENTER();
    FRT_CRITICAL_ENTER();
    CHECK_EQ(frt::detail::criticalDepth(), 2);
    FRT_CRITICAL_EXIT();
    FRT_CRITICAL_EXIT();
    CHECK_EQ(frt::detail::criticalDepth(), 0);
}

static void test_queue_basic()
{
    frt::Queue<int, 3> q;
    int v = -1;

    CHECK(q.isEmpty());
    CHECK_EQ(q.availableForWrite(), 3);
    CHECK_EQ(q.capacity(), 3);

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.pushFront(0));
    CHECK(q.isFull());
    CHECK_EQ(q.available(), 3);
    CHECK(!q.push(9, 0));

    // peek used to pass a third argument to xQueuePeekFromISR and did not compile
    CHECK(q.peek(v, 0));
    CHECK_EQ(v, 0);
    CHECK(q.pop(v));
    CHECK_EQ(v, 0);
    CHECK(q.pop(v, 0));
    CHECK_EQ(v, 1);
    CHECK(q.pop(v, 0));
    CHECK_EQ(v, 2);

    // A timed out pop must wait (roughly) the requested time
    const TickType_t t0 = xTaskGetTickCount();
    CHECK(!q.pop(v, 20));
    CHECK(xTaskGetTickCount() - t0 >= 20);

    // Non blocking pop on an empty queue
    CHECK(!q.pop(v, 0));

    // push with remainder used to report failure on success and vice versa
    unsigned int remainder = 0;
    CHECK(q.push(5, 10, remainder));
    CHECK_EQ(remainder, 0);
    CHECK_EQ(q.available(), 1);
    CHECK(q.push(6, 0, remainder));
    CHECK(q.push(7, 0, remainder));
    CHECK(!q.push(8, 0, remainder));

    q.clear();
    CHECK(q.isEmpty());

    frt::Queue<int, 1> box;
    CHECK(box.override(1));
    CHECK(box.override(2));
    CHECK(box.pop(v, 0));
    CHECK_EQ(v, 2);
}

static void test_queue_between_tasks()
{
    frt::Queue<uint32_t, 4> q;
    const uint32_t count = 200;

    uint32_t sent = 0;
    FnTask producer([&](FnTask &) {
        q.push(sent++);
        return sent < count;
    });

    producer.start(2, "producer");

    uint64_t sum = 0;
    bool inOrder = true;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t v = 0;
        if (!q.pop(v, 1000))
        {
            inOrder = false;
            break;
        }
        inOrder = inOrder && v == i;
        sum += v;
    }

    CHECK(inOrder);
    CHECK_EQ(sum, (uint64_t)count * (count - 1) / 2);
}

static void test_mutex()
{
    frt::Mutex m;
    std::atomic<int> step(0);
    std::atomic<bool> otherTry(true), otherTimed(true), otherLater(false);

    m.lock();
    CHECK(m.isLockedByCurrentTask());

    FnTask other([&](FnTask &) {
        otherTry = m.try_lock();
        otherTimed = m.lock(10);
        step = 1;
        while (step != 2)
            vTaskDelay(1);
        otherLater = m.lock(1000);
        if (otherLater)
            m.unlock();
        step = 3;
        return false;
    });

    other.start(2, "mutex_other");
    CHECK(waitFor([&] { return step == 1; }));
    CHECK(!otherTry);
    CHECK(!otherTimed);

    m.unlock();
    CHECK(!m.isLockedByCurrentTask());
    step = 2;
    CHECK(waitFor([&] { return step == 3; }));
    CHECK(otherLater);

    {
        std::lock_guard<frt::Mutex> guard(m);
        CHECK(m.isLockedByCurrentTask());
    }
    {
        frt::LockGuard guard(m);
        CHECK(m.isLockedByCurrentTask());
    }
    CHECK(!m.isLockedByCurrentTask());
    CHECK(m.try_lock());
    m.unlock();
}

static void test_recursive_mutex()
{
    frt::RecursiveMutex m;
    std::atomic<int> result(-1);

    m.lock();
    m.lock();

    FnTask other([&](FnTask &) {
        result = m.try_lock() ? 1 : 0;
        if (result == 1)
            m.unlock();
        return false;
    });

    other.start(2, "rmutex");
    CHECK(waitFor([&] { return result != -1; }));
    CHECK_EQ(result, 0);
    CHECK(waitFor([&] { return !other.isRunning(); }));

    m.unlock();
    m.unlock();

    result = -1;
    other.start(2, "rmutex");
    CHECK(waitFor([&] { return result != -1; }));
    CHECK_EQ(result, 1);
}

static void test_semaphore()
{
    frt::Semaphore counting(frt::Semaphore::Type::COUNTING, 10, 0);
    CHECK(counting.post());
    CHECK(counting.post());
    CHECK(counting.post());
    CHECK_EQ(counting.count(), 3);
    CHECK(counting.wait(0));
    CHECK(counting.wait(0));
    CHECK(counting.wait(0));
    CHECK(!counting.wait(0));

    frt::Semaphore binary;
    CHECK(binary.post());
    CHECK(!binary.post());
    CHECK_EQ(binary.count(), 1);
    CHECK(binary.wait());
    CHECK(!binary.wait(5));
}

static void test_event_group()
{
    frt::EventGroup eg;

    FnTask setter([&](FnTask &t) {
        t.msleep(10);
        eg.setBits(0x1);
        return false;
    });

    setter.start(2, "setter");
    CHECK(eg.waitAll(0x1, 1000, false));
    CHECK(!eg.waitAny(0x2, 10));
    CHECK_EQ(eg.getBits(), 0x1);
    CHECK_EQ(eg.clearBits(0x1), 0x1);
    CHECK_EQ(eg.getBits(), 0);

    eg.setBits(0x6);
    CHECK(eg.waitAny(0x2, 0));
    CHECK_EQ(eg.getBits(), 0x4);

    unsigned int remainder = 0;
    const EventBits_t bits = eg.waitBits(0x4, true, true, 0, remainder);
    CHECK_EQ(bits & 0x4, 0x4);
    CHECK_EQ(eg.getBits(), 0);
}

static void test_message_buffer()
{
    frt::MessageBuffer<64> mb;
    char out[16] = {0};

    CHECK(mb.isEmpty());
    CHECK(mb.send("hello", 5));
    CHECK(mb.send("world!!", 7, 0));
    CHECK(!mb.isEmpty());
    CHECK(mb.availableForWrite() < 64);
    CHECK(mb.getFillLevel() > 0);
    CHECK_EQ(mb.nextMessageSize(), 5);

    CHECK_EQ(mb.receive(out, sizeof(out)), 5);
    CHECK(memcmp(out, "hello", 5) == 0);

    // Too small for the next message: nothing is received
    CHECK_EQ(mb.receive(out, 3, 0), 0);
    CHECK_EQ(mb.receive(out, sizeof(out), 0), 7);
    CHECK(memcmp(out, "world!!", 7) == 0);

    CHECK_EQ(mb.receive(out, sizeof(out), 5), 0);
    CHECK(mb.send("x", 1));
    CHECK(mb.clear());
    CHECK(mb.isEmpty());
}

static void test_timer()
{
    std::atomic<int> oneShot(0), periodic(0);

    frt::CallbackTimer once("once", pdMS_TO_TICKS(10), false, [&] { oneShot++; });
    CHECK(!once.isActive());
    CHECK(once.start());
    sleepMs(50);
    CHECK_EQ(oneShot, 1);
    CHECK(!once.isActive());
    CHECK(strcmp(once.name(), "once") == 0);

    frt::CallbackTimer every(pdMS_TO_TICKS(5), true, [&] { periodic++; });
    CHECK(every.start());
    CHECK(every.isActive());
    sleepMs(52);
    CHECK(every.stop());
    const int fired = periodic;
    CHECK(fired >= 8 && fired <= 11);
    sleepMs(20);
    CHECK_EQ(periodic, fired);

    CHECK(every.setPeriodMs(20));
    CHECK(every.period() == pdMS_TO_TICKS(20));
    CHECK(every.stop());
}

static void test_timer_destroy_while_running()
{
    // The timer task runs at the same priority as this helper, so it
    // processes the delete command only after the destructor returned.
    // Without waiting for that, the static timer buffer inside the freed
    // object is accessed afterwards (caught by AddressSanitizer).
    std::atomic<int> fired(0);
    std::atomic<bool> done(false);

    FnTask churn([&](FnTask &) {
        for (int i = 0; i < 50; i++)
        {
            frt::CallbackTimer *t = new frt::CallbackTimer(1, true, [&] { fired++; });
            t->start();
            vTaskDelay(2);
            delete t;
        }
        done = true;
        return false;
    });

    churn.start(configTIMER_TASK_PRIORITY, "churn");
    CHECK(waitFor([&] { return done.load(); }, 5000));
    CHECK(fired > 0);
}

static void test_task_lifecycle()
{
    std::atomic<int> counter(0);
    std::atomic<int> inits(0);

    FnTask counting([&](FnTask &t) {
        counter++;
        t.msleep(1);
        return counter % 10 != 0;
    });

    // The name is copied, the caller's buffer may change afterwards
    char name[16];
    strcpy(name, "counting");
    CHECK(counting.start(2, name) != nullptr);
    memset(name, 'x', sizeof(name) - 1);
    CHECK(strcmp(counting.name(), "counting") == 0);

    CHECK(waitFor([&] { return counter == 10 && !counting.isRunning(); }));
    // A task that ended on its own leaves the registry
    CHECK(frt::Manager::getInstance()->findTask("counting") == nullptr);

    // ... and can be started again
    CHECK(counting.start(2, "counting") != nullptr);
    CHECK(waitFor([&] { return counter == 20 && !counting.isRunning(); }));
    CHECK(counting.getHandle() != nullptr || !counting.isRunning());
    FRT_UNUSED(inits);
}

static void test_task_stop_blocked()
{
    std::atomic<int> runs(0);

    // Blocks forever in wait(), stop() must still return
    FnTask blocked([&](FnTask &t) {
        runs++;
        t.wait();
        return true;
    });

    CHECK(blocked.start(2, "blocked") != nullptr);
    // Starting a running task again is refused (it would reuse the stack)
    CHECK(blocked.start(2, "blocked") == nullptr);
    CHECK(waitFor([&] { return runs == 1; }));

    const TickType_t t0 = xTaskGetTickCount();
    CHECK(blocked.stop());
    CHECK(xTaskGetTickCount() - t0 < 100);
    CHECK(!blocked.isRunning());
    CHECK(blocked.getHandle() == nullptr);
    CHECK(!blocked.stop());
}

static void test_task_stop_self()
{
    std::atomic<int> runs(0);

    FnTask self([&](FnTask &t) {
        runs++;
        if (runs == 3)
            t.stop(); // used to dead lock
        t.msleep(1);
        return true;
    });

    self.start(2, "self");
    CHECK(waitFor([&] { return runs >= 3 && !self.isRunning(); }));
    CHECK_EQ(runs, 3);
}

static void test_task_unnamed_registry()
{
    frt::Manager *man = frt::Manager::getInstance();
    const size_t before = man->getTasks().size();

    FnTask a([](FnTask &t) { t.wait(); return true; });
    FnTask b([](FnTask &t) { t.wait(); return true; });

    a.start(2);
    b.start(2);

    // Unnamed tasks all had the same key before and replaced each other
    CHECK_EQ(man->getTasks().size(), before + 2);

    a.stop();
    CHECK_EQ(man->getTasks().size(), before + 1);
    b.stop();
    CHECK_EQ(man->getTasks().size(), before);
}

static void test_task_notifications()
{
    std::atomic<int> posts(0);
    std::atomic<uint32_t> value(0);

    FnTask waiter([&](FnTask &t) {
        if (t.wait(1000))
            posts++;
        return true;
    });

    waiter.start(2, "waiter");
    waiter.post();
    waiter.post();
    waiter.post();
    CHECK(waitFor([&] { return posts == 3; }));
    waiter.stop();

    FnTask notified([&](FnTask &t) {
        uint32_t v = 0;
        if (t.waitNotification(v, 1000))
            value = value | v;
        return true;
    });

    notified.start(2, "notified");
    sleepMs(5);
    CHECK(notified.notify(0x10));
    CHECK(notified.notify(0x01));
    CHECK(waitFor([&] { return value == 0x11; }));
}

static void test_task_sleep_until()
{
    std::vector<TickType_t> wakeups;
    TickType_t last = 0;

    FnTask periodic([&](FnTask &t) {
        if (wakeups.empty())
            last = xTaskGetTickCount();
        t.msleepUntil(last, 5);
        // The scheduled wake time is exact, the actual one depends on host load
        wakeups.push_back(last);
        return wakeups.size() < 10;
    });

    periodic.start(3, "periodic");
    CHECK(waitFor([&] { return !periodic.isRunning(); }));
    CHECK_EQ(wakeups.size(), 10);
    if (wakeups.size() == 10)
    {
        for (size_t i = 1; i < wakeups.size(); i++)
            CHECK_EQ(wakeups[i] - wakeups[i - 1], 5);
    }
}

static void test_suspend_other_tasks()
{
    std::atomic<int> ticks(0);

    FnTask worker([&](FnTask &t) {
        ticks++;
        t.msleep(1);
        return true;
    });

    // A finished task must not make suspendOtherTasks() suspend the caller
    FnTask finished([](FnTask &) { return false; });
    finished.start(2, "finished");
    CHECK(waitFor([&] { return !finished.isRunning(); }));

    worker.start(2, "worker");
    CHECK(waitFor([&] { return ticks > 3; }));

    frt::task::suspendOtherTasks();
    sleepMs(5);
    const int frozen = ticks;
    sleepMs(20);
    CHECK_EQ(ticks, frozen);

    frt::task::resumeOtherTasks();
    CHECK(waitFor([&] { return ticks > frozen + 3; }));
}

struct Sample
{
    uint32_t seq;
    float value;
};

static void test_pubsub_basic()
{
    frt::Publisher<Sample> *pub = frt::pubsub::advertise<Sample>("ps/basic");
    CHECK(pub != nullptr);
    CHECK(frt::pubsub::advertise<Sample>("ps/basic") == pub);
    // Old two-argument form still compiles, queue size belongs to subscribers
    CHECK((frt::pubsub::advertise<Sample, 5>("ps/basic")) == pub);

    // Different queue sizes on one topic used to static_cast the publisher
    // to the wrong type
    frt::Subscriber<Sample, 10> *many = frt::pubsub::subscribe<Sample, 10>("ps/basic");
    frt::Subscriber<Sample, 1> *latest = frt::pubsub::subscribe<Sample, 1>("ps/basic");
    CHECK(many != nullptr && latest != nullptr);
    CHECK_EQ(pub->subscriberCount(), 2);
    CHECK(strcmp(many->topic(), "ps/basic") == 0);

    for (uint32_t i = 1; i <= 3; i++)
        CHECK_EQ(pub->publish(Sample{i, i * 1.5f}), 2);

    Sample s;
    CHECK_EQ(many->available(), 3);
    CHECK(many->receive(s, 0) && s.seq == 1);
    CHECK(many->receive(s, 0) && s.seq == 2);
    CHECK(many->receive(s, 0) && s.seq == 3);
    CHECK(!many->receive(s, 0));

    CHECK_EQ(latest->available(), 1);
    CHECK(latest->peek(s) && s.seq == 3);
    CHECK(latest->receive(s, 0) && s.seq == 3);

    // Type mismatch on an existing topic is refused
    CHECK((frt::pubsub::subscribe<float, 10>("ps/basic")) == nullptr);
    CHECK(frt::pubsub::advertise<int>("ps/basic") == nullptr);

    // Unsubscribe / remove topic
    CHECK(!frt::Manager::getInstance()->removePublisher("ps/basic"));
    CHECK(frt::pubsub::unsubscribe(latest));
    CHECK_EQ(pub->subscriberCount(), 1);
    CHECK_EQ(pub->publish(Sample{4, 0}), 1);
    CHECK(frt::pubsub::unsubscribe(many));

    const size_t topics = frt::Manager::getInstance()->topicCount();
    CHECK(frt::Manager::getInstance()->removePublisher("ps/basic"));
    CHECK_EQ(frt::Manager::getInstance()->topicCount(), topics - 1);
}

static void test_pubsub_drop_oldest()
{
    frt::Publisher<int> *pub = frt::pubsub::advertise<int>("ps/drop");
    frt::Subscriber<int, 2> *sub = frt::pubsub::subscribe<int, 2>("ps/drop");

    pub->publish(1);
    pub->publish(2);
    pub->publish(3);

    int v = 0;
    CHECK(sub->receive(v, 0) && v == 2);
    CHECK(sub->receive(v, 0) && v == 3);
    CHECK(!sub->receive(v, 0));

    // Subscribing first also creates the topic
    frt::Subscriber<int, 4> *early = frt::pubsub::subscribe<int, 4>("ps/early");
    CHECK(early != nullptr);
    frt::pubsub::advertise<int>("ps/early")->publish(7);
    CHECK(early->receive(v, 0) && v == 7);

    frt::pubsub::unsubscribe(sub);
    frt::pubsub::unsubscribe(early);
}

static void test_pubsub_tasks_and_queue_set()
{
    const uint32_t count = 100;
    frt::Subscriber<uint32_t, count> *subA = frt::pubsub::subscribe<uint32_t, count>("ps/a");
    frt::Subscriber<uint32_t, count> *subB = frt::pubsub::subscribe<uint32_t, count>("ps/b");

    QueueSetHandle_t set = xQueueCreateSet(2 * count);
    CHECK(subA->addToSet(set));
    CHECK(subB->addToSet(set));

    uint32_t i = 0;
    FnTask producer([&](FnTask &) {
        frt::pubsub::advertise<uint32_t>(i % 2 ? "ps/b" : "ps/a")->publish(i);
        i++;
        return i < count;
    });

    producer.start(2, "producer");

    uint32_t receivedA = 0, receivedB = 0, sum = 0;
    for (uint32_t n = 0; n < count; n++)
    {
        QueueSetMemberHandle_t member = xQueueSelectFromSet(set, pdMS_TO_TICKS(1000));
        uint32_t v = 0;

        if (subA->canReceive(member) && subA->receive(v, 0))
        {
            receivedA++;
            sum += v;
        }
        else if (subB->canReceive(member) && subB->receive(v, 0))
        {
            receivedB++;
            sum += v;
        }
    }

    CHECK_EQ(receivedA, count / 2);
    CHECK_EQ(receivedB, count / 2);
    CHECK_EQ(sum, count * (count - 1) / 2);
}

static size_t countLines(const std::string &s)
{
    size_t lines = 0;
    for (size_t pos = s.find("\r\n"); pos != std::string::npos; pos = s.find("\r\n", pos + 2))
        lines++;
    return lines;
}

static void test_log()
{
    StringStream out;
    frt::Log *log = frt::Log::getInstance();

    FRT_LOG_REGISTER_STREAM(&out);
    FRT_LOG_LEVEL_INFO();

    FRT_LOG_DEBUG("hidden");
    CHECK(out.data.empty());

    FRT_LOG_INFO("hello %d", 42);
    CHECK(out.data.find("INFO") != std::string::npos);
    CHECK(out.data.find("hello 42\r\n") != std::string::npos);

    // Messages longer than the buffer used to overflow it
    out.data.clear();
    std::string big(4 * MAX_LOG_SIZE, 'a');
    FRT_LOG_ERROR("%s", big.c_str());
    CHECK(out.data.size() <= MAX_LOG_SIZE);
    CHECK(out.data.size() > MAX_LOG_SIZE / 2);
    CHECK(out.data.compare(out.data.size() - 2, 2, "\r\n") == 0);

    out.data.clear();
    std::vector<uint8_t> bytes(400, 0xAB);
    log->log_buffer(frt::LogLevel::ERROR, "bytes", bytes.data(), bytes.size());
    CHECK(out.data.size() <= MAX_LOG_SIZE);
    CHECK(out.data.compare(out.data.size() - 2, 2, "\r\n") == 0);

    // Concurrent logging used to share the format buffer without a lock
    out.data.clear();
    const int lines = 200;
    std::atomic<int> doneTasks(0);
    auto spam = [&](const char *tag) {
        return [&, tag](FnTask &) {
            for (int i = 0; i < lines; i++)
            {
                FRT_LOG_INFO("%s-%03d-%s", tag, i, "0123456789abcdefghijklmnopqrstuvwxyz");
                if (i % 16 == 0)
                    taskYIELD();
            }
            doneTasks++;
            return false;
        };
    };

    FnTask a(spam("A"));
    FnTask b(spam("B"));
    a.start(2, "logA");
    b.start(2, "logB");
    CHECK(waitFor([&] { return doneTasks == 2; }, 5000));

    CHECK_EQ(countLines(out.data), 2 * lines);
    size_t intact = 0;
    for (size_t pos = 0; pos < out.data.size();)
    {
        const size_t end = out.data.find("\r\n", pos);
        if (end == std::string::npos)
            break;
        const std::string line = out.data.substr(pos, end - pos);
        if (line.find("-0123456789abcdefghijklmnopqrstuvwxyz") == line.size() - 37 &&
            (line.find(" A-") != std::string::npos || line.find(" B-") != std::string::npos))
            intact++;
        pos = end + 2;
    }
    CHECK_EQ(intact, 2 * lines);

    log->unregisterStream(&out);
    out.data.clear();
    FRT_LOG_ERROR("gone");
    CHECK(out.data.empty());
    FRT_LOG_LEVEL_ERROR();
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

struct TestCase
{
    const char *name;
    void (*fn)();
};

static const TestCase tests[] = {
    {"ms_to_ticks", test_ms_to_ticks},
    {"critical_section", test_critical_section},
    {"queue_basic", test_queue_basic},
    {"queue_between_tasks", test_queue_between_tasks},
    {"mutex", test_mutex},
    {"recursive_mutex", test_recursive_mutex},
    {"semaphore", test_semaphore},
    {"event_group", test_event_group},
    {"message_buffer", test_message_buffer},
    {"timer", test_timer},
    {"timer_destroy_while_running", test_timer_destroy_while_running},
    {"task_lifecycle", test_task_lifecycle},
    {"task_stop_blocked", test_task_stop_blocked},
    {"task_stop_self", test_task_stop_self},
    {"task_unnamed_registry", test_task_unnamed_registry},
    {"task_notifications", test_task_notifications},
    {"task_sleep_until", test_task_sleep_until},
    {"suspend_other_tasks", test_suspend_other_tasks},
    {"pubsub_basic", test_pubsub_basic},
    {"pubsub_drop_oldest", test_pubsub_drop_oldest},
    {"pubsub_tasks_and_queue_set", test_pubsub_tasks_and_queue_set},
    {"log", test_log},
};

// Tests keep FnTask objects (which embed their stack) on the runner stack
class Runner : public frt::Task<Runner, 8 * 1024 * 1024>
{
public:
    bool run() override
    {
        for (const TestCase &t : tests)
        {
            const int failuresBefore = g_failures;
            printf("[ RUN  ] %s\n", t.name);
            t.fn();
            printf("[ %s ] %s\n", g_failures == failuresBefore ? " OK " : "FAIL", t.name);
        }

        printf("\n%d checks, %d failures (%s allocation)\n", g_checks, g_failures,
               configSUPPORT_STATIC_ALLOCATION ? "static" : "dynamic");
        fflush(stdout);
        _exit(g_failures == 0 ? 0 : 1);
        return false;
    }
};

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static Runner runner;
    runner.start(1, "runner");

    frt::spin();

    return 1;
}
