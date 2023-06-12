TIP: If your sketch crashes and behaves unexpectedly make sure the tasks have appropriate stack size.

# Taskfun

Taskfun is a library designed to introduce preemptive multitasking capabilities to your Arduino AVR and SAMD21 projects. Unlike cooperative multitasking, preemptive multitasking in Taskfun ensures automatic time-sharing between tasks, enhancing CPU utilization by allowing simultaneous execution of multiple tasks. Taskfun gives you the ability to run multiple operations concurrently, providing your Arduino with an extra layer of responsiveness.

Simply `#include <Taskfun.h>`, add `setupTasks()` to the `setup()` function of the sketch and you can use `runTask()` to start a task. For synchronized access to shared variables across tasks use `SyncVar<>` class which overloads all operators and wraps each operation in a critical section (temporarily disables task switching).

The primary advantage of using Taskfun is its ability to handle multiple operations concurrently without the risk of task monopolization. This is particularly useful for larger or more complex projects. Below is a standard Arduino blink example implemented as two independent tasks and a synchronization variable.

```
#include <Taskfun.h>

// synchronize access to this shared global variable
SyncVar<bool> _on;

// strongly typed argument
void On(int) {
  while (1) {
    if (_on) { // overloaded operator disables interrupts before reading value
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      _on = false; // overloaded operator disables interrupts before assigning value
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
`numTasks` - number of tasks to initialize the internal task list with (+1 for the main `loop()`). The list will automatically grow (but not shrink) if you add more tasks, but that involves allocating new memory for a bigger list and copying the old list to the new one. Try to avoid this by specifying the expected number of tasks.

`msSlice` - number of milliseconds in a time slice. How long to allow a task to run before automatically switching to a different task (if there are any other tasks).

**Known Issue** - on Seeeduino XIAO add `delay(500)` as the first line of the `setup()` function in your sketch before calling `setupTasks()`

## Task
A task is a void function (or a class method) that takes one argument of any type. If your task does not need an argument you still have to declare it but you don't have to use it.
```
// task taking an integer argument by value
void myTaskFunction(int arg) {
  // do stuff
}

// task taking a String argument by reference
void myTaskFunction(String& arg) {
  // do stuff
}

// class methods as tasks are also supported
class Program {
public:
  // task taking a pointer to Serial class
  void myTaskMethod(double arg) {
     // do stuff
  }
}
```
A task may take an argument by value or by reference. When you run a task, a copy of the argument value is saved on the task's stack. If the task takes an argument by reference it will receive a reference to this copy. If a task takes an argument by value then it will receive a copy of that copy. Pass simple types (`int`, `char`, `float`, etc) by value and complex types (`class` or `struct`) by reference. Remember that a copy of the argument is made on the task's stack even if it is passed by reference. If you want to avoid this use pointer type for argument like `void*` or `MyClass*`.


## Starting a task
To start a task use `runTask()` function after you initialized the library by calling `setupTasks()`. Both function and method tasks are supported.
```
template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize = 128 * sizeof(int), uint8_t priority = 1);

template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize = 128 * sizeof(int), uint8_t priority = 1);
```
The first declaration is for function tasks - it takes a pointer to a void function taking argument of type T. The second declaration is for method tasks - it takes a class instance and a pointer to the method.

Returns `int` - created task id. Use this with `stopTask()` to stop a task. The main `loop()` task has id 0 and cannot be stopped.

`arg` - argument to pass to the task (either by value or by reference depending on the task's signature)

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
Tasks can take the argument by value or by reference. In both cases a copy of the argument is made on the task's stack. If you don't want to make a copy of the argument on the tasks's stack use a pointer type for the argument.
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
Internally the implementation of `delay()` calls `yield()` which initiates task switching, so a task that is waiting in a `delay()` is not using the CPU. You can call `yield()` whenever you want to initiate a task switch, typically when a task does something and then waits for the next cycle.

## stopTask()
If you want to stop a task use `stopTask()` function which takes task id as a parameter.
```
void stopTask(int id);
```
A task can stop other tasks or can stop itself. In case a running task calls `stopTask()` with its own `id` the task will be removed from the list during the next task switch. When a task is stopped by using `stopTask()` or by naturally exiting the task function or method the task memory, including the stack, is freed but the task list does not shrink.

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

## SyncVar<>
When two tasks access a global(shared) variable, access needs to be synchronized, meaning a task cannot be interrupted when modifying or reading the global variable value. To simplify writing code that accesses global variables use `SyncVar<>` class that wraps all operations in `noInterrupts()`/`interrupts()`.
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
}
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

## Contact
If you need assistance using the library please open an [issue](https://github.com/glutio/Taskfun/issues) on GitHub.
