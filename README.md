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
