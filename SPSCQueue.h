/*
MIT License

Copyright (c) 2018 Meng Rao <raomeng1@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <atomic>
#include <array>

template<class T, uint32_t CNT>
class SPSCQueue
{
public:

    static_assert(CNT && !(CNT & (CNT - 1)), "CNT must be a power of 2");
    // Constraining CNT to be a power of 2, so as to ensure we can use 
    // (x & (CNT - 1)) to replace (x % CNT), which is a huge performance boost

    T* alloc() {
      if (write_idx - read_idx_cach == CNT) {
        // Use the cached read index first, so as to minimize shared variables loading
        read_idx_cach = ((std::atomic<uint32_t>*)&read_idx)->load(std::memory_order_consume);
        // And if the cached read index indicates the queue is full, update the cached read index
        if (__builtin_expect(write_idx - read_idx_cach == CNT, 0)) { // no enough space
          return nullptr;
        }
      }
      return &data[write_idx & MASK];
    }

    void push() {
      // after filling the elements, increment the write index atomically
      ((std::atomic<uint32_t>*)&write_idx)->store(write_idx + 1, std::memory_order_release);
    }

    template<typename Writer>
    bool tryPush(Writer writer) {
      T* p = alloc();
      if (!p) return false;
      writer(p);
      push();
      return true;
    }

    template<typename Writer>
    void blockPush(Writer writer) {
      while (!tryPush(writer))
        ;
    }

    T* front() {
        if (read_idx == ((std::atomic<uint32_t>*)&write_idx)->load(std::memory_order_acquire)) {
          // The queue is empty. Notice that the cached read index is only used in writer thread
          return nullptr;
        }
        return &data[read_idx & MASK];
    }

    void pop() {
      // increment the read index atomically
      ((std::atomic<uint32_t>*)&read_idx)->store(read_idx + 1, std::memory_order_release);
    }

    template<typename Reader>
    bool tryPop(Reader reader) {
      T* v = front();
      if (!v) return false;
      reader(v);
      pop();
      return true;
    }

  private:
    alignas(128) std::array<T, CNT> data;

    alignas(128) uint32_t write_idx = 0;
    uint32_t read_idx_cach = 0; // used only by writing thread

    alignas(128) uint32_t read_idx = 0;

    constexpr static uint32_t MASK = CNT - 1;
};

