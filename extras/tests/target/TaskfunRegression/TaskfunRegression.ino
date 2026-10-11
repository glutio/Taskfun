#include <Taskfun.h>
#include <new>

static const unsigned StackBytes = 256;
SyncVar<int> completed(0);
SyncVar<int> received(0);
SyncVar<bool> continuedAfterStop(false);
SyncVar<int> childResult(-2);
SyncVar<bool> sleeperStarted(false);
SyncVar<bool> sleeperFinished(false);
SyncVar<bool> sleeperSucceeded(false);
SyncVar<unsigned long> sleeperElapsed(0);

void require(bool condition, const __FlashStringHelper* name) {
  Serial.print(condition ? F("PASS: ") : F("FAIL: "));
  Serial.println(name);
  if (!condition) {
    while (true) delay(100);
  }
}

void waitFor(int count) {
  unsigned long start = millis();
  while (completed < count && millis() - start < 2000) yield();
  require(completed == count, F("task completed before timeout"));
  yield();
}

void finish(int value) {
  received = value;
  ++completed;
}

void finishReference(int& value) {
  received = ++value;
  ++completed;
}

void stopSelf(int) {
  ++completed;
  stopTask(currentTask());
  continuedAfterStop = true;
}

void createChild(int) {
  childResult = runTask(finish, 11, 128);
  ++completed;
}

void sleepingWorker(int milliseconds) {
  unsigned long start = millis();
  sleeperStarted = true;
  bool slept = sleepTask(static_cast<unsigned long>(milliseconds));
  sleeperElapsed = millis() - start;
  TaskStackInfo info;
  sleeperSucceeded = slept && taskStackInfo(currentTask(), info)
    && info.size == StackBytes && info.minimumFree > 0 && info.minimumFree < StackBytes
    && !info.overflowed;
  sleeperFinished = true;
  ++completed;
}

void waitForSleepingWorker() {
  unsigned long start = millis();
  while (!sleeperStarted && millis() - start < 2000) yield();
  require(sleeperStarted, F("sleeping worker started"));
}

struct Program {
  void value(int argument) { finish(argument + 10); }
  void reference(int& argument) { finish(++argument + 20); }
};

void setup() {
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 2000) {}
  Serial.println(F("TASKFUN TESTS START"));
  require(runTask(finish, 0, StackBytes) == -1, F("task before initialization rejected"));
  setupTasks(3);

  int count = 0;
  for (uint8_t priority = 0; priority < 3; ++priority) {
    int id = runTask(finish, 5, StackBytes, priority);
    require(id > 0, F("task created at each priority"));
    waitFor(++count);
    require(received == 5, F("function value argument"));
  }

  int argument = 7;
  require(runTask(finishReference, argument, StackBytes) > 0, F("reference task created"));
  waitFor(++count);
  require(received == 8 && argument == 7, F("reference receives owned copy"));

  Program program;
  require(runTask(&program, &Program::value, argument, StackBytes) > 0, F("method value task created"));
  waitFor(++count);
  require(received == 17, F("method value argument"));
  require(runTask(&program, &Program::reference, argument, StackBytes) > 0, F("method reference task created"));
  waitFor(++count);
  require(received == 28 && argument == 7, F("method reference receives owned copy"));

  noInterrupts();
  int paused = runTask(finish, 99, StackBytes, TaskPriority::High);
  pauseTask(paused);
  pauseTask(paused);
  stopTask(paused);
  interrupts();
  require(runTask(finish, 6, StackBytes) > 0, F("task after paused stop created"));
  waitFor(++count);
  require(received == 6, F("stopping paused task preserves scheduling"));

  noInterrupts();
  pauseTask(0);
  int cursor = runTask(finish, 99, StackBytes);
  resumeTask(0);
  stopTask(cursor);
  yield();
  interrupts();
  require(currentTask() == 0, F("freed cursor cannot be scheduled"));

  require(runTask(stopSelf, 0, StackBytes) > 0, F("self stopping task created"));
  waitFor(++count);
  require(!continuedAfterStop, F("self stop never returns"));

  require(runTask(createChild, 0, StackBytes) > 0, F("parent task created"));
  unsigned long childStart = millis();
  while (childResult == -2 && millis() - childStart < 2000) yield();
  require(childResult != -2, F("creation from task returns without crashing"));
  if (childResult > 0) {
    waitFor(count += 2);
    require(received == 11, F("child executes when allocation succeeds"));
  } else {
    waitFor(++count);
    require(childResult == -1, F("allocation failure from task is reported"));
  }

  require(runTask(finish, 0, 0) == -1, F("zero stack rejected"));
  require(runTask(finish, 0, static_cast<unsigned>(-1)) == -1, F("overflowing stack rejected"));
  require(runTask(static_cast<void (*)(int)>(0), 0, StackBytes) == -1, F("null function rejected"));
  require(runTask(static_cast<Program*>(0), &Program::value, argument, StackBytes) == -1,
          F("null method instance rejected"));

  SyncVar<int> local;
  require(local == 0 && local.compareAndSet(0, 3), F("local SyncVar initialization and CAS"));

  noInterrupts();
  int sleeper = runTask(sleepingWorker, 30, StackBytes);
  TaskStackInfo info;
  require(sleeper > 0 && taskStackInfo(sleeper, info), F("stack diagnostics available"));
  require(info.size == StackBytes && info.minimumFree == StackBytes && !info.overflowed,
          F("new task has unused stack and intact canary"));
  require(!taskStackInfo(0, info), F("main stack diagnostics unsupported"));
  interrupts();
  waitForSleepingWorker();
  require(sleepTask(5), F("main can sleep while worker sleeps"));
  require(!sleeperFinished, F("worker does not execute before its deadline"));
  waitFor(++count);
  require(sleeperSucceeded && sleeperElapsed >= 30UL, F("sleep deadline and stack high-water mark"));

  sleeperStarted = sleeperFinished = false;
  noInterrupts();
  sleeper = runTask(sleepingWorker, 20, StackBytes);
  interrupts();
  waitForSleepingWorker();
  pauseTask(sleeper);
  require(sleepTask(30), F("sleep works with paused sleeper"));
  require(!sleeperFinished, F("deadline does not resume a paused task"));
  resumeTask(sleeper);
  waitFor(++count);
  require(sleeperSucceeded, F("resumed sleeper completes safely"));

  sleeperStarted = sleeperFinished = false;
  noInterrupts();
  sleeper = runTask(sleepingWorker, 20, StackBytes);
  interrupts();
  waitForSleepingWorker();
  stopTask(sleeper);
  require(sleepTask(30), F("sleep after stopping a sleeper"));
  require(!sleeperFinished, F("stopped sleeper never returns"));
  require(!taskStackInfo(sleeper, info), F("ended task diagnostics rejected"));

  noInterrupts();
  require(!sleepTask(1), F("sleep with interrupts disabled rejected"));
  interrupts();
  require(!sleepTask(0x80000000UL), F("excessive sleep duration rejected"));
  require(sleepTask(0), F("zero sleep yields"));

  noInterrupts();
  require(runTask(finish, 9, StackBytes) > 0, F("last task created"));
  pauseTask(0);
  interrupts();
  waitFor(++count);
  require(received == 9 && currentTask() == 0, F("last task returns to paused main"));
  resumeTask(0);
  Serial.println(F("TASKFUN TESTS PASSED"));
}

void loop() { yield(); }
