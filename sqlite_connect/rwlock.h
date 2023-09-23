// https://zh.cppreference.com/w/cpp/thread/shared_mutex
/*
  #include <iostream>
  #include <thread>
  #include "rwlock.h"

  // Read Write Lock
  namespace global {
    unsigned int count = 0;
    rwlock::Lock lock;
  }

  // read thread
  void read_thread() {
    for (int i = 0; i < 10; ++i) {
      rwlock::LockRead _(global::lock);
      std::cout << std::this_thread::get_id() << ' ' << global::count << std::endl;
    }
  }

  // write thread
  void write_thread() {
    for (int i = 0; i < 10; ++i) {
      rwlock::LockWrite _(global::lock);
      ++global::count;
      std::cout << std::this_thread::get_id() << ' ' << global::count << std::endl;
    }
  }

  int main() {
    std::thread thread1(read_thread);
    std::thread thread2(write_thread);

    thread1.join();
    thread2.join();
  }
*/
#ifndef __ATHOME_RWLOCK_H_
#define __ATHOME_RWLOCK_H_

#include <mutex>
#include <shared_mutex>

namespace rwlock {
    typedef std::shared_mutex Lock;
    typedef std::shared_lock<Lock> LockRead;
    typedef std::unique_lock<Lock> LockWrite;
}

#endif //__ATHOME_RWLOCK_H_