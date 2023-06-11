#ifndef __BTASKSWITCHER_H__
#define __BTASKSWITCHER_H__

#include "BTask.h"
#include "BList.h"
#include "BTaskSwitcherSAMD.h"
#include "BTaskSwitcherAVR.h"

__BTASKSWITCHER_ARCH_HEADER__

struct TaskPriority {
  static const int High = 0;
  static const int Medium = 1;
  static const int Low = 2;
};

template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T>
int runTask(void (*task)(T arg), T arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U arg), U arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
void stopTask(int id);
int currentTask();
void setupTasks(int numTasks = 3, int msSlice = 1);

extern "C" void yield();

namespace Buratino {

class BTaskSwitcher {
protected:
  /* RAII to disable/restore interrups */
  class BDisableInterrupts {
  public:
    BDisableInterrupts();
    ~BDisableInterrupts();
  protected:
    bool enabled;
  };

  struct BTaskInfoBase {
    uint8_t* sp;
    int id;
    uint8_t priority;
    virtual ~BTaskInfoBase() {}

    static void* operator new(size_t size) {
        return ::operator new(size);
    }

    static void* operator new(size_t, void* where) {
      return where;
    }
  };

  template<typename T, typename U>
  struct BTaskInfo : BTaskInfoBase {
    BTask<T> delegate;
    U arg;
    BTaskInfo(BTask<T>& task, T& argument) : delegate(task), arg(argument) { }
  };

  struct BSwitchState {
    int current;
    unsigned count;
  };

  typedef void (*BTaskWrapper)(BTaskInfoBase*);

protected:
  static volatile bool _initialized;
  static BList<BTaskInfoBase*> _tasks;
  static volatile int _current_task;
  static volatile int _next_task;
  static volatile int _yielded_task;
  static volatile int _current_slice;
  static int _slice;
  static BSwitchState _pri[3];

protected:
  static int current_task_id();
  static void free_task(int id);
  static int get_next_task();
  static unsigned context_size();
  static bool disable();
  static void restore(bool enable);
  static void initialize(int tasks, int slice);
  static void yield_task();
  static void kill_task(int id);
  static void init_arch();
  static void init_task(BTaskInfoBase* taskInfo, BTaskWrapper wrapper);
  static uint8_t* swap_stack(uint8_t* sp);
  static void switch_context();
  static void schedule_task();
  static bool can_switch();
  static void preempt_task();

  template<typename T, typename U>
  static BTaskInfoBase* alloc_task(BTask<T>& task, U& arg, unsigned stackSize) {
    auto size = sizeof(BTaskInfo<T, U>) + stackSize + context_size();
    auto block = new uint8_t[size];
    auto taskInfo = new (block) BTaskInfo<T, U>(task, arg);
    taskInfo->sp = &block[size - 1];
    return taskInfo;
  }

  template<typename T>
  static BTaskInfoBase* alloc_task(BTask<T&>& task, T& arg, unsigned stackSize) {
    return alloc_task<T&, T>(task, arg, stackSize);
  }

  template<typename T>
  static BTaskInfoBase* alloc_task(BTask<T>& task, T& arg, unsigned stackSize) {
    return alloc_task<T, T>(task, arg, stackSize);
  }

  template<typename T, typename U>
  static void task_wrapper(BTaskInfo<T, U>* taskInfo) {
    taskInfo->delegate(taskInfo->arg);
    kill_task(current_task_id());
  }

  template<typename T, typename U>
  static int run_task(BTask<T>& task, U& arg, unsigned stackSize, uint8_t priority) {
    BDisableInterrupts cli;
    if (!_initialized || priority > TaskPriority::Low || !stackSize) {
      return -1;
    }

    unsigned new_task = 0;
    while (new_task < _tasks.Length() && _tasks[new_task]) {
      ++new_task;
    }

    auto taskInfo = alloc_task(task, arg, stackSize);
    if (new_task == _tasks.Length()) {
      _tasks.Add(taskInfo);
    } else {
      _tasks[new_task] = taskInfo;
    }

    taskInfo->id = new_task;
    taskInfo->priority = priority;
    if (!_pri[priority].count) {
      _pri[priority].current = new_task;
    };
    ++_pri[priority].count;

    init_task(taskInfo, (BTaskWrapper)task_wrapper<typename BTask<T>::ArgumentType, U>);
    return new_task;
  }

  template<typename T>
  static int run_task(BTask<T>& task, T& arg, unsigned stackSize, uint8_t priority) {
    return run_task<T, T>(task, arg, stackSize, priority);
  }

  template<typename T>
  static int run_task(BTask<T&>& task, T& arg, unsigned stackSize, uint8_t priority) {    
    return run_task<T&, T>(task, arg, stackSize, priority);
  }

  template<typename T>
  friend int ::runTask(void (*)(T&), T&, unsigned, uint8_t);
  template<typename T, typename U>
  friend int ::runTask(const T*, void (T::*)(U&), U&, unsigned stackSize, uint8_t priority);
  template<typename T>
  friend int ::runTask(void (*)(T), T, unsigned, uint8_t);
  template<typename T, typename U>
  friend int ::runTask(const T*, void (T::*)(U), U, unsigned, uint8_t);
  
  friend void ::stopTask(int);
  friend int ::currentTask();
  friend void ::setupTasks(int, int);
  friend void ::yield();
  template<typename T>
  friend class SyncVar;

  __BTASKSWITCHER_ARCH_CLASS__
};

}

template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize, uint8_t priority) {
  auto btask = Buratino::BTask<T&>(task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize, uint8_t priority) {
  auto btask = Buratino::BTask<U&>(instance, task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T>
int runTask(void (*task)(T arg), T arg, unsigned stackSize, uint8_t priority) {
  auto btask = Buratino::BTask<T>(task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T, typename U>
int runTask(const T* instance, void (T::*task)(U arg), U arg, unsigned stackSize, uint8_t priority) {
  auto btask = Buratino::BTask<T>(instance, task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

#endif