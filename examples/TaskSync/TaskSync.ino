#include <Taskfun.h>

// synchronize access to this shared global variable
SyncVar<bool> _on;

// function that turns LED on or off depending on argument
void OnOff(bool isOn) {
  while (1) {
    if (_on == isOn) {
      digitalWrite(LED_BUILTIN, isOn);
      delay(1000);  
      _on = !_on;
    }
  } 
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  // initialize multitasking
  setupTasks();

  // ensure no task switching while initializing
  noInterrupts();
  // add OnOff task as a task responsible for LED On state
  runTask(OnOff, true);
  // add OnOff task as a task responsible for LED Off state
  runTask(OnOff, false);
  // resume multitasking
  interrupts();
}

void loop() {
  yield(); // give up CPU
}