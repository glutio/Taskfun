#ifndef __SyncVar_H__
#define __SyncVar_H__

#include "BTaskSwitcher.h"

template<typename T>
class SyncVar {
protected:
  typedef Buratino::BTaskSwitcher::BDisableInterrupts Cli;

protected:
  T _value;

public:
  SyncVar() {}

  SyncVar(const T& value)
    : _value(value) {}

  bool compareAndSet(const T& expected_value, const T& new_value) {
    Cli cli;  // disable interrupts
    if (_value == expected_value) {
        _value = new_value;
        return true;  // success, value was changed
    }
    return false;  // failure, value was not changed
  }
  
  // Overloading += operator
  SyncVar& operator+=(const T& rhs) {
    Cli cli;
    _value += rhs;
    return *this;
  }

  // Overloading -= operator
  SyncVar& operator-=(const T& rhs) {
    Cli cli;
    _value -= rhs;
    return *this;
  }

  // Overloading *= operator
  SyncVar& operator*=(const T& rhs) {
    Cli cli;
    _value *= rhs;
    return *this;
  }

  // Overloading /= operator
  SyncVar& operator/=(const T& rhs) {
    Cli cli;
    _value /= rhs;
    return *this;
  }

  SyncVar& operator=(const SyncVar& rhs) {
    Cli cli;
    if (this != &rhs) {
      _value = rhs._value;
    }
    return *this;
  }

  SyncVar& operator=(const T& rhs) {
    Cli cli;
    _value = rhs;
    return *this;
  }

  // Overloading cast to T
  operator T() const {
    Cli cli;
    T temp = _value;
    return temp;
  }

  // Overloading == operator for T
  bool operator==(const T& rhs) const {
    Cli cli;
    bool result = (_value == rhs);
    return result;
  }

  // Overloading != operator for T
  bool operator!=(const T& rhs) const {
    Cli cli;
    bool result = (_value != rhs);
    return result;
  }

  // Overloading < operator for T
  bool operator<(const T& rhs) const {
    Cli cli;
    bool result = (_value < rhs);
    return result;
  }

  // Overloading > operator for T
  bool operator>(const T& rhs) const {
    Cli cli;
    bool result = (_value > rhs);
    return result;
  }

  // Overloading >= operator for T
  bool operator>=(const T& rhs) const {
    Cli cli;
    bool result = (_value >= rhs);
    return result;
  }

  // Overloading <= operator for T
  bool operator<=(const T& rhs) const {
    Cli cli;
    bool result = (_value <= rhs);
    return result;
  }

  T operator-() const {
    Cli cli;
    T result = -_value;
    return result;
  }

  // Logical negation
  bool operator!() const {
    Cli cli;
    bool result = !_value;
    return result;
  }

  // Increment (prefix)
  SyncVar& operator++() {
    Cli cli;
    ++_value;
    return *this;
  }

  // Decrement (prefix)
  SyncVar& operator--() {
    Cli cli;
    --_value;
    return *this;
  }

  // Increment (postfix)
  SyncVar operator++(int) {
    Cli cli;
    SyncVar temp(*this);
    ++_value;
    return temp;
  }

  // Decrement (postfix)
  SyncVar operator--(int) {
    Cli cli;
    SyncVar temp(*this);
    --_value;
    return temp;
  }

  // Bitwise NOT
  SyncVar operator~() {
    Cli cli;
    SyncVar temp(~_value);
    return temp;
  }

  // Bitwise AND
  SyncVar operator&(const T& rhs) {
    Cli cli;
    SyncVar temp(_value & rhs);
    return temp;
  }

  // Bitwise OR
  SyncVar operator|(const T& rhs) {
    Cli cli;
    SyncVar temp(_value | rhs);
    return temp;
  }

  // Logical AND
  bool operator&&(const T& rhs) {
    Cli cli;
    bool result = _value && rhs;
    return result;
  }

  // Logical OR
  bool operator||(const T& rhs) {
    Cli cli;
    bool result = _value || rhs;
    return result;
  }

  // Bitwise AND assignment
  SyncVar& operator&=(const T& rhs) {
    Cli cli;
    _value &= rhs;
    return *this;
  }

  // Bitwise OR assignment
  SyncVar& operator|=(const T& rhs) {
    Cli cli;
    _value |= rhs;
    return *this;
  }

  // Left shift
  SyncVar operator<<(int shift) {
    Cli cli;
    SyncVar temp(_value << shift);
    return temp;
  }

  // Right shift
  SyncVar operator>>(int shift) {
    Cli cli;
    SyncVar temp(_value >> shift);
    return temp;
  }

  // Left shift assignment
  SyncVar& operator<<=(int shift) {
    Cli cli;
    _value <<= shift;
    return *this;
  }

  // Right shift assignment
  SyncVar& operator>>=(int shift) {
    Cli cli;
    _value >>= shift;
    return *this;
  }

  // Modulus and modulus assignment
  T operator%(const T& rhs) const {
    Cli cli;
    T result = _value % rhs;
    return result;
  }
  SyncVar& operator%=(const T& rhs) {
    Cli cli;
    _value %= rhs;
    return *this;
  }

  // Bitwise XOR and XOR assignment
  T operator^(const T& rhs) const {
    Cli cli;
    T result = _value ^ rhs;
    return result;
  }

  SyncVar& operator^=(const T& rhs) {
    Cli cli;
    _value ^= rhs;
    return *this;
  }

  // // Array subscripting
  // T& operator[](size_t index) {
  //   // Assume _value is an array or supports array-like access
  //   Cli cli;
  //   T& result = _value[index];
  //   BTaskSwitcher::restore(sreg);
  //   return result;
  // }

  SyncVar operator+(const T& rhs) const {
    Cli cli;
    SyncVar temp(_value + rhs);
    return temp;
  }

  SyncVar operator-(const T& rhs) const {
    Cli cli;
    SyncVar temp(_value - rhs);
    return temp;
  }

  SyncVar operator*(const T& rhs) const {
    Cli cli;
    SyncVar temp(_value * rhs);
    return temp;
  }

  SyncVar operator/(const T& rhs) const {
    Cli cli;
    SyncVar temp(_value / rhs);
    return temp;
  }
};

#endif