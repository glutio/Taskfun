#include "Arduino.h"
#include "Taskfun.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

bool testInterruptsEnabled = true;
unsigned testDice = 0;
uint32_t testMilliseconds = 0;
static unsigned switchRequests = 0;
static int failAfter = -1;
static int liveAllocations = 0;
static bool watchAllocations = false;
static unsigned unguardedAllocations = 0;

static void* allocate(size_t size) {
  if (watchAllocations && testInterruptsEnabled) ++unguardedAllocations;
  if (failAfter == 0) return 0;
  if (failAfter > 0) --failAfter;
  void* block = malloc(size ? size : 1);
  if (block) {
    memset(block, 0xaf, size ? size : 1);
    ++liveAllocations;
  }
  return block;
}

void* operator new(size_t size) {
  void* block = allocate(size);
  if (!block) throw std::bad_alloc();
  return block;
}
void* operator new[](size_t size) { return ::operator new(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void operator delete(void* block) noexcept {
  if (block) { --liveAllocations; free(block); }
}
void operator delete[](void* block) noexcept { ::operator delete(block); }
void operator delete(void* block, size_t) noexcept { ::operator delete(block); }
void operator delete[](void* block, size_t) noexcept { ::operator delete(block); }
void operator delete(void* block, const std::nothrow_t&) noexcept { ::operator delete(block); }
void operator delete[](void* block, const std::nothrow_t&) noexcept { ::operator delete(block); }

#define CHECK(condition) do { \
  if (!(condition)) { printf("FAIL %s:%d: %s\n", __func__, __LINE__, #condition); return false; } \
} while (0)

static void idle(int) {}

namespace Buratino {
unsigned BTaskSwitcher::context_size() { return 64; }
bool BTaskSwitcher::disable() {
  bool enabled = testInterruptsEnabled;
  noInterrupts();
  return enabled;
}
void BTaskSwitcher::init_arch() {}
void BTaskSwitcher::init_task(BTaskInfoBase*, BTaskWrapper) {}
void BTaskSwitcher::switch_context() { ++switchRequests; }

class NativeTests : public BTaskSwitcher {
public:
  typedef BTaskInfoBase Task;
  static void reset() {
    failAfter = -1;
    noInterrupts();
    _current_task = _next_task = 0;
    reap_task();
    for (unsigned i = 1; i < _tasks.Length(); ++i) {
      if (_tasks[i]) free_task(i);
    }
    if (_tasks.Length()) delete _tasks[0];
    while (_tasks.Length()) _tasks.Remove(_tasks.Length() - 1);
    _initialized = false;
    _yielded_task = -1;
    _current_slice = 0;
    for (unsigned i = 0; i < 3; ++i) {
      _pri[i].count = 0;
      _pri[i].current = 0;
    }
    switchRequests = 0;
    testDice = 0;
    testMilliseconds = 0;
    watchAllocations = false;
    unguardedAllocations = 0;
    interrupts();
  }
  static void setup(int capacity = 2, int slice = 1) {
    reset();
    setupTasks(capacity, slice);
  }
  static int create(unsigned priority = 1) {
    return runTask(idle, 0, 128, static_cast<uint8_t>(priority));
  }
  static Task* task(int id) { return _tasks[id]; }
  static unsigned count(unsigned priority) { return _pri[priority].count; }
  static int next() { return _next_task; }
  static unsigned length() { return _tasks.Length(); }
  static unsigned liveTasks() {
    unsigned count = 0;
    for (unsigned i = 0; i < _tasks.Length(); ++i) {
      if (_tasks[i]) ++count;
    }
    return count;
  }
  static bool initialized() { return _initialized; }
  static void tick() { preempt_task(); }
  static void commit() {
    if (_current_task != _next_task) swap_stack(_tasks[_current_task]->sp);
  }
  static bool invariants() {
    unsigned expected[3] = { 0, 0, 0 };
    for (unsigned i = 0; i < _tasks.Length(); ++i) {
      if (!_tasks[i] || _tasks[i]->id < 0) continue;
      if (_tasks[i]->priority() > 2) return false;
      if (_tasks[i]->runnable()) ++expected[_tasks[i]->priority()];
    }
    for (unsigned i = 0; i < 3; ++i) {
      if (expected[i] != _pri[i].count) return false;
    }
    return true;
  }
  template<typename T, typename U>
  static void invoke(int id) {
    BTaskInfo<T, U>* info = static_cast<BTaskInfo<T, U>*>(_tasks[id]);
    info->delegate(info->arg);
  }
  static void terminateCurrent() {
    Task* current = _tasks[_current_task];
    if (current->runnable()) --_pri[current->priority()].count;
    current->id = -1;
    yield_task();
  }
  static void reap() { reap_task(); }
  static bool sleep(unsigned long milliseconds) { return begin_sleep(milliseconds); }
};
}

using Buratino::NativeTests;

static bool flagsAreInitialized() {
  NativeTests::setup();
  int id = runTask(idle, 0, 128, TaskPriority::Medium);
  bool correct = NativeTests::task(id)->priority() == TaskPriority::Medium
    && !NativeTests::task(id)->paused();
  NativeTests::task(id)->flags = TaskPriority::Medium;
  CHECK(correct);
  return true;
}

static bool settingPriorityPreservesPause() {
  NativeTests::Task task;
  task.flags = 0;
  task.priority(TaskPriority::Medium);
  task.pause();
  task.priority(TaskPriority::High);
  CHECK(task.priority() == TaskPriority::High);
  CHECK(task.paused());
  return true;
}

static bool stoppingPausedTaskKeepsMainRunnable() {
  NativeTests::setup();
  int id = NativeTests::create();
  pauseTask(id);
  stopTask(id);
  CHECK(NativeTests::count(1) == 1);
  CHECK(NativeTests::invariants());
  return true;
}

static bool stoppingPausedPriorityDoesNotUnderflow() {
  NativeTests::setup();
  int id = NativeTests::create(TaskPriority::High);
  pauseTask(id);
  stopTask(id);
  CHECK(NativeTests::count(0) == 0);
  CHECK(NativeTests::invariants());
  return true;
}

static bool yieldingWithFreedCursorKeepsMain() {
  NativeTests::setup();
  pauseTask(0);
  int id = NativeTests::create();
  resumeTask(0);
  stopTask(id);
  yield();
  CHECK(NativeTests::next() == 0);
  CHECK(switchRequests == 0);
  return true;
}

static bool yieldedTaskIsExcludedEvenWithStaleCursor() {
  NativeTests::setup();
  int other = NativeTests::create();
  yield();
  NativeTests::commit();
  CHECK(currentTask() == other);
  NativeTests::create(TaskPriority::High);
  pauseTask(other);
  NativeTests::commit();
  CHECK(currentTask() != other);
  resumeTask(other);
  CHECK(NativeTests::invariants());
  return true;
}

static bool pauseAndResumeAreIdempotent() {
  NativeTests::setup();
  int id = NativeTests::create();
  pauseTask(id);
  pauseTask(id);
  CHECK(NativeTests::count(1) == 1);
  resumeTask(id);
  resumeTask(id);
  CHECK(NativeTests::count(1) == 2);
  stopTask(0);
  pauseTask(-1);
  resumeTask(100);
  stopTask(id);
  stopTask(id);
  CHECK(NativeTests::invariants());
  return true;
}

static bool guardsRestoreInterruptState() {
  NativeTests::setup();
  noInterrupts();
  int id = NativeTests::create();
  CHECK(!testInterruptsEnabled);
  pauseTask(id);
  resumeTask(id);
  stopTask(id);
  CHECK(!testInterruptsEnabled);
  interrupts();
  CHECK(currentTask() == 0);
  CHECK(testInterruptsEnabled);
  return true;
}

static bool pendingTaskRemovalReschedules() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  CHECK(NativeTests::next() == id);
  stopTask(id);
  CHECK(NativeTests::next() == 0);
  NativeTests::commit();
  CHECK(currentTask() == 0);
  return true;
}

static bool pendingTaskPauseReschedules() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  pauseTask(id);
  CHECK(NativeTests::next() == 0);
  CHECK(NativeTests::invariants());
  return true;
}

static bool timeSliceUsesExactTicks() {
  NativeTests::setup(2, 3);
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  CHECK(currentTask() == id);
  switchRequests = 0;
  NativeTests::tick();
  NativeTests::tick();
  CHECK(switchRequests == 0);
  NativeTests::tick();
  CHECK(switchRequests == 1);
  return true;
}

static bool failedAllocationsLeaveNoTaskOrLeak() {
  for (int allocation = 0; allocation < 4; ++allocation) {
    NativeTests::setup(1);
    NativeTests::create();
    int before = liveAllocations;
    failAfter = allocation;
    int id = runTask(idle, 0, 128);
    failAfter = -1;
    CHECK(id == -1);
    CHECK(liveAllocations == before);
    CHECK(NativeTests::length() == 2);
    CHECK(NativeTests::invariants());
    CHECK(runTask(idle, 0, 128) == 2);
  }
  return true;
}

static bool setupFailureCanBeRetried() {
  for (int allocation = 0; allocation < 2; ++allocation) {
    NativeTests::reset();
    int before = liveAllocations;
    failAfter = allocation;
    setupTasks(100);
    failAfter = -1;
    CHECK(!NativeTests::initialized());
    CHECK(NativeTests::length() == 0);
    CHECK(liveAllocations == before);
    setupTasks(100);
    CHECK(NativeTests::initialized());
    CHECK(NativeTests::invariants());
  }
  return true;
}

static bool taskAllocationsAreGuarded() {
  NativeTests::setup(1);
  watchAllocations = true;
  int first = runTask(idle, 0, 128);
  int second = runTask(idle, 0, 128);
  watchAllocations = false;
  CHECK(first > 0 && second > 0);
  CHECK(unguardedAllocations == 0);
  CHECK(testInterruptsEnabled);
  return true;
}

static bool oversizedAndInvalidTasksAreRejected() {
  NativeTests::setup();
  int before = liveAllocations;
  CHECK(runTask(idle, 0, UINT_MAX) == -1);
  CHECK(runTask(idle, 0, 0) == -1);
  CHECK(runTask(idle, 0, 128, 3) == -1);
  CHECK(runTask(static_cast<void (*)(int)>(0), 0, 128) == -1);
  CHECK(liveAllocations == before);
  CHECK(NativeTests::invariants());
  return true;
}

static int received = 0;
static void receiveValue(int value) { received = value; }
static void receiveReference(int& value) { received = ++value; }
struct Receiver {
  void value(int argument) { received = argument + 10; }
  void reference(int& argument) { received = ++argument + 20; }
};

static bool allTaskOverloadsInvokeCopiedArguments() {
  NativeTests::setup(4);
  Receiver instance;
  int original = 7;
  int functionValue = runTask(receiveValue, original, 128);
  int functionRef = runTask(receiveReference, original, 128);
  int methodValue = runTask(&instance, &Receiver::value, original, 128);
  int methodRef = runTask(&instance, &Receiver::reference, original, 128);
  CHECK(functionValue > 0 && functionRef > 0 && methodValue > 0 && methodRef > 0);
  NativeTests::invoke<int, int>(functionValue);
  CHECK(received == 7);
  NativeTests::invoke<int&, int>(functionRef);
  CHECK(received == 8 && original == 7);
  NativeTests::invoke<int, int>(methodValue);
  CHECK(received == 17);
  NativeTests::invoke<int&, int>(methodRef);
  CHECK(received == 28 && original == 7);
  CHECK(runTask(static_cast<Receiver*>(0), &Receiver::value, original, 128) == -1);
  return true;
}

static void receiveNoArgument() { received = 42; }
struct NoArgReceiver {
  void method() { received = 99; }
};

static bool noArgTasksIgnoreDummyArgument() {
  NativeTests::setup(4);
  received = 0;
  NoArgReceiver instance;
  int functionId = runTask(receiveNoArgument, 128);
  int methodId = runTask(&instance, &NoArgReceiver::method, 128);
  CHECK(functionId > 0 && methodId > 0);
  NativeTests::invoke<Buratino::BNoArg, Buratino::BNoArg>(functionId);
  CHECK(received == 42);
  NativeTests::invoke<Buratino::BNoArg, Buratino::BNoArg>(methodId);
  CHECK(received == 99);
  CHECK(runTask(static_cast<NoArgReceiver*>(0), &NoArgReceiver::method, 128) == -1);
  return true;
}

struct Argument {
  static int alive;
  int value;
  explicit Argument(int number) : value(number) { ++alive; }
  Argument(const Argument& other) : value(other.value) { ++alive; }
  ~Argument() { --alive; }
};
int Argument::alive = 0;
static void takeArgument(Argument&) {}

static bool copiedArgumentsAreDestroyedExactlyOnce() {
  NativeTests::setup();
  Argument original(5);
  int id = runTask(takeArgument, original, 128);
  CHECK(Argument::alive == 2);
  stopTask(id);
  CHECK(Argument::alive == 1);
  NativeTests::setup(1);
  NativeTests::create();
  failAfter = 3;
  id = runTask(takeArgument, original, 128);
  failAfter = -1;
  CHECK(id == -1 && Argument::alive == 1);
  return true;
}

struct CopyProbe {
  static const CopyProbe* watched;
  static bool copiedWithoutGuard;
  int value;
  explicit CopyProbe(int number = 0) : value(number) {}
  CopyProbe(const CopyProbe& other) : value(other.value) {
    if (&other == watched && testInterruptsEnabled) copiedWithoutGuard = true;
  }
};
const CopyProbe* CopyProbe::watched = nullptr;
bool CopyProbe::copiedWithoutGuard = false;
struct WatchedSyncVar : SyncVar<CopyProbe> {
  WatchedSyncVar() : SyncVar<CopyProbe>(CopyProbe(7)) {}
  const CopyProbe* address() const { return &_value; }
};

static bool copyingSyncVarProtectsSharedRead() {
  NativeTests::setup();
  WatchedSyncVar source;
  CopyProbe::watched = source.address();
  CopyProbe::copiedWithoutGuard = false;
  SyncVar<CopyProbe> copy(source);
  bool guarded = !CopyProbe::copiedWithoutGuard;
  CopyProbe::watched = nullptr;
  CHECK(guarded);
  CHECK(static_cast<CopyProbe>(copy).value == 7);
  CHECK(testInterruptsEnabled);
  return true;
}

static bool terminatedStackIsReapedOnlyAfterSwitch() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  NativeTests::terminateCurrent();
  NativeTests::commit();
  CHECK(currentTask() == 0);
  CHECK(NativeTests::task(id) != 0);
  NativeTests::reap();
  CHECK(NativeTests::task(id) == 0);
  CHECK(NativeTests::invariants());
  return true;
}

static bool terminalTaskCanReturnToPausedMain() {
  NativeTests::setup();
  int id = NativeTests::create();
  pauseTask(0);
  NativeTests::commit();
  CHECK(currentTask() == id);
  NativeTests::terminateCurrent();
  NativeTests::commit();
  CHECK(currentTask() == 0);
  NativeTests::reap();
  CHECK(NativeTests::task(id) == 0);
  CHECK(NativeTests::count(1) == 0);
  return true;
}

static bool localSyncVarStartsInitializedAndRestoresInterrupts() {
  alignas(SyncVar<unsigned>) unsigned char storage[sizeof(SyncVar<unsigned>)];
  memset(storage, 0xaf, sizeof(storage));
  SyncVar<unsigned>* value = new (storage) SyncVar<unsigned>();
  CHECK(static_cast<unsigned>(*value) == 0);
  noInterrupts();
  CHECK(value->compareAndSet(0, 3));
  CHECK(!value->compareAndSet(0, 4));
  CHECK(!testInterruptsEnabled);
  *value += 2;
  CHECK(static_cast<unsigned>(*value) == 5);
  CHECK(!testInterruptsEnabled);
  interrupts();
  value->~SyncVar<unsigned>();
  return true;
}

static bool sleepExcludesTaskUntilDeadline() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  testMilliseconds = 100;
  CHECK(NativeTests::sleep(20));
  NativeTests::commit();
  CHECK(currentTask() == 0);
  CHECK(NativeTests::task(id)->sleeping());
  CHECK(NativeTests::count(1) == 1);
  testMilliseconds = 119;
  yield();
  CHECK(NativeTests::next() == 0);
  testMilliseconds = 120;
  NativeTests::tick();
  CHECK(NativeTests::next() == id);
  CHECK(!NativeTests::task(id)->sleeping());
  CHECK(NativeTests::count(1) == 2);
  NativeTests::tick();
  CHECK(NativeTests::count(1) == 2);
  CHECK(NativeTests::invariants());
  return true;
}

static bool sleepingPauseResumePreservesCounts() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  CHECK(NativeTests::sleep(10));
  NativeTests::commit();
  pauseTask(id);
  resumeTask(id);
  CHECK(NativeTests::count(1) == 1);
  pauseTask(id);
  testMilliseconds = 10;
  NativeTests::tick();
  CHECK(!NativeTests::task(id)->sleeping());
  CHECK(NativeTests::task(id)->paused());
  CHECK(NativeTests::count(1) == 1);
  resumeTask(id);
  CHECK(NativeTests::count(1) == 2);
  CHECK(NativeTests::invariants());
  return true;
}

static bool stoppingSleeperDoesNotDecrementTwice() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  CHECK(NativeTests::sleep(10));
  NativeTests::commit();
  stopTask(id);
  CHECK(NativeTests::count(1) == 1);
  testMilliseconds = 20;
  yield();
  CHECK(NativeTests::next() == 0);
  CHECK(NativeTests::invariants());
  return true;
}

static bool sleepHandlesClockRollover() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  testMilliseconds = 0xfffffff0u;
  CHECK(NativeTests::sleep(32));
  NativeTests::commit();
  CHECK(NativeTests::task(id)->wakeAt == 16);
  testMilliseconds = 15;
  yield();
  CHECK(NativeTests::task(id)->sleeping());
  testMilliseconds = 16;
  yield();
  CHECK(NativeTests::next() == id);
  CHECK(NativeTests::invariants());
  return true;
}

static bool allSleepingTasksRemainBlocked() {
  NativeTests::setup();
  int id = NativeTests::create();
  CHECK(NativeTests::sleep(20));
  NativeTests::commit();
  CHECK(currentTask() == id);
  CHECK(NativeTests::sleep(10));
  CHECK(NativeTests::next() == id);
  CHECK(NativeTests::count(1) == 0);
  testMilliseconds = 10;
  NativeTests::tick();
  CHECK(NativeTests::next() == id);
  CHECK(!NativeTests::task(id)->sleeping());
  CHECK(NativeTests::task(0)->sleeping());
  testMilliseconds = 20;
  yield();
  CHECK(NativeTests::next() == 0);
  CHECK(NativeTests::invariants());
  return true;
}

static bool invalidSleepDoesNotChangeScheduler() {
  NativeTests::reset();
  CHECK(!sleepTask(10));
  NativeTests::setup();
  noInterrupts();
  CHECK(!sleepTask(10));
  CHECK(!testInterruptsEnabled);
  interrupts();
  CHECK(!sleepTask(0x80000000UL));
  CHECK(sleepTask(0));
  CHECK(NativeTests::count(1) == 1);
  CHECK(NativeTests::invariants());
  return true;
}

static bool stackDiagnosticsTrackLowestHeadroom() {
  NativeTests::setup();
  int id = NativeTests::create();
  TaskStackInfo info = { 0, 0, false };
  CHECK(taskStackInfo(id, info));
  CHECK(info.size == 128 && info.minimumFree == 128 && !info.overflowed);
  NativeTests::task(id)->stackBegin[93] = 0;
  CHECK(taskStackInfo(id, info));
  CHECK(info.minimumFree == 93);
  NativeTests::task(id)->stackBegin[93] = NativeTests::Task::stackFill;
  CHECK(taskStackInfo(id, info));
  CHECK(info.minimumFree == 93);
  NativeTests::task(id)->stackBegin[7] = 0;
  CHECK(taskStackInfo(id, info));
  CHECK(info.minimumFree == 7 && !info.overflowed);
  CHECK(!taskStackInfo(0, info));
  CHECK(!taskStackInfo(-1, info));
  CHECK(info.size == 128 && info.minimumFree == 7 && !info.overflowed);
  stopTask(id);
  CHECK(!taskStackInfo(id, info));
  return true;
}

static bool stackCanaryDamageIsSticky() {
  NativeTests::setup();
  int id = NativeTests::create();
  TaskStackInfo info;
  NativeTests::task(id)->stackBegin[-1] = 0;
  CHECK(taskStackInfo(id, info));
  CHECK(info.overflowed);
  NativeTests::task(id)->stackBegin[-1] = NativeTests::Task::stackFill;
  CHECK(taskStackInfo(id, info));
  CHECK(info.overflowed);
  return true;
}

static bool switchingRecordsStackPointerLowWatermark() {
  NativeTests::setup();
  int id = NativeTests::create();
  yield();
  NativeTests::commit();
  auto task = NativeTests::task(id);
  task->sp = task->stackBegin + 12;
  yield();
  NativeTests::commit();
  TaskStackInfo info;
  CHECK(taskStackInfo(id, info));
  CHECK(info.minimumFree == 12);
  yield();
  NativeTests::commit();
  task->sp = task->stackBegin - 2;
  yield();
  NativeTests::commit();
  CHECK(taskStackInfo(id, info));
  CHECK(info.overflowed && info.minimumFree == 0);
  return true;
}

static bool schedulerStateMachineMaintainsInvariants() {
  NativeTests::setup(16);
  unsigned state = 17;
  for (unsigned step = 0; step < 5000; ++step) {
    state = state * 1664525u + 1013904223u;
    testDice = state >> 8;
    unsigned id = (state >> 16) % NativeTests::length();
    switch (state % 8) {
    case 0:
      if (NativeTests::liveTasks() < 16) NativeTests::create((state >> 24) % 3);
      break;
    case 1:
      if (id != static_cast<unsigned>(currentTask())) stopTask(static_cast<int>(id));
      break;
    case 2: pauseTask(static_cast<int>(id)); break;
    case 3: resumeTask(static_cast<int>(id)); break;
    case 4: yield(); break;
    case 5: NativeTests::tick(); break;
    case 6: NativeTests::sleep((state >> 8) % 30); break;
    case 7: testMilliseconds += (state >> 24) % 10; NativeTests::tick(); break;
    }
    CHECK(NativeTests::invariants());
    CHECK(NativeTests::task(NativeTests::next()) != 0);
    CHECK(NativeTests::task(NativeTests::next())->id >= 0);
    if (NativeTests::next() != currentTask()) {
      CHECK(NativeTests::task(NativeTests::next())->runnable());
    }
    NativeTests::commit();
  }
  return true;
}

struct Test { const char* name; bool (*run)(); };
int main() {
  Test tests[] = {
    { "flags initialization", flagsAreInitialized },
    { "priority preserves pause", settingPriorityPreservesPause },
    { "stop paused task", stoppingPausedTaskKeepsMainRunnable },
    { "paused priority underflow", stoppingPausedPriorityDoesNotUnderflow },
    { "freed scheduler cursor", yieldingWithFreedCursorKeepsMain },
    { "yield/pause eligibility", yieldedTaskIsExcludedEvenWithStaleCursor },
    { "idempotent lifecycle", pauseAndResumeAreIdempotent },
    { "interrupt restoration", guardsRestoreInterruptState },
    { "pending stop", pendingTaskRemovalReschedules },
    { "pending pause", pendingTaskPauseReschedules },
    { "time slice ticks", timeSliceUsesExactTicks },
    { "allocation failure rollback", failedAllocationsLeaveNoTaskOrLeak },
    { "initialization retry", setupFailureCanBeRetried },
    { "allocation critical section", taskAllocationsAreGuarded },
    { "invalid task rejection", oversizedAndInvalidTasksAreRejected },
    { "task overloads and copies", allTaskOverloadsInvokeCopiedArguments },
    { "no-arg task overloads", noArgTasksIgnoreDummyArgument },
    { "argument destruction", copiedArgumentsAreDestroyedExactlyOnce },
    { "SyncVar copy critical section", copyingSyncVarProtectsSharedRead },
    { "deferred stack reclamation", terminatedStackIsReapedOnlyAfterSwitch },
    { "termination with paused main", terminalTaskCanReturnToPausedMain },
    { "local SyncVar", localSyncVarStartsInitializedAndRestoresInterrupts },
    { "sleep deadline eligibility", sleepExcludesTaskUntilDeadline },
    { "sleep pause/resume counts", sleepingPauseResumePreservesCounts },
    { "stop sleeping task", stoppingSleeperDoesNotDecrementTwice },
    { "sleep clock rollover", sleepHandlesClockRollover },
    { "all tasks sleeping", allSleepingTasksRemainBlocked },
    { "invalid sleep rejection", invalidSleepDoesNotChangeScheduler },
    { "stack high-water mark", stackDiagnosticsTrackLowestHeadroom },
    { "sticky stack canary", stackCanaryDamageIsSticky },
    { "saved stack pointer diagnostics", switchingRecordsStackPointerLowWatermark },
    { "scheduler state machine", schedulerStateMachineMaintainsInvariants },
  };
  unsigned failed = 0;
  for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    bool passed = tests[i].run();
    printf("%s: %s\n", passed ? "PASS" : "FAIL", tests[i].name);
    if (!passed) ++failed;
  }
  NativeTests::reset();
  printf("%u tests, %u failures\n", static_cast<unsigned>(sizeof(tests) / sizeof(tests[0])), failed);
  return failed ? 1 : 0;
}
