#include <Taskfun.h>

// shared variable to synchronize tasks
SyncVar<bool> _on;

// task to turn the LED on
void On(int) {
  while (1) {
    if (_on) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      _on = false;
    }
  }
}

// task to turn the LED off
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
