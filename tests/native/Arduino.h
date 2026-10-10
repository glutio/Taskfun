#ifndef TASKFUN_TEST_ARDUINO_H
#define TASKFUN_TEST_ARDUINO_H

#include <stddef.h>
#include <stdint.h>
#include <new>

#define __BTASKSWITCHER_ARCH_HEADER__
#define __BTASKSWITCHER_ARCH_CLASS__

extern bool testInterruptsEnabled;
extern unsigned testDice;
extern uint32_t testMilliseconds;

inline void interrupts() { testInterruptsEnabled = true; }
inline void noInterrupts() { testInterruptsEnabled = false; }
inline long random(long maximum) { return testDice % maximum; }
inline unsigned long millis() { return testMilliseconds; }

template<typename T>
T min(T left, T right) { return left < right ? left : right; }

#endif
