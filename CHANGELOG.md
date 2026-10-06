# Changelog

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

See [Migrating from 1.x](https://github.com/smith1401/arduino-freertos-wrapper/blob/v2.0.0/README.md#migrating-from-1x) in the README.
