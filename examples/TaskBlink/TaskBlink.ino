#include <Taskfun.h>
#include <Eventfun.h>

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
  yield(); // give up CPU
}