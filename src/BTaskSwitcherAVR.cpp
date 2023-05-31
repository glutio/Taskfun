#ifdef ARDUINO_ARCH_AVR
#include <avr/interrupt.h>
#include <Arduino.h>
#include "BTaskSwitcher.h"

namespace Buratino {

struct Ctx {
  uint8_t r31;
  uint8_t r30;
  uint8_t r29;
  uint8_t r28;
  uint8_t r27;
  uint8_t r26;
  uint8_t r25;
  uint8_t r24;
  uint8_t r23;
  uint8_t r22;
  uint8_t r21;
  uint8_t r20;
  uint8_t r19;
  uint8_t r18;
  uint8_t r17;
  uint8_t r16;
  uint8_t r15;
  uint8_t r14;
  uint8_t r13;
  uint8_t r12;
  uint8_t r11;
  uint8_t r10;
  uint8_t r9;
  uint8_t r8;
  uint8_t r7;
  uint8_t r6;
  uint8_t r5;
  uint8_t r4;
  uint8_t r3;
  uint8_t r2;
  uint8_t r1;
  uint8_t sreg;
  uint8_t r0;
};

unsigned BTaskSwitcher::context_size() {
  return sizeof(Ctx);
}

bool BTaskSwitcher::disable() {
  auto sreg = SREG;
  noInterrupts();
  return sreg & _BV(SREG_I);
}

void __attribute__((naked)) BTaskSwitcher::switch_context() {
  asm volatile("push r0");
  asm volatile("in r0, __SREG__");
  asm volatile("push r0");
  asm volatile("push r1");
  asm volatile("push r2");
  asm volatile("push r3");
  asm volatile("push r4");
  asm volatile("push r5");
  asm volatile("push r6");
  asm volatile("push r7");
  asm volatile("push r8");
  asm volatile("push r9");
  asm volatile("push r10");
  asm volatile("push r11");
  asm volatile("push r12");
  asm volatile("push r13");
  asm volatile("push r14");
  asm volatile("push r15");
  asm volatile("push r16");
  asm volatile("push r17");
  asm volatile("push r18");
  asm volatile("push r19");
  asm volatile("push r20");
  asm volatile("push r21");
  asm volatile("push r22");
  asm volatile("push r23");
  asm volatile("push r24");
  asm volatile("push r25");
  asm volatile("push r26");
  asm volatile("push r27");
  asm volatile("push r28");
  asm volatile("push r29");
  asm volatile("push r30");
  asm volatile("push r31");
  asm volatile("in r24, __SP_L__");
  asm volatile("in r25, __SP_H__");
  asm volatile("call %x0"
               :
               : "i"(swap_stack));
  asm volatile("out __SP_L__, r24");
  asm volatile("out __SP_H__, r25");
  asm volatile("pop r31");
  asm volatile("pop r30");
  asm volatile("pop r29");
  asm volatile("pop r28");
  asm volatile("pop r27");
  asm volatile("pop r26");
  asm volatile("pop r25");
  asm volatile("pop r24");
  asm volatile("pop r23");
  asm volatile("pop r22");
  asm volatile("pop r21");
  asm volatile("pop r20");
  asm volatile("pop r19");
  asm volatile("pop r18");
  asm volatile("pop r17");
  asm volatile("pop r16");
  asm volatile("pop r15");
  asm volatile("pop r14");
  asm volatile("pop r13");
  asm volatile("pop r12");
  asm volatile("pop r11");
  asm volatile("pop r10");
  asm volatile("pop r9");
  asm volatile("pop r8");
  asm volatile("pop r7");
  asm volatile("pop r6");
  asm volatile("pop r5");
  asm volatile("pop r4");
  asm volatile("pop r3");
  asm volatile("pop r2");
  asm volatile("pop r1");
  asm volatile("pop r0");
  asm volatile("out __SREG__, r0");
  asm volatile("pop r0");
  asm volatile("ret");
}

void BTaskSwitcher::init_task(BTaskInfoBase* taskInfo, BTaskWrapper wrapper) {
  // push task_wrapper address for `ret` to pop
  *taskInfo->sp-- = lowByte((uintptr_t)wrapper);
  *taskInfo->sp-- = highByte((uintptr_t)wrapper);
#if defined(__AVR_ATmega2560__)
  *taskInfo->sp-- = 0;  // for devices with more than 128kb program memory
#endif

  // clear registers
  for (unsigned i = 0; i < context_size(); ++i) {
    *taskInfo->sp-- = 0;
  }

  Ctx* ctx = (Ctx*)(taskInfo->sp + 1);
  ctx->sreg = _BV(SREG_I);  // enable interrupts
  // compiler/architecture specific, passing argument via registers
  ctx->r24 = lowByte((uintptr_t)taskInfo);   // r24
  ctx->r25 = highByte((uintptr_t)taskInfo);  // r25
}

void BTaskSwitcher::init_arch() {
  BDisableInterrupts cli;

  // Clear the Timer on Compare Match (CTC) mode (setting the WGM01 bit).
  TCCR0A |= (1 << WGM01);

  // Set the Output Compare Register A value for a 1 ms interrupt rate.
  OCR0A = 249;

  // Enable the Timer0 Compare Match A interrupt.
  TIMSK0 |= (1 << OCIE0A);

  // The prescaler is already set by Arduino's initialization code to 64.
  // Hence no need to set it again.
}

}

using namespace Buratino;

ISR(TIMER0_COMPA_vect) {
  if (BTaskSwitcher::can_switch() && BTaskSwitcher::_current_slice <= 0) {
    BTaskSwitcher::schedule_task();
  } else {
    --BTaskSwitcher::_current_slice;
  }
}

#endif