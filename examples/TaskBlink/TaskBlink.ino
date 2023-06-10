#include <Taskfun.h>

void Led1(int ms){
  while(1) {
    digitalWrite(10, HIGH);
    delay(ms);
    digitalWrite(10, LOW);
    delay(ms);
  }
}

void Led2(int ms){
  while(1) {
    digitalWrite(11, HIGH);
    delay(ms);
    digitalWrite(11, LOW);
    delay(ms);
  }
}

void setup(){
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  setupTasks();
  runTask(Led1, 500);
  runTask(Led2, 1000);
}

void loop() {
  // nothing to do here, yield() manually triggers task switch, without it
  // we would waste CPU in an empty loop() until automatic task switch happens
  yield();
}
