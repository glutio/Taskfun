#ifdef ARDUINO_ARCH_AVR

#define __BTASKSWITCHER_ARCH_HEADER__ \
  extern "C" void TIMER0_COMPA_vect();

#define __BTASKSWITCHER_ARCH_CLASS__ \
  friend void ::TIMER0_COMPA_vect();

#endif