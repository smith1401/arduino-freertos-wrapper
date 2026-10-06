# FreeRTOS C++ Wrapper for Arduino

Object oriented, ISR aware C++ wrappers for FreeRTOS on Arduino, plus a typed
publish/subscribe system, a thread safe logger and ready made services
(debounced buttons, PID control).

```cpp
#include <frt/frt.h>
#include <frt/task.h>
#include <frt/pubsub.h>

class Blinker final : public frt::Task<Blinker, 1024>
{
protected:
    bool run() override
    {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        msleep(500);
        return true; // return false to end the task
    }
};

Blinker blinker;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    blinker.start(1, "blinker");
    frt::spin(); // starts the scheduler on STM32, no-op on ESP32 / nRF52
}

void loop() {}
```

## Features

| Header | Contents |
| --- | --- |
| `frt/task.h` | `frt::Task<T, STACK_BYTES>`: start / stop / restart, `msleep`, drift free `msleepUntil`, `post`/`wait` and `notify`/`waitNotification` (task notifications), priority, suspend / resume, ESP32 core pinning, stack usage |
| `frt/queue.h` | `frt::Queue<T, SIZE>`: type safe queue, push / pushFront / pop / peek / override, queue sets |
| `frt/mutex.h` | `frt::Mutex`, `frt::RecursiveMutex`, `frt::Semaphore` (binary / counting), `frt::LockGuard`. Mutexes work with `std::lock_guard` / `std::unique_lock` |
| `frt/timer.h` | `frt::Timer` (override `run()`) and `frt::CallbackTimer` (lambda) software timers |
| `frt/event_group.h` | `frt::EventGroup`: set / clear / wait any / wait all / sync |
| `frt/messagebuffer.h` | `frt::MessageBuffer<SIZE>` for variable length messages |
| `frt/pubsub.h` | Typed publish / subscribe between tasks and ISRs |
| `frt/log.h` | Thread safe logger (`FRT_LOG_INFO(...)`) writing to any `Stream` |
| `frt/frt.h` | `frt::CriticalSection` (RAII), `frt::FOREVER`, `frt::spin()` |
| `frt/services/*` | Input (buttons: press / release / short / long / repeat), PID, burst firing output, temperature |
| `frt/streams/*` | Log streams over UDP (ESP32) and SEGGER RTT (STM32, nRF52) |

Design rules used throughout:

* **Static allocation**: if `configSUPPORT_STATIC_ALLOCATION` is enabled, all
  kernel objects (including task stacks) live inside the C++ objects, so
  RAM usage is known at compile time.
* **ISR aware**: every method that FreeRTOS allows in an interrupt detects the
  ISR context and calls the matching `...FromISR` function (and yields if a
  higher priority task was woken). Calls that may block never block in an ISR.
* **Timeouts in milliseconds**: `0` never blocks, `frt::FOREVER` blocks until
  the operation succeeds, everything else is rounded up to at least one tick.
  Overloads taking an extra `unsigned int &remainder` carry the sub tick part
  of a timeout over to the next call (useful in loops).
* **Non-copyable**: kernel objects can not be copied by accident.

## Supported platforms

| Platform | Arduino core | FreeRTOS |
| --- | --- | --- |
| ESP32 | arduino-esp32 2.x and 3.x | built into the core |
| STM32 (F1, F2, F4, U5) | STM32duino | [STM32FreeRTOS](https://github.com/stm32duino/STM32FreeRTOS) library |
| nRF52 | Adafruit nRF52 | built into the core |

## Installation

**PlatformIO**: add the library to `platformio.ini`; dependencies are
installed automatically:

```ini
lib_deps = https://github.com/smith1401/arduino-freertos-wrapper.git
lib_ldf_mode = deep+
```

On nRF52 also add `Adafruit TinyUSB Library` to `lib_deps`, otherwise
PlatformIO does not link the USB `Serial` of the Adafruit core.

**Arduino IDE**: install the ZIP of this repository (Sketch → Include Library →
Add .ZIP Library). On STM32 also install *STM32duino FreeRTOS*. The optional
services need [BindArg](https://github.com/openlab-vn-ua/BindArg) (input and
output services on STM32 / nRF52) and
[CircularBuffer](https://github.com/rlogiacco/CircularBuffer) (`UDPStream`).
Services whose dependency is missing are skipped, so the rest of the library
builds without them.

## Usage

### Tasks

```cpp
class Controller final : public frt::Task<Controller, 2048>
{
protected:
    void init() override { m_lastWake = xTaskGetTickCount(); } // runs once, in the task

    bool run() override               // called in a loop until it returns false
    {
        regulate();
        msleepUntil(m_lastWake, 10);   // exactly every 10 ms, no drift
        return !shouldStop();
    }

private:
    TickType_t m_lastWake = 0;
};

Controller controller;
controller.start(3, "ctrl");          // priority, name (copied), [ESP32 core]
controller.stop();                    // waits until run() returned
controller.start(3, "ctrl");          // tasks can be restarted
```

* `run()` and `init()` may be `protected` or `private`.
* `stop()` asks the task to end and waits for it. With
  `INCLUDE_xTaskAbortDelay` enabled, a task blocked in a sleep / queue / wait
  call is woken up so `stop()` does not hang; check return values in `run()`.
  Calling `stop()` from inside the task only requests the stop.
* If you derive from a task class again, call `stop()` in the destructor of the
  most derived class.
* `post()` (also from an ISR) wakes a task waiting in `wait(msecs)`.

### Queues, mutexes, semaphores

```cpp
frt::Queue<Reading, 8> readings;

readings.push(r);                     // blocks while full
if (!readings.push(r, 10)) { ... }    // gives up after 10 ms
if (readings.pop(r, 0)) { ... }       // never blocks
readings.push(r);                     // in an ISR: never blocks

frt::Mutex m;
{
    frt::LockGuard lock(m);           // or std::lock_guard<frt::Mutex>
    ...
}
if (m.lock(5)) { ...; m.unlock(); }   // false on timeout
```

Queue items are copied byte-wise, so `T` must be trivially copyable (checked
at compile time).

### Publish / subscribe

```cpp
struct Temperature { uint32_t timestamp; float value; };

// Producer
frt::Publisher<Temperature> *pub = frt::pubsub::advertise<Temperature>("temperature");
pub->publish({millis(), 21.5f});      // returns the number of receivers, ISR safe

// Consumers: every subscriber has its own queue and gets every message
auto *log  = frt::pubsub::subscribe<Temperature, 10>("temperature");
auto *last = frt::pubsub::subscribe<Temperature, 1>("temperature");  // mailbox: latest value only

Temperature t;
if (log->receive(t, 100)) { ... }

frt::pubsub::unsubscribe(log);
```

* A topic is bound to one message type. `advertise` / `subscribe` with a
  different type returns `nullptr` instead of mixing up data.
* When a subscriber queue is full, its oldest message is dropped.
* Subscribers can be added to a FreeRTOS queue set (`addToSet()`,
  `canReceive()`) to wait on several topics at once, see the QueueSet example.
* Topic names are limited to `FRT_TOPIC_MAX_LEN - 1` (23) characters.

### Timers

```cpp
frt::CallbackTimer blink("blink", pdMS_TO_TICKS(500), true, [] {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
});
blink.start();
```

Timer callbacks run in the FreeRTOS timer task: keep them short and never
block. Destroying a timer waits until the timer task has let go of it.

### Logging

```cpp
FRT_LOG_REGISTER_STREAM(&Serial);
FRT_LOG_LEVEL_DEBUG();
FRT_LOG_INFO("Speed: %d rpm", rpm);  // "00:12:345 INFO  Speed: 3000 rpm"
```

Define `LOG_USE_COLOR` for ANSI colors and the task name. Lines longer than
`MAX_LOG_SIZE` (512) are truncated; logging from an ISR is ignored.

### Critical sections

```cpp
{
    frt::CriticalSection cs;   // task, ISR and before the scheduler started
    shared++;
}
```

`FRT_CRITICAL_ENTER()` / `FRT_CRITICAL_EXIT()` still work but must be properly
nested.

### Examples

* `BasicTasks`: tasks, queue, mutex and a callback timer
* `PubSub`: one publisher, subscribers with different queue sizes
* `QueueSet`: wait on several subscribers at once (needs `configUSE_QUEUE_SETS`)
* `InputService`: debounced button events

### FreeRTOS configuration

Everything works with the default configurations of the supported cores.
These options enable extra behaviour:

| Option | Used for |
| --- | --- |
| `configSUPPORT_STATIC_ALLOCATION` | static allocation of all objects |
| `INCLUDE_xTaskAbortDelay` | `Task::stop()` interrupts blocking calls |
| `configUSE_QUEUE_SETS` | `addToSet()` (queue sets) |
| `configUSE_RECURSIVE_MUTEXES` | `frt::RecursiveMutex` |
| `configUSE_TRACE_FACILITY` + `INCLUDE_xTimerPendFunctionCall` | `EventGroup::setBits()` from an ISR |

## Testing

The wrapper is tested on a PC against the official FreeRTOS POSIX port, with
AddressSanitizer and UndefinedBehaviorSanitizer, in static and dynamic
allocation configurations:

```sh
cmake -S test/host -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CI (`.github/workflows/ci.yml`) runs these tests and builds every example for
ESP32, STM32 and nRF52 with PlatformIO.

## Versioning and releases

The library follows [semantic versioning](https://semver.org): breaking API
changes increase the major version, new features the minor version, fixes the
patch version. Every release is a `vX.Y.Z` tag with a
[GitHub release](https://github.com/smith1401/arduino-freertos-wrapper/releases)
that carries the changelog entry and a ZIP for the Arduino IDE.

The version is stored in `library.json`, `library.properties` and as a
`## X.Y.Z` heading in `CHANGELOG.md`; CI checks that they agree.
`scripts/version.py` keeps them in sync.

To release a new version:

1. Write the changes under `## Unreleased` in `CHANGELOG.md` while working.
2. Set the version (turns `## Unreleased` into `## X.Y.Z - date`) and merge
   the change into the default branch:
   ```sh
   python3 scripts/version.py set X.Y.Z
   ```
3. In GitHub, open *Actions → Release → Run workflow* and enter `X.Y.Z`.
   The workflow checks the version, runs the tests, tags the default branch
   as `vX.Y.Z` and publishes the release. Pushing the tag yourself
   (`git tag vX.Y.Z && git push origin vX.Y.Z`) does the same.

With a `PLATFORMIO_AUTH_TOKEN` repository secret the release is also
published to the PlatformIO registry. Once the library is registered in the
[Arduino Library Manager](https://github.com/arduino/library-registry), new
tags are picked up there automatically.

## Migrating from 1.x

* `Publisher<T, QUEUE_SIZE>` is now `Publisher<T>`. The queue size belongs to
  each subscriber (`subscribe<T, QUEUE_SIZE>`). The second template argument of
  `advertise` is accepted and ignored.
* A timeout of `0` now means "do not block" everywhere (some methods used to
  wait one tick). Use `frt::FOREVER` instead of large values like
  `portMAX_DELAY / configTICK_RATE_HZ`.
* `Mutex::lock(msecs)` returns `bool`.
* `MessageBuffer::available()` returned the *free* space and is deprecated,
  use `availableForWrite()` or `getFillLevel()`. `send` / `receive` take
  `void *`.
* `Manager` keys tasks and topics by object / full name (no hashes). The task
  map accessor is replaced by `getTasks()` (a snapshot vector) and
  `findTask(name)`; `addTask` / `removeTask` take only the task.
* `ITask::name()` returns a copy owned by the task, the name passed to
  `start()` does not need to outlive the call.
* `Timer` subclasses should call `destroy()` in their destructor.
* The burst firing output service is not available on arduino-esp32 3.x
  (its RMT API can not be used from an interrupt anymore).

## Alternatives

Other C++ FreeRTOS wrappers, in case they suit you better:

* [Flössie's frt](https://github.com/Floessie/frt): the minimal AVR wrapper
  this library's task and queue API is derived from.
* [freertos-addons](https://github.com/michaelbecker/freertos-addons): mature
  and feature rich, not Arduino specific.
* [FreeRTOS-Cpp](https://github.com/jonenz/FreeRTOS-Cpp): modern C++17
  header only wrapper, not Arduino specific.
* [WrapperFreeRTOS](https://github.com/alexCajas/WrapperFreeRTOS): Arduino
  wrapper for ESP32 / ESP8266.

This library focuses on Arduino on several platforms with a single API, typed
publish / subscribe between tasks and ISRs, and ready made services.

## License

MIT, see [LICENSE](LICENSE).
