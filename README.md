# Taskfun

Preemptive multitasking for hobbyists for Arduino AVR and SAMD21. Just `#include "Taskfun.h"` and use `runTask()` to start a task. Use SyncVar class to add synchronous access to a shared variable. Below is a standard Arduino blink example implemented as two independed tasks and a synchronization variable.

```
#include "Taskfun.h"

// synchronize access to this shared global variable
SyncVar<bool> _on;

// strongly typed argument
void On(int) {
  while (1) {
    if (_on) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      _on = false;
    }
  }
}

void Off(int) {
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
  runTask(On, 0 /* unused argument */);
  runTask(Off, 0);
  interrupts();
}

void loop() {
}
```

## Initialization
Before tasks can be created the library needs to be initialized by calling `setupTasks()` from the `setup()` function of your Arduino sketch.

```
void setupTasks(int numTasks = 3, int msSlice = 1);
```
`numTasks` - number of tasks to initialize the internal task list with. The list will automatically grow (but not shrink) if you add more tasks, but that involves allocating new memory for a bigger list and copying the old list to the new one. Try to avoid this by specifying the expected number of tasks.

`msSlice` - number milliseconds in a time slice. How long to allow a task to run before automatically switching to a different task (if there are any other tasks).

**Known Issue** - on Seeeduino XIAO add `delay(500)` as the first line of the `setup()` function in your sketch before calling `setupTasks()`

## Task
A task is a void function (or a class method) that takes one argument of any type. If your task does not need an argument you still have to declare it but you don't have to use it.
```
// task taking an integer argument
void myTaskFunction(int arg) {
  // do stuff
}

class Program {
public:
  // task taking a pointer to Serial class
  void myTaskMethod(double arg) {
     // do stuff
  }
}
```

## Starting a task
To start a task use `runTask()` function after you initialized the library by calling `setupTasks()`. Both function and method tasks are supported.
```
template<typename T>
int runTask(void (*task)(T arg), T arg, unsigned stackSize = 128 * sizeof(int), uint8_t priority = 1);

template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U arg), unsigned stackSize = 128 * sizeof(int), uint8_t priority = 1);
```
The first declaration is for function tasks - it takes a pointer to a void function taking argument of type T. The second declaration is for method tasks - it takes a class instance and a pointer to the method.

Returns `int` - created task id. Use this with `killTask()` if needed.

`arg` - argument to pass to the task.

`stackSize` - the size of the task's stack in bytes. The actual stack size will be bigger by the size of the task context which depends on the board and is 64 bytes on SAMD21 and 33 bytes on AVR. This parameter is critical, you may encounter either stack overflow if it's too small or main stack corruption if it's too big. If your sketch unexpectedly stops working make sure `stackSize` is appropriate for the amount of memory you have and the code you run in your tasks.

`priority` - in which queue this task will live. There are three queues which share the CPU time. Priority 0 (High) gets 50% of CPU time, priority 1 gets 33% and priority 2 gets 17%. Once the queue is selected the next task from that queue is scheduled to run. The queue is processed in a round-robin fashion. Use priority 0 for tasks that need to run most of the time, use priority 1 for regular tasks and priority 2 for sleepy tasks.

```
void myTaskFunction(int arg) {
  // do stuff
}

class Program {
public:
  void myTaskMethod(double arg) {
     // do stuff
  }
}

Program program; // instance of Program class

void setup() {
  noInterrupts(); // do not switch tasks while setting things up
  setupTasks(); // default parameters
  
  // run function task
  runTask(myTaskFunction, 0 /* unused arg */);
  
  // run method task
  runTask(&program, &Program::myTaskMethod, 0 /* unused arg */);
  
  interrupts(); // enable task switching
}
```
# delay() and yield()
To implement a timer task you can use Arduino's `delay()` function. Here is a simple timer that triggers every second:
```
void timerTask() {
  while(1) {
    delay(1000);
    // do stuff
  }
}

void loop() {
  yield(); // switch tasks
}
```
Internally the implementation of `delay()` calls `yield()` which initiates task switching, so a task that is waiting in a `delay()` is not using the CPU. You can call `yield()` whenever you want to initiate a task switch, typically when a task does something and then waits for the next cycle.

# killTask()
If you want to stop a task use `killTask()` function which takes task id as a parameter.
```
void killTask(int id);
```
A task can stop other tasks or can stop itself. In case a task calls `killTask()` with its own `id` the task will be removed from the list during the next task switch. When a task is stopped using `killTask()` or naturally exits the task function or method the task memory including the stack is freed but the task list does not shrink.
