#ifndef __BTASK_H__
#define __BTASK_H__

#include "BPtr.h"

namespace Buratino {

/*
  BTask - a function/method delegate 
*/
template<typename TArgument>
class BTask {
private:
  struct Callable {
    virtual ~Callable() {}
    virtual void Call(TArgument& argument) = 0;
  };

  template<typename TClass>
  struct CallableMethodImpl : public Callable {
    TClass* _instance;
    void (TClass::*_method)(TArgument&);

    CallableMethodImpl(TClass* instance, void (TClass::*method)(TArgument&))
      : _instance(instance), _method(method) {}

    virtual void Call(TArgument& argument) {
      (_instance->*_method)(argument);
    }
  };

  struct CallableFunctionImpl : public Callable {
    void (*_func)(TArgument&);

    CallableFunctionImpl(void (*func)(TArgument&))
      : _func(func) {}

    virtual void Call(TArgument& argument) {
      (_func)(argument);
    }
  };

protected:
  BPtr<Callable> _callable;  // smart pointer

public:
  typedef TArgument ArgumentType;

  BTask()
    : _callable(0) {}

  template<typename TClass>
  BTask(const TClass* instance, void (TClass::*method)(TArgument& argument))
    : _callable(new CallableMethodImpl<TClass>(instance, method)) {}

  BTask(void (*func)(TArgument& argument))
    : _callable(new CallableFunctionImpl(func)) {}

  void operator()(TArgument& argument) {
    if (_callable) {
      _callable->Call(argument);
    }
  }
};

}
#endif