#ifndef __BPTR_H__
#define __BPTR_H__

#include <new>

namespace Buratino {
/*
  BPtr - a smart pointer implementation
*/
template<typename T>
class BPtr {
private:
  T* ptr;
  int* refCount;

public:
  // Constructor
  explicit BPtr(T* p = nullptr)
    : ptr(p), refCount(p ? new (std::nothrow) int(1) : nullptr) {
    if (ptr && !refCount) {
      delete ptr;
      ptr = nullptr;
    }
  }

  // Copy constructor
  BPtr(const BPtr& other)
    : ptr(other.ptr), refCount(other.refCount) {
    if (refCount) (*refCount)++;
  }

  // Assignment operator
  BPtr& operator=(const BPtr& other) {
    if (this != &other) {
      // Decrease the reference count of the current object
      if (refCount) (*refCount)--;

      // If the reference count becomes zero, delete the object and refCount
      if (refCount && *refCount == 0) {
        delete ptr;
        delete refCount;
      }

      // Assign the new object and increase its reference count
      ptr = other.ptr;
      refCount = other.refCount;
      if (refCount) (*refCount)++;
    }
    return *this;
  }

  // Destructor
  ~BPtr() {
    if (refCount) (*refCount)--;

    if (refCount && *refCount == 0) {
      delete ptr;
      delete refCount;
    }
  }

  // Dereference operator
  T& operator*() const {
    return *ptr;
  }

  // Arrow operator
  T* operator->() const {
    return ptr;
  }

  // Comparison operator
  bool operator ==(const BPtr& other) const {
    return ptr == other.ptr;
  }

  // Conversion operator to bool
  explicit operator bool() const {
    return ptr != nullptr;
  }
};

}
#endif