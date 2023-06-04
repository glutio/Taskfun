#include <Taskfun.h>

class Semaphore {
protected:
  unsigned _count;

public:
  Semaphore(unsigned count)
    : _count(count) {}

  void Acquire() {
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

  void Release() {
    noInterrupts();
    ++_count;
    interrupts();
  }
};

class Mutex : public Semaphore {
public:
  Mutex()
    : Semaphore(1) {}
};

// main program
const int _pins[] = { 9, 10, 11 };
const int _numLeds = sizeof(_pins) / sizeof(_pins[0]);
const int _buzzerPin = 3;

SyncVar<bool> _ledInUse[_numLeds] = { 0, 0, 0 };
Semaphore _semaphore(_numLeds);
Mutex _mutex;

const char* _letters[] = { ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." };

template<typename T>
void mutexPrint(T message) {
  _mutex.Acquire();
  Serial.print(message);
  _mutex.Release();
}

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

void processMessage(String message) {
  mutexPrint("Received: ");
  mutexPrint(message);
  
  _semaphore.Acquire();
  
  mutexPrint("Processing: ");
  mutexPrint(message);

  for (auto i = 0; i < _numLeds; ++i) {
    if (!_ledInUse[i]) {
      _ledInUse[i] = true;      
      blinkMessage(message.c_str(), _pins[i]);
      _ledInUse[i] = false;
      break;
    }
  }
  _semaphore.Release();
}

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
  runTask(produceTone, 0, 32);
}

void loop() {  
  if (Serial.available()) {    
    auto message = Serial.readString();
    runTask(processMessage, message, 96);
  }
}