#ifndef __BTASK_H__
#define __BTASK_H__

#include <Buratino.h>

namespace Buratino {

/*
  BTask - a function/method delegate 
*/
template<typename TArgument>
class BTask {
private:
  struct Callable {
    virtual ~Callable() {}
    virtual void Call(TArgument argument) = 0;
  };

  template<typename TClass>
  struct CallableMethodImpl : public Callable {
    TClass* _instance;
    void (TClass::*_method)(TArgument);

    CallableMethodImpl(TClass* instance, void (TClass::*method)(TArgument))
      : _instance(instance), _method(method) {}

    virtual void Call(TArgument argument) {
      (_instance->*_method)(argument);
    }
  };

  struct CallableFunctionImpl : public Callable {
    void (*_func)(TArgument);

    CallableFunctionImpl(void (*func)(TArgument))
      : _func(func) {}

    virtual void Call(TArgument argument) {
      (_func)(argument);
    }
  };

  /* adapters so a no-arg function/method can be used where an argument is expected;
     the argument passed at the call site is simply ignored */
  template<typename TClass>
  struct CallableNoArgMethodImpl : public Callable {
    TClass* _instance;
    void (TClass::*_method)();

    CallableNoArgMethodImpl(TClass* instance, void (TClass::*method)())
      : _instance(instance), _method(method) {}

    virtual void Call(TArgument) {
      (_instance->*_method)();
    }
  };

  struct CallableNoArgFunctionImpl : public Callable {
    void (*_func)();

    CallableNoArgFunctionImpl(void (*func)())
      : _func(func) {}

    virtual void Call(TArgument) {
      (_func)();
    }
  };

protected:
  BPtr<Callable> _callable;  // smart pointer

public:
  typedef TArgument ArgumentType;

  BTask()
    : _callable(0) {}

  template<typename TClass>
  BTask(TClass* instance, void (TClass::*method)(TArgument argument))
    : _callable(instance && method ? new (std::nothrow) CallableMethodImpl<TClass>(instance, method) : nullptr) {}

  BTask(void (*func)(TArgument argument))
    : _callable(func ? new (std::nothrow) CallableFunctionImpl(func) : nullptr) {}

  template<typename TClass>
  BTask(TClass* instance, void (TClass::*method)())
    : _callable(instance && method ? new (std::nothrow) CallableNoArgMethodImpl<TClass>(instance, method) : nullptr) {}

  BTask(void (*func)())
    : _callable(func ? new (std::nothrow) CallableNoArgFunctionImpl(func) : nullptr) {}

  explicit operator bool() const {
    return static_cast<bool>(_callable);
  }

  void operator()(TArgument argument) {
    if (_callable) {
      _callable->Call(argument);
    }
  }
};

}
#endif