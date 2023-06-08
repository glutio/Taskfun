#include <Taskfun.h>

#define MELODY_PIN 3

// popcorn melody
const char* _melody[] = {
  "4G4", "4F4", "4G4", "4D4", "8B3", "4D4", "2G3",
  "4G4", "4F4", "4G4", "4D4", "8B3", "4D4", "2G3",
  "4G4", "4A4", "4B4", "8A4", "4B4", "4B4", "8G4", "4A4", "8G4", "4A4", "4A4", "8F4", "4G4", "8F4", "4G4", "4G4", "8E4", "2G4"
};
const int _melodyLength = sizeof(_melody) / sizeof(_melody[0]);

// Frequency table for notes (in Hz)
const int _freq[9][7] = {
  { 27, 31, 16, 18, 21, 22, 25 },                // A0 - G1
  { 55, 62, 33, 37, 41, 44, 49 },                // A1 - G2
  { 110, 123, 65, 73, 82, 87, 98 },              // A2 - G3
  { 220, 247, 131, 147, 165, 175, 196 },         // A3 - G4
  { 440, 494, 261, 294, 330, 349, 392 },         // A4 - G5
  { 880, 988, 523, 587, 659, 698, 784 },         // A5 - G6
  { 1760, 1976, 1047, 1174, 1319, 1397, 1568 },  // A6 - G7
};

// current note 
SyncVar<int> _note(0);

// get note frequency
int getFreq(const char* note) {
  return _freq[(note[2] - '0')][note[1] - 'A'];
}

// get note duration
int getDuration(const char* note) {
  return 800 / (note[0] - '0');
}

// play note
void playNote(char note) {
  while (1) {
    if (_melody[_note][1] == note) {
      tone(MELODY_PIN, getFreq(_melody[_note]));
      delay(getDuration(_melody[_note]));
      noTone(MELODY_PIN);
      delay(20);
      _note = (_note + 1) % _melodyLength;
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MELODY_PIN, OUTPUT);
  
  // initialize multitasking for 7 tasks
  setupTasks('G' - 'A' + 1);

  noInterrupts();
  // start 7 tasks each playing one of the scale notes
  for (auto i = 'A'; i <= 'G'; i++) {
    runTask(playNote, i /* note */, 64 * sizeof(int) /* stack size */);
  }
  interrupts();
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
