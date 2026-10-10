TIP: If your sketch crashes and behaves unexpectedly make sure the tasks have appropriate stack size.

# Taskfun

Taskfun is a library designed to introduce preemptive multitasking capabilities to your Arduino AVR and SAMD21 projects. Unlike cooperative multitasking, preemptive multitasking in Taskfun ensures automatic time-sharing between tasks, enhancing CPU utilization by allowing simultaneous execution of multiple tasks. Taskfun gives you the ability to run multiple operations concurrently, providing your Arduino with an extra layer of responsiveness.

Simply `#include <Taskfun.h>`, add `setupTasks()` to the `setup()` function of the sketch and you can use `runTask()` to start a task. For synchronized access to shared variables across tasks use `SyncVar<>` class which overloads all operators and wraps each operation in a critical section (temporarily disables task switching).

Keep application code in your sketch's `.ino` file, not in Taskfun's library source files. If you use a separate `.cpp` file, include `Arduino.h` and declare functions before calling them; Arduino's automatic function declarations apply only to `.ino` files.

The primary advantage of using Taskfun is its ability to handle multiple operations concurrently without the risk of task monopolization. This is particularly useful for larger or more complex projects. Below is a standard Arduino blink example implemented as two independent tasks and a synchronization variable.

```
#include <Taskfun.h>

// synchronize access to this shared global variable
SyncVar<bool> _on;

// strongly typed argument
void On() {
  while (1) {
    if (_on) { // overloaded operator disables interrupts before reading value
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      _on = false; // overloaded operator disables interrupts before assigning value
    }
  }
}

void Off() {
  while (1) {
    if (!_on) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);
      _on = true;
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setupTasks();
  noInterrupts();
  runTask(On);
  runTask(Off);
  interrupts();
}

void loop() {
}
```

## Initialization
Before tasks can be created the library needs to be initialized by calling `setupTasks()` from the `setup()` function of your Arduino sketch.

```
void setupTasks(int numTasks = 3, int msSlice = 1, uint8_t loopPriority = 1);
```
`numTasks` - number of tasks to initialize the internal task list with (+1 for the main `loop()`). The list will automatically grow (but not shrink) if you add more tasks, but that involves allocating new memory for a bigger list and copying the old list to the new one. Try to avoid this by specifying the expected number of tasks.

`msSlice` - number of timer ticks in a time slice. A tick is 1 ms on SAMD21 and approximately 1.024 ms on a 16 MHz AVR using Arduino's default Timer0 configuration.

`loopPriority` - priority of the main `loop()` task, using the same priority values as `runTask()`.

**Known Issue** - on Seeeduino XIAO add `delay(500)` as the first line of the `setup()` function in your sketch before calling `setupTasks()`

## Task
A task is a void function (or a class method) that takes one argument of any type, or no argument at all.
```
// task taking an integer argument by value
void myTaskFunction(int arg) {
  // do stuff
}

// task taking a String argument by reference
void myTaskFunction(String& arg) {
  // do stuff
}

// task that doesn't need an argument
void myTaskFunction() {
  // do stuff
}

// class methods as tasks are also supported
class Program {
public:
  // task taking a double argument
  void myTaskMethod(double arg) {
     // do stuff
  }
};
```
A task may take an argument by value or by reference. When you run a task, a copy of the argument value is saved in the task's private allocation, separately from its usable stack space. If the task takes an argument by reference it will receive a reference to this copy. If a task takes an argument by value then it will receive a copy of that copy. Pass simple types (`int`, `char`, `float`, etc) by value and complex types (`class` or `struct`) by reference. An argument copy is stored even when passed by reference. To share an existing object instead, pass a pointer such as `MyClass*`; the pointer is copied, and the pointed-to object must remain alive while the task uses it.


## Starting a task
To start a task use `runTask()` function after you initialized the library by calling `setupTasks()`. Both function and method tasks are supported.
```
template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);

template<typename T, typename U>
int runTask(T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);

// overloads for tasks that take no argument
int runTask(void (*task)(), unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);

template<typename T>
int runTask(T* instance, void (T::*task)(), unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
```
The first declaration is for function tasks - it takes a pointer to a void function taking argument of type T. The second declaration is for method tasks - it takes a class instance and a pointer to the method. The no-argument overloads are for tasks that don't need any state - no argument is passed in and none needs to be declared.

Returns `int` - created task id, or `-1` if initialization has not succeeded, the task/parameters are invalid, or the library cannot allocate memory. Check the result before using it with `stopTask()`. The main `loop()` task has id 0 and cannot be stopped. Task IDs are reused after cleanup; do not retain an ID after its task ends.

`arg` - argument to pass to the task (either by value or by reference depending on the task's signature)

`stackSize` - usable task stack space in bytes. The allocation additionally holds the task object, copied argument, saved context (64 bytes on SAMD21 or 33 bytes on AVR), an 8-byte stack guard, and 8 bytes reserved for return-address/alignment overhead. The delegate and its reference count use separate allocations. The default stack is 512 bytes on AVR and 1024 bytes on SAMD21. A stack that is too small can corrupt memory; choose sizes appropriate for the board and task code.

`priority` - which queue this task belongs to. High (0), Medium (1), and Low (2) have per-task weights of 50, 33, and 17. Queue selection is random, weighted by its number of eligible tasks times its per-task weight; selection within a queue is round-robin. These are not guaranteed fixed CPU percentages. A yielding task is excluded from that selection when another eligible task exists.

```
void myTaskFunction(int arg) {
  // do stuff
}

void myOtherTaskFunction() {
  // do stuff, no argument needed
}

class Program {
public:
  void myTaskMethod(double arg) {
     // do stuff
  }
};

Program program; // instance of Program class

void setup() {
  noInterrupts(); // do not switch tasks while setting things up
  setupTasks(); // default parameters
  
  // run function task
  runTask(myTaskFunction, 0 /* unused arg */);
  
  // run function task that takes no argument
  runTask(myOtherTaskFunction);
  
  // run method task
  runTask(&program, &Program::myTaskMethod, 0.0 /* unused double arg */);
  
  interrupts(); // enable task switching
}
```
Tasks can take the argument by value or by reference. In both cases a copy of the argument is stored in the task's private allocation. Use a pointer argument to share the original object instead of copying it.
```
// take argument by value
void taskByValue(int i) {
  // ...
}

// take argument by reference
void taskByRef(String& s) {
  // ...
}

// take a pointer type
void taskByPointer(String* ps) {
  // ...
}

String global("hello");

void setup() {
  // initialize multitasking
  setupTasks();
  
  // pass argument by value
  runTask(taskByValue, 100);
  
  // pass argument by reference, a copy is made on the task's stack
  String s = "hello";
  runTask(taskByRef, s);
  
  // pass a pointer
  runTask(taskByPointer, &global);
}
```

## delay() and yield()
To implement a timer task you can use Arduino's `delay()` function. Here is a simple timer that triggers every second:
```
void timerTask(int) {
  while(1) {
    delay(1000);
    // do stuff
  }
}

void loop() {
  yield(); // switch tasks
}
```
Internally `delay()` calls `yield()`, allowing other tasks to run while it polls the timer. This is not a blocking sleep: the waiting task remains eligible and consumes CPU when scheduled. `yield()` requests a task switch but does not pause the task or put the processor to sleep.

## stopTask()
If you want to stop a task use `stopTask()` function which takes task id as a parameter.
```
void stopTask(int id);
```
A task can stop other tasks or itself. Stopping another task frees it immediately. A self-stopping or naturally returning task is marked dead, then its stack is reclaimed from a different task on a subsequent timer tick or library call. This avoids freeing the stack while it is still in use. If no runnable tasks remain, control returns to the main loop even if it was paused. The task list does not shrink.

## currentTask()
Use `currentTask()` to get the id of currenly executing task. Technically the id is the tasks's position in the list of tasks. Main `loop()` task id is 0.

```
int currentTask();
```
## pauseTask() and resumeTask()
You can pause and resume tasks using `pauseTask()` and `resumeTask()`. If you pause the last running task, it will get marked as paused but will continue running until there is another task to switch to. In case you need to temporarily pause a task's activity, using `pauseTask()` is more efficient than letting the task run without performing an action.
```
void pauseTask(int id);
void resumeTask(int id);
```
`id` - id of the task to pause or resume. 

## sleepTask()

```cpp
bool sleepTask(unsigned long milliseconds);
```

Unlike `delay()`, this blocks the calling task and removes it from scheduling until its deadline. It returns `true` after waking, or `false` if initialization has not succeeded, the calling task is paused, a positive delay is requested with interrupts disabled, or the duration exceeds `2147483647` ms. `sleepTask(0)` yields without sleeping.

```cpp
void timerTask(int) {
  while (true) {
    // perform work
    sleepTask(1000);
  }
}
```

Sleep deadlines use `millis()` and handle its 32-bit rollover. Expiration makes a task eligible; it does not guarantee immediate execution. Pause and sleep are independent: expiration does not resume a paused task, and `resumeTask()` does not shorten a sleep. If all tasks are blocked, the processor polls inside the sleep call; this is not a low-power sleep. No interrupt or timer configuration changes are needed.

## Stack diagnostics

```cpp
struct TaskStackInfo {
  unsigned size;
  unsigned minimumFree;
  bool overflowed;
};
bool taskStackInfo(int id, TaskStackInfo& info);
```

Each task stack is filled with a known pattern and has an additional 8-byte guard below its usable space. `minimumFree` records the smallest observed unused space in bytes; lower values mean more stack was used. The saved stack pointer is checked during switching, and `taskStackInfo()` scans the fill pattern when requested. `overflowed` remains true once guard damage or a below-stack saved pointer is observed.

```cpp
TaskStackInfo stack;
if (taskStackInfo(taskId, stack)) {
  Serial.println(stack.minimumFree);
  if (stack.overflowed) {
    // report failure; investigate the task's stack size before continuing
  }
}
```

The function returns `false` for invalid/ended tasks and for the main loop, whose existing stack is not owned by the library. It leaves the output unchanged on failure. Diagnostics are estimates, not memory protection: an overflow may already have corrupted memory, and writes matching the fill pattern can be missed. Queries scan with interrupts disabled, so use them sparingly. Metadata adds one pointer, two unsigned values, and a 32-bit deadline per task, plus the guard allocation; there are no additional heap allocations for these features.

## SyncVar<>
When tasks access a shared variable, synchronize reads and writes. `SyncVar<>` wraps reads, updates, and copies in an interrupt guard that restores the previous interrupt state. Default construction value-initializes the contained value, including local variables. Separate operations are not a single atomic transaction; use `compareAndSet()` for a conditional update. Its overloaded logical operators evaluate both operands, unlike built-in short-circuiting `&&` and `||`.
```
// synchronized counter
SyncVar<int> _counter;

// array of synchronized variables
SyncVar<bool> state[10];

// synchronized linked list node
template<typename T>
struct ListNode {
  SyncVar<T> _value;
  SyncVar<ListNode*> _next;
};
```
You can use `SyncVar<>` in many synchronization scenarios including Compare-and-Set where you want to compare the SyncVar variable with a value and if they are equal set SyncVar variable to another value. This way both the comparison and setting the value are performed as one uninterrupted operation.
```
// if _syncVar is false, set it to true and return true
if (_syncVar.compareAndSet(false, true)) {
  // do stuff
  //...
  _syncVar = false; // set back to false
}
```

## Allocation constraints

For a fixed set of independent activities, create the long-lived tasks once during `setup()` and pass the expected task count to `setupTasks()`. Their allocations remain in place until they finish or are stopped; scheduling, sleeping, and stack queries do not allocate memory.

Library-owned task allocations, reference counts, and cleanup run with interrupts disabled. This does not make arbitrary application calls to `malloc`, `new`, or allocating `String` operations safe under preemption. Synchronize those operations across all tasks, keep argument constructors/destructors non-blocking, and avoid yielding while manipulating the heap.

With avr-libc's default heap configuration, heap growth is limited by the current stack pointer. A task stack lives inside the heap, so an allocation from that task may fail even when RAM remains available above the heap. Reusable free blocks may still satisfy it. `runTask()` reports its own allocation failures as `-1`; it cannot recover failures inside arbitrary user constructors. This library does not automatically change `__malloc_heap_end` or reserve the main stack. Configuring a fixed AVR heap boundary requires a deliberate memory budget before allocations begin.

Timer0 is shared with Arduino timing/PWM on AVR. The library leaves the default Fast PWM mode intact; `OCR0A = 249` changes the compare phase, not the timer's overflow period.

## Tests

Run from the repository root in PowerShell:

```powershell
.\tests\run-native.ps1 -Sanitize
.\tests\run-target.ps1 -Examples
.\tests\run-target.ps1 -Simulate
```

The native suite compiles the actual scheduler and templates with a small Arduino/architecture shim, injected allocation failures, poisoned allocations, and optional MSVC AddressSanitizer. It uses only C++11 features; MSVC's earliest selectable language mode is `/std:c++14`.

Target tests require Arduino CLI with the `arduino:avr` and `arduino:samd` cores. They compile for Uno, Mega, and Zero; `-Examples` also compiles all examples for Uno and Zero. `-Simulate` runs the Uno and Mega firmware in Wokwi, checking serial assertions with a timeout. Configure `WOKWI_CLI_TOKEN` in the process environment, never in source control. SAMD21 runtime behavior still needs a supported simulator or board; a successful cross-build is not a runtime test.

GitHub Actions runs native sanitizer tests and AVR/SAMD compilation on pushes and pull requests. For optional Uno/Mega runtime checks, add a repository secret named `WOKWI_CLI_TOKEN`, then manually run the Tests workflow with `simulate` enabled. No simulator token is needed for the default checks.

## Contact
If you need assistance using the library please open an [issue](https://github.com/glutio/Taskfun/issues) on GitHub.
