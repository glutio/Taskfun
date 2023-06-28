#ifndef __BLIST_H__
#define __BLIST_H__

namespace Buratino {

template<typename T>
class BList {
protected:
  T* _array;
  unsigned _count;
  unsigned _capacity;

public:
  BList(uint16_t capacity)
    : _array(new T[capacity]), _count(0), _capacity(capacity)  {}

  BList()
    : _array(0), _count(0), _capacity(0) {}

  ~BList() {
    delete[] _array;
  }

  T& operator[](unsigned index) {
    return _array[index];
  }

  void Add(T item) {
    if (_count == _capacity) {
      Resize(_capacity ? _capacity * 2 : 2);
    }

    _array[_count++] = item;
  }

  void Remove(unsigned index) {
    for (auto i = index; i < _count - 1; ++i) {
      _array[i] = _array[i + 1];
    }
    --_count;
  }

  unsigned Length() {
    return _count;
  }

  unsigned Capacity() {
    return _capacity;
  }

  void Resize(unsigned capacity) {
    T* array = new T[capacity];
    for (unsigned i = 0; i < min(capacity, _capacity); ++i) {
      array[i] = _array[i];
    }
    delete[] _array;
    _array = array;
    _capacity = capacity;
  }

  void Compact(T filter) {
    for (auto i = 0; i < _count; ++i) {
      if (_array[i] == filter) {
        for (auto j = i + 1; j < _count; j++) {
          if (_array[j] != filter) {
            _array[i++] = _array[j];
          }
        }
        _count = i;
        break;
      }
    }
  }
};

}
#endif