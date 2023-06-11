#include <Arduino.h>
#include "BList.h"
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

void BTaskSwitcher::kill_task(int id) {
  auto cli = disable();
  if (id > 0 && id < (int)_tasks.Length() && _tasks[id] && _tasks[id]->id > 0) {
    --_pri[_tasks[id]->priority()].count;

    if (id == _current_task) {
      _tasks[id]->id = -1;
      yield_task();
      restore(cli);
      while (1)
        ;
    }

    free_task(id);
  }
  restore(cli);
}

int BTaskSwitcher::get_next_task() { 
  unsigned weights[] = { 50, 33, 17 };
  const unsigned priCount = sizeof(weights) / sizeof(weights[0]);
  auto total = 0;
  for(auto i = 0; i < priCount; ++i) {
    if (!_pri[i].count || (_pri[i].count == 1 && _pri[i].current == _yielded_task)) {
      weights[i] = 0;
    }
    else {
      weights[i] *= _pri[i].count;
      total += weights[i];
    }
  }

  if (!total) {
    return _current_task;
  }
 
  unsigned dice = random(total);
  unsigned pri = 0;

  unsigned cumulative = 0;
  while (dice >= cumulative + weights[pri]) {
    cumulative += weights[pri];
    ++pri;
  }

  auto next_task = _pri[pri].current;
  do {
    ++next_task;
    if (next_task >= (int)_tasks.Length()) {
      next_task = 0;
    }
  } while (next_task != _pri[pri].current && (next_task == _yielded_task || !_tasks[next_task] || _tasks[next_task]->id < 0 || _tasks[next_task]->priority() != pri || _tasks[next_task]->paused()));
  _pri[pri].current = next_task;

  return next_task;
}

void BTaskSwitcher::pause_task(int id) {
  BDisableInterrupts cli;
  if (id >=0 && id < _tasks.Length() && _tasks[id] && !_tasks[id]->paused()) {
    --_pri[_tasks[id]->priority()].count;
    _tasks[id]->pause();
    if (id == _current_task) {
      yield();
    }
  }
}

void BTaskSwitcher::resume_task(int id) {
  BDisableInterrupts cli;
  if (id >=0 && id < _tasks.Length() && _tasks[id] && _tasks[id]->paused()) {
    ++_pri[_tasks[id]->priority()].count;
    _tasks[id]->resume();
  }
}

uint8_t* BTaskSwitcher::swap_stack(uint8_t* sp) {
  if (_tasks[_current_task]->id < 0) {
    free_task(_current_task);
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
  }
}

bool BTaskSwitcher::can_switch() {
  return _initialized && _current_task == _next_task;
}

void BTaskSwitcher::preempt_task() {
  BDisableInterrupts cli;
  if (can_switch() && _current_slice <= 0) {
    schedule_task();
  } else {
    --_current_slice;
  }
}

void BTaskSwitcher::yield_task() {
  BDisableInterrupts cli;
  if (can_switch()) {
    _yielded_task = _current_task;
    schedule_task();
  }
}

void BTaskSwitcher::initialize(int tasks, int slice) {
  BDisableInterrupts cli;
  if (!_initialized && tasks > 0 && slice > 0) {
    _slice = slice;
    _tasks.Resize(tasks + 1);  // 1 for main loop()

    // add the initial loop() task
    _tasks.Add(new BTaskInfoBase());  // loop() already has a stack
    _tasks[0]->id = 0;
    _tasks[0]->priority(TaskPriority::Medium);
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

int currentTask() {
  return BTaskSwitcher::current_task_id();
}

void setupTasks(int numTasks, int msSlice) {
  BTaskSwitcher::initialize(numTasks, msSlice);
}

// used by arduino's delay()
void yield() {
  BTaskSwitcher::yield_task();
}