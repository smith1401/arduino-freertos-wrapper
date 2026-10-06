# Changelog

## 2.0.1 - 2026-10-06

### Fixed

* **STM32 FreeRTOS port compiled into ESP32 / nRF52 builds (PlatformIO)**:
  * The wrapper's `library.json` declared `STM32duino FreeRTOS` and BindArg as
    dependencies. Their platform filter is not applied in every setup (not for
    a wrapper in the project's `lib/` folder), so they were installed for
    every platform. These dependencies are removed, see *Changed*.
  * The `deep` dependency scan ignored the `#ifdef`s in `frt.h`. The library
    now uses `deep+`, which still scans all library sources but evaluates
    them. STM32 is also detected by the series macros from the compiler flags
    (`STM32F4xx`, ...), which the scan knows. On nRF52 the core's FreeRTOS
    headers, which have the same names as those of `STM32duino FreeRTOS`, are
    included so the scan can not resolve them to it.
  * On ESP32 it also came in through AsyncTCP, an unused dependency (the
    `UDPStream` uses AsyncUDP from the arduino-esp32 core), which is removed.
    `UDPStream.cpp` includes WiFi / AsyncUDP where the scan can see them (it
    can not evaluate the `__has_include()` guard around the rest). For the
    same reason the input / output services check for BindArg through a
    helper macro, so a BindArg in the project's `lib_deps` is found.
  * PlatformIO matches libraries by their manifest name: a `lib_ignore` for
    this library must say `STM32duino FreeRTOS`, not `STM32FreeRTOS`. Whether
    it is still needed depends on the project: it is if the project itself
    makes `STM32duino FreeRTOS` visible to non-STM32 environments (e.g. in a
    shared `lib_deps` or `lib/` folder).
  * CI builds the examples with the wrapper in `lib_deps` and in `lib/`, and
    on ESP32 / nRF52 with `STM32duino FreeRTOS` visible, and fails if it is
    used there.
* The migration guide in the README was missing breaking changes made during
  1.x: the `Timer` method renames, `msgs::PID` → `msgs::PIDInput`,
  `InputType` becoming an `enum class`, the removed `...FromInterrupt()`
  methods, `Task::beginCriticalSection()`, `Queue::getFillLevel()` and the
  empty `node.h` / `streambuffer.h` headers.

### Changed

* `library.json` no longer declares `STM32duino FreeRTOS` and BindArg. STM32
  projects add `stm32duino/STM32duino FreeRTOS` to their `lib_deps`, and
  STM32 / nRF52 projects that use the input or output services add
  `https://github.com/openlab-vn-ua/BindArg.git`. See *Installation* in the
  README.

### Known issues

* `PIDService` ignores its `calc_pid` argument (documented now, behaviour
  unchanged: the PID still runs on every calculation event).

## 2.0.0

### Fixed

* **Pub/sub type confusion**: subscribers with different queue sizes on one
  topic (as used by the PID and burst firing services) made the manager
  `static_cast` the publisher to the wrong type. Publishers are now typed on
  the message only, and using a topic with a second message type returns
  `nullptr`.
* **Topic / task registry collisions**: topics and tasks were keyed by a hash
  of their name (`std::_Hash_bytes`, a libstdc++ internal). Colliding names
  aliased each other, and all tasks started without a name replaced each
  other. They are now stored by full name / object.
* **ESP32 critical sections**: the spinlock was a `static` variable in a
  header, so every source file had its own lock and critical sections in
  different files did not exclude each other on the second core.
* **Critical sections before the scheduler started (STM32)**: leaving the
  section took the ISR path because interrupts were masked, so interrupts
  stayed disabled. `frt::CriticalSection` now remembers how it was entered.
* **Logger**: messages longer than the buffer overflowed it, and concurrent
  log calls shared the format buffer without a lock. Logging in `setup()`
  before any task existed could crash on STM32. Tick counts are converted to
  milliseconds correctly for tick rates other than 1 kHz.
* **Timer destruction**: deleting a timer freed the object while the timer
  task could still access it (callback context and static timer buffer). The
  destructor now waits until the timer task processed the delete.
* `Queue::push(item, msecs, remainder)` reported success and failure the
  wrong way round.
* `Queue::peek()` called `xQueuePeekFromISR` with a wrong argument count and
  did not compile once used.
* Remainder overloads divided by `portTICK_PERIOD_MS`, which is 0 at tick
  rates above 1 kHz (nRF52 runs at 1024 Hz).
* `Mutex::lock(msecs)` ignored whether the mutex was acquired.
* `MessageBuffer` static storage was one byte smaller than older FreeRTOS
  versions require.
* `EventGroup::setBits()` returned an uninitialized value from an ISR when
  `xEventGroupSetBitsFromISR` is unavailable.
* `Task::stop()` called from the task itself dead locked, and a task blocked
  forever in `wait()` could not be stopped.
* `task::suspendOtherTasks()` / `resumeOtherTasks()` / `deleteOtherTasks()`
  called FreeRTOS with a `NULL` handle for finished tasks, which suspended
  or deleted the *calling* task.
* Starting a running task again re-used its static stack. Task names are now
  copied instead of keeping the caller's pointer.
* `InputService` destructor leaked its timers and left interrupts attached.
* InputService example did not compile (`new` on a function call), QueueSet
  example used `suspendLoop()` which only exists on nRF52.
* The library no longer fails to build on arduino-esp32 3.x or when the
  optional BindArg / CircularBuffer libraries are missing.

### Added

* Host test suite on the FreeRTOS POSIX port with ASan / UBSan, CI workflow
  that also builds all examples for ESP32, STM32 and nRF52.
* `frt::CriticalSection`, `frt::FOREVER`, `frt::RecursiveMutex`,
  `frt::CallbackTimer`, `pubsub::unsubscribe()`.
* `Task`: `msleepUntil()`, `notify()` / `waitNotification()`,
  `shouldStop()`, priority get / set, suspend / resume, ESP32 core pinning,
  restart after the task ended.
* `Queue`: `pushFront()`, `clear()`, `isEmpty()`, `isFull()`.
* `Mutex::try_lock()` (works with `std::lock_guard` / `std::unique_lock`).
* `Semaphore`: max / initial count, `count()`.
* `EventGroup`: `waitAny()`, `waitAll()`.
* `MessageBuffer`: `nextMessageSize()`, `isEmpty()`, `isFull()`, `clear()`.
* `Timer`: `setPeriodMs()`, `period()`, `name()`.
* `Subscriber`: `peek()`, `available()`, `clear()`. `Publisher::publish()`
  returns the number of receivers.
* `Log`: `unregisterStream()`, `setQuiet()`, printf format checking.
* `library.properties` for the Arduino IDE, examples in Arduino sketch layout.
* Release workflow (tag, GitHub release with notes and Arduino IDE ZIP,
  optional PlatformIO registry publishing) and `scripts/version.py`.

### Changed

See [Migrating from 1.x](https://github.com/smith1401/arduino-freertos-wrapper/blob/main/README.md#migrating-from-1x) in the README.
