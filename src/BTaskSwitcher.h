#ifndef __BTASKSWITCHER_H__
#define __BTASKSWITCHER_H__

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <new>
#include "BTask.h"
#include <Buratino.h>
#include "BTaskSwitcherSAMD.h"
#include "BTaskSwitcherAVR.h"

__BTASKSWITCHER_ARCH_HEADER__

template<typename T>
class SyncVar;

struct TaskPriority {
  static const int High = 0;
  static const int Medium = 1;
  static const int Low = 2;
};

template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T, typename U>
int runTask(T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T>
int runTask(void (*task)(T arg), T arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T, typename U>
int runTask(T* instance, void (T::*task)(U arg), U arg, unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
int runTask(void (*task)(), unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
template<typename T>
int runTask(T* instance, void (T::*task)(), unsigned stackSize = 256 * sizeof(int), uint8_t priority = 1);
void stopTask(int id);
int currentTask();
void pauseTask(int id);
void resumeTask(int id);
bool sleepTask(unsigned long milliseconds);

struct TaskStackInfo {
  unsigned size;
  unsigned minimumFree;
  bool overflowed;
};

bool taskStackInfo(int id, TaskStackInfo& info);
void setupTasks(int numTasks = 3, int msSlice = 1, uint8_t loopPriority = 1);

extern "C" void yield();

namespace Buratino {

/* placeholder argument type used internally so a no-arg task can reuse
   the existing typed-argument run_task/alloc_task/task_wrapper machinery */
struct BNoArg {};

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
    enum {
      fPriorityMask = 0x03,
      fPause = 0x08,
      fSleep = 0x10,
      fStackOverflow = 0x20,
      stackGuardSize = 8,
      stackFill = 0xa5,
    };

    uint8_t* sp;
    int id;
    uint8_t flags;
    uint8_t* stackBegin;
    unsigned stackSize;
    unsigned minimumFree;
    uint32_t wakeAt;
    BTaskInfoBase()
      : sp(0), id(-1), flags(0), stackBegin(0), stackSize(0), minimumFree(0), wakeAt(0) {}
    virtual ~BTaskInfoBase() {}

    static void* operator new(size_t size) {
        return ::operator new(size);
    }

    static void* operator new(size_t size, const std::nothrow_t& tag) noexcept {
      return ::operator new(size, tag);
    }

    static void* operator new(size_t, void* where) {
      return where;
    }

    uint8_t priority() {
      return flags & fPriorityMask;
    }

    void priority(uint8_t p) {
      flags = (flags & ~fPriorityMask) | (p & fPriorityMask);
    }

    void pause() {
      flags |= fPause;
    }

    bool paused() {
      return flags & fPause;
    }

    void resume() {
      flags &= ~fPause;
    }

    bool sleeping() {
      return flags & fSleep;
    }

    bool runnable() {
      return id >= 0 && !paused() && !sleeping();
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
  static int _terminated_task;
  static int _slice;
  static BSwitchState _pri[3];

protected:
  static int current_task_id();
  static void free_task(int id);
  static void reap_task();
  static int get_next_task();
  static unsigned context_size();
  static bool disable();
  static void restore(bool enable);
  static void initialize(int tasks, int slice, uint8_t loop_pri);
  static void yield_task();
  static void pause_task(int id);
  static void resume_task(int id);
  static bool begin_sleep(unsigned long milliseconds);
  static bool sleep_task(unsigned long milliseconds);
  static void wake_tasks();
  static void check_stack(BTaskInfoBase* task, uint8_t* sp);
  static bool stack_info(int id, TaskStackInfo& info);
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
    // Reserve return-address/alignment space without consuming the requested stack.
    size_t overhead = sizeof(BTaskInfo<T, U>) + BTaskInfoBase::stackGuardSize + context_size() + 8;
    if (overhead > UINT_MAX || stackSize > UINT_MAX - overhead) return nullptr;
    auto size = overhead + stackSize;
    auto block = new (std::nothrow) uint8_t[size];
    if (!block) return nullptr;
    auto taskInfo = new (block) BTaskInfo<T, U>(task, arg);
    uint8_t* guard = block + sizeof(BTaskInfo<T, U>);
    for (unsigned i = 0; i < BTaskInfoBase::stackGuardSize + stackSize; ++i) {
      guard[i] = BTaskInfoBase::stackFill;
    }
    taskInfo->stackBegin = guard + BTaskInfoBase::stackGuardSize;
    taskInfo->stackSize = stackSize;
    taskInfo->minimumFree = stackSize;
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
    reap_task();
    if (!_initialized || priority > TaskPriority::Low || !stackSize || !task) {
      return -1;
    }

    unsigned new_task = 0;
    while (new_task < _tasks.Length() && _tasks[new_task]) {
      ++new_task;
    }

    if (new_task >= static_cast<unsigned>(INT_MAX)) return -1;
    auto taskInfo = alloc_task(task, arg, stackSize);
    if (!taskInfo) return -1;
    if (new_task == _tasks.Length()) {
      if (!_tasks.Add(taskInfo)) {
        taskInfo->~BTaskInfoBase();
        delete[] reinterpret_cast<uint8_t*>(taskInfo);
        return -1;
      }
    } else {
      _tasks[new_task] = taskInfo;
    }

    taskInfo->id = new_task;
    taskInfo->priority(priority);
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
  friend int ::runTask(T*, void (T::*)(U&), U&, unsigned stackSize, uint8_t priority);
  template<typename T>
  friend int ::runTask(void (*)(T), T, unsigned, uint8_t);
  template<typename T, typename U>
  friend int ::runTask(T*, void (T::*)(U), U, unsigned, uint8_t);
  friend int ::runTask(void (*)(), unsigned, uint8_t);
  template<typename T>
  friend int ::runTask(T*, void (T::*)(), unsigned, uint8_t);
  
  friend void ::stopTask(int);
  friend int ::currentTask();
  friend void ::pauseTask(int);
  friend void ::resumeTask(int);
  friend bool ::sleepTask(unsigned long);
  friend bool ::taskStackInfo(int, TaskStackInfo&);
  friend void ::setupTasks(int, int, uint8_t);
  friend void ::yield();
  template<typename T>
  friend class ::SyncVar;

  __BTASKSWITCHER_ARCH_CLASS__
};

}

template<typename T>
int runTask(void (*task)(T& arg), T& arg, unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  auto btask = Buratino::BTask<T&>(task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T, typename U>
int runTask(T* instance, void (T::*task)(U& arg), U& arg, unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  auto btask = Buratino::BTask<U&>(instance, task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T>
int runTask(void (*task)(T arg), T arg, unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  auto btask = Buratino::BTask<T>(task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

template<typename T, typename U>
int runTask(T* instance, void (T::*task)(U arg), U arg, unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  auto btask = Buratino::BTask<U>(instance, task);
  return Buratino::BTaskSwitcher::run_task(btask, arg, stackSize, priority);
}

inline int runTask(void (*task)(), unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  static Buratino::BNoArg dummy;
  auto btask = Buratino::BTask<Buratino::BNoArg>(task);
  return Buratino::BTaskSwitcher::run_task(btask, dummy, stackSize, priority);
}

template<typename T>
int runTask(T* instance, void (T::*task)(), unsigned stackSize, uint8_t priority) {
  Buratino::BTaskSwitcher::BDisableInterrupts cli;
  static Buratino::BNoArg dummy;
  auto btask = Buratino::BTask<Buratino::BNoArg>(instance, task);
  return Buratino::BTaskSwitcher::run_task(btask, dummy, stackSize, priority);
}

#endif