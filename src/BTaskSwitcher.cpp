#include <Arduino.h>
#include <Buratino.h>
#include "BTask.h"
#include "BTaskSwitcher.h"

namespace Buratino {

volatile bool BTaskSwitcher::_initialized = false;
BList<BTaskSwitcher::BTaskInfoBase*> BTaskSwitcher::_tasks;
volatile int BTaskSwitcher::_current_task = 0;
volatile int BTaskSwitcher::_next_task = 0;
volatile int BTaskSwitcher::_yielded_task = -1;
int BTaskSwitcher::_slice = 1;
volatile int BTaskSwitcher::_current_slice = 0;
int BTaskSwitcher::_terminated_task = -1;
BTaskSwitcher::BSwitchState BTaskSwitcher::_pri[3] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };


BTaskSwitcher::BDisableInterrupts::BDisableInterrupts() {
  enabled = BTaskSwitcher::disable();
}

BTaskSwitcher::BDisableInterrupts::~BDisableInterrupts() {
  BTaskSwitcher::restore(enabled);
}

void BTaskSwitcher::restore(bool enable) {
  if (enable) interrupts();
  else noInterrupts();
}

int BTaskSwitcher::current_task_id() {
  BDisableInterrupts cli;
  auto id = _current_task;
  return id;
}

void BTaskSwitcher::free_task(int id) {
  _tasks[id]->~BTaskInfoBase();
  delete[](uint8_t*) _tasks[id];
  _tasks[id] = 0;
}

void BTaskSwitcher::reap_task() {
  if (_terminated_task >= 0 && _terminated_task != _current_task) {
    free_task(_terminated_task);
    _terminated_task = -1;
  }
}

void BTaskSwitcher::kill_task(int id) {
  auto cli = disable();
  reap_task();
  if (id > 0 && id < (int)_tasks.Length() && _tasks[id] && _tasks[id]->id > 0) {
    if (_tasks[id]->runnable()) --_pri[_tasks[id]->priority()].count;

    if (id == _current_task) {
      _tasks[id]->id = -1;
      yield_task();
      restore(cli);
      while (1)
        ;
    }

    free_task(id);
    if (id == _next_task) schedule_task();
  }
  restore(cli);
}

int BTaskSwitcher::get_next_task() { 
  wake_tasks();
  unsigned long weights[] = { 50, 33, 17 };
  const unsigned priCount = sizeof(weights) / sizeof(weights[0]);
  unsigned long total = 0;
  for(unsigned i = 0; i < priCount; ++i) {
    unsigned count = _pri[i].count;
    if (_yielded_task >= 0 && _tasks[_yielded_task]
        && _tasks[_yielded_task]->runnable()
        && _tasks[_yielded_task]->priority() == i) {
      --count;
    }
    weights[i] *= count;
    total += weights[i];
  }

  if (!total) {
    // A terminating task must be able to return to even a paused main loop.
    return _tasks[_current_task]->id < 0 ? 0 : _current_task;
  }
 
  unsigned long dice = random(total);
  unsigned pri = 0;

  unsigned long cumulative = 0;
  while (dice >= cumulative + weights[pri]) {
    cumulative += weights[pri];
    ++pri;
  }

  auto next_task = _pri[pri].current;
  for (unsigned visited = 0; visited < _tasks.Length(); ++visited) {
    ++next_task;
    if (next_task >= (int)_tasks.Length()) {
      next_task = 0;
    }
    if (next_task != _yielded_task && _tasks[next_task] && _tasks[next_task]->id >= 0
        && _tasks[next_task]->priority() == pri && _tasks[next_task]->runnable()) {
      _pri[pri].current = next_task;
      return next_task;
    }
  }

  return _current_task;
}

void BTaskSwitcher::pause_task(int id) {
  BDisableInterrupts cli;
  reap_task();
  if (id >= 0 && id < (int)_tasks.Length() && _tasks[id] && _tasks[id]->id >= 0 && !_tasks[id]->paused()) {
    if (_tasks[id]->runnable()) --_pri[_tasks[id]->priority()].count;
    _tasks[id]->pause();
    if (id == _current_task) {
      yield();
    } else if (id == _next_task) {
      schedule_task();
    }
  }
}

void BTaskSwitcher::resume_task(int id) {
  BDisableInterrupts cli;
  reap_task();
  if (id >= 0 && id < (int)_tasks.Length() && _tasks[id] && _tasks[id]->id >= 0 && _tasks[id]->paused()) {
    _tasks[id]->resume();
    if (_tasks[id]->runnable()) ++_pri[_tasks[id]->priority()].count;
  }
}

void BTaskSwitcher::wake_tasks() {
  uint32_t now = static_cast<uint32_t>(millis());
  for (unsigned i = 0; i < _tasks.Length(); ++i) {
    auto task = _tasks[i];
    if (task && task->id >= 0 && task->sleeping()
        && static_cast<uint32_t>(now - task->wakeAt) < 0x80000000UL) {
      task->flags &= ~BTaskInfoBase::fSleep;
      if (task->runnable()) ++_pri[task->priority()].count;
    }
  }
}

bool BTaskSwitcher::begin_sleep(unsigned long milliseconds) {
  if (!_initialized || milliseconds > 0x7fffffffUL) return false;
  auto task = _tasks[_current_task];
  if (!task->runnable()) return false;
  if (milliseconds) {
    --_pri[task->priority()].count;
    task->wakeAt = static_cast<uint32_t>(millis()) + static_cast<uint32_t>(milliseconds);
    task->flags |= BTaskInfoBase::fSleep;
  }
  yield_task();
  return true;
}

bool BTaskSwitcher::sleep_task(unsigned long milliseconds) {
  auto enabled = disable();
  if ((!enabled && milliseconds) || !begin_sleep(milliseconds)) {
    restore(enabled);
    return false;
  }
  restore(enabled);
  for (;;) {
    BDisableInterrupts cli;
    auto task = _tasks[_current_task];
    if (!task->sleeping() && !task->paused()) return true;
    // If every task is blocked, poll here rather than executing application code.
    yield_task();
  }
}

void BTaskSwitcher::check_stack(BTaskInfoBase* task, uint8_t* sp) {
  if (!task->stackBegin) return;
  uint8_t* guard = task->stackBegin - BTaskInfoBase::stackGuardSize;
  for (unsigned i = 0; i < BTaskInfoBase::stackGuardSize; ++i) {
    if (guard[i] != BTaskInfoBase::stackFill) task->flags |= BTaskInfoBase::fStackOverflow;
  }
  if (sp) {
    uintptr_t address = reinterpret_cast<uintptr_t>(sp);
    uintptr_t begin = reinterpret_cast<uintptr_t>(task->stackBegin);
    // AVR's saved SP points one byte below the last occupied byte.
    if (address + 1 < begin) task->flags |= BTaskInfoBase::fStackOverflow;
    unsigned free = address < begin ? 0
      : address - begin > task->stackSize ? task->stackSize : static_cast<unsigned>(address - begin);
    if (free < task->minimumFree) task->minimumFree = free;
  }
}

bool BTaskSwitcher::stack_info(int id, TaskStackInfo& info) {
  BDisableInterrupts cli;
  reap_task();
  if (id <= 0 || id >= static_cast<int>(_tasks.Length()) || !_tasks[id]
      || _tasks[id]->id < 0) return false;
  auto task = _tasks[id];
  check_stack(task, nullptr);
  unsigned free = 0;
  while (free < task->stackSize && task->stackBegin[free] == BTaskInfoBase::stackFill) ++free;
  if (free < task->minimumFree) task->minimumFree = free;
  info.size = task->stackSize;
  info.minimumFree = task->minimumFree;
  info.overflowed = task->flags & BTaskInfoBase::fStackOverflow;
  return true;
}

uint8_t* BTaskSwitcher::swap_stack(uint8_t* sp) {
  check_stack(_tasks[_current_task], sp);
  if (_tasks[_current_task]->id < 0) {
    _terminated_task = _current_task;
  } else {
    _tasks[_current_task]->sp = sp;
  }

  _current_task = _next_task;
  _yielded_task = -1;
  _current_slice = _slice;

  sp = _tasks[_current_task]->sp;
  return sp;
}

void BTaskSwitcher::schedule_task() {
  _next_task = get_next_task();
  if (_next_task != _current_task) {
    switch_context();
  } else {
    _yielded_task = -1;
    _current_slice = _slice;
  }
}

bool BTaskSwitcher::can_switch() {
  return _initialized && _current_task == _next_task;
}

void BTaskSwitcher::preempt_task() {
  BDisableInterrupts cli;
  reap_task();
  if (_initialized) wake_tasks();
  if (!can_switch()) return;
  if (_current_slice > 0) --_current_slice;
  if (_current_slice <= 0) {
    schedule_task();
  }
}

void BTaskSwitcher::yield_task() {
  BDisableInterrupts cli;
  reap_task();
  if (can_switch()) {
    _yielded_task = _current_task;
    schedule_task();
  }
}

void BTaskSwitcher::initialize(int tasks, int slice, uint8_t loop_pri) {
  BDisableInterrupts cli;
  if (!_initialized && tasks > 0 && tasks < INT_MAX && slice > 0 && loop_pri <= TaskPriority::Low) {
    auto loop = new (std::nothrow) BTaskInfoBase();
    if (!loop) return;
    if (!_tasks.Resize(static_cast<unsigned>(tasks) + 1)) {
      delete loop;
      return;
    }
    _slice = slice;
    _current_slice = slice;

    // add the initial loop() task
    _tasks.Add(loop);  // loop() already has a stack
    _tasks[0]->id = 0;
    _tasks[0]->priority(loop_pri);
    _pri[_tasks[0]->priority()].count = 1;

    init_arch();

    _initialized = true;
  }
}

}

using namespace Buratino;

void stopTask(int id) {
  BTaskSwitcher::kill_task(id);
}

void pauseTask(int id) {
  BTaskSwitcher::pause_task(id);
}

void resumeTask(int id) {
  BTaskSwitcher::resume_task(id);
}

bool sleepTask(unsigned long milliseconds) {
  return BTaskSwitcher::sleep_task(milliseconds);
}

bool taskStackInfo(int id, TaskStackInfo& info) {
  return BTaskSwitcher::stack_info(id, info);
}

int currentTask() {
  return BTaskSwitcher::current_task_id();
}

void setupTasks(int numTasks, int msSlice, uint8_t loopPriority) {
  BTaskSwitcher::initialize(numTasks, msSlice, loopPriority);
}

// used by arduino's delay()
void yield() {
  BTaskSwitcher::yield_task();
}