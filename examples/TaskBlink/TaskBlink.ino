#include <Taskfun.h>

// Implement blink as a task
void Blink(int blink_delay) {
  while (1) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(blink_delay);  
    digitalWrite(LED_BUILTIN, HIGH);
    delay(blink_delay);  
  } 
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // initialize multitasking
  setupTasks();

  // start the task
  runTask(Blink, 1000 /* blink_delay */);
}

void loop() {
  // nothing to do here, yield() manually triggers task switch, without it
  // we would waste CPU in an empty loop() until automatic task switch happens
  yield();
}