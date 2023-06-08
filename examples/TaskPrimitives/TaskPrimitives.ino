#include <Taskfun.h>

// implementation of a sempaphore
class Semaphore {
protected:
  unsigned _count; // resource count

public:
  Semaphore(unsigned count)
    : _count(count) {}

  // decrement resource count or block if resource not available
  void acquire() {
    while (1) {
      noInterrupts();
      if (_count > 0) {
        --_count;
        interrupts();
        break;
      }
      interrupts();
      yield();
    }
  }

  // increment count of available resources
  void release() {
    noInterrupts();
    ++_count;
    interrupts();
  }
};

// mutex is a semaphore for 1 resource count
class Mutex : public Semaphore {
public:
  Mutex()
    : Semaphore(1) {}
};

// main program
//
// Semaphore is used to synchronize access to some number of resources by a larger number of tasks
// In this example you can submit a message via serial input and the message will be signaled on one of the 3 LEDs in Morse code
// Each message is a separate task. There may be more tasks than LEDs so some tasks have to wait. Semaphore is used for that.
// Serial is a global object and for that reason when multiple tasks what to use it they should synchronize access to it, 
// there Serial is a single resource, so we use mutex, which is a semaphore for 1 resource.

const int _pins[] = { 9, 10, 11 }; // LED pins
const int _numLeds = sizeof(_pins) / sizeof(_pins[0]);
const int _buzzerPin = 3; // buzzer pin to listen to Morse code

SyncVar<bool> _ledInUse[_numLeds] = { 0, 0, 0 }; // which led is in use
Semaphore _semaphore(_numLeds); // semaphore for 3 LEDs
Mutex _mutex; // mutex for single Serial object to use for printing

// Morse code table form A to Z
const char* _letters[] = { ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." };

// function to print a message using Serial with a mutex
template<typename T>
void mutexPrint(T message) {
  _mutex.acquire();
  Serial.print(message);
  _mutex.release();
}

// blink a letter in morse code 
void blinkMorse(const char* code, int pin) {
  auto p = code;
  while (*p) {
    digitalWrite(pin, HIGH);
    auto ms = *p == '-' ? 300 : 50;
    delay(ms);
    digitalWrite(pin, LOW);
    delay(100);
    ++p;
  }
}

// blink the message
void blinkMessage(const char* message, int pin) {
  auto p = message;
  while (*p) {
    char c = toupper(*p);
    if (c >= 'A' && c <= 'Z') {
      auto code = _letters[c - 'A'];
      blinkMorse(code, pin);
      delay(500);
    }
    ++p;
  }
}

// task for processing a message
void processMessage(String message) {
  mutexPrint("Received: ");
  mutexPrint(message);
  
  // make sure an LED is available 
  _semaphore.acquire();
  
  mutexPrint("Processing: ");
  mutexPrint(message);

  for (auto i = 0; i < _numLeds; ++i) {
    if (_ledInUse[i].compareAndSet(false, true)) {
      blinkMessage(message.c_str(), _pins[i]);
      _ledInUse[i] = false;
      break;
    }
  }

  // release LED resource
  _semaphore.release();
}

// produce constant tone to mix with the morse code
void produceTone(int) {
  while(1) {
    digitalWrite(_buzzerPin, HIGH);
    delay(1);
    digitalWrite(_buzzerPin, LOW);
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Ready to receive messages...");
  for(auto i = 0; i < _numLeds; ++i) {
    pinMode(_pins[i], OUTPUT);
  }
  pinMode(_buzzerPin, OUTPUT);
  setupTasks(20);
  runTask(produceTone, 0, 64);
}

void loop() {  
  if (Serial.available()) {    
    auto message = Serial.readString();
    runTask(processMessage, message, 96);
  }
}