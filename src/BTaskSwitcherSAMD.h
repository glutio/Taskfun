#ifdef ARDUINO_ARCH_SAMD

#define __BTASKSWITCHER_ARCH_HEADER__ \
  extern "C" int sysTickHook();

#define __BTASKSWITCHER_ARCH_CLASS__ \
  friend void ::PendSV_Handler(); \
  friend int ::sysTickHook();

#endif