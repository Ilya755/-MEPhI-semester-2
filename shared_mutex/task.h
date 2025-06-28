#pragma once
#include <thread>
#include <atomic>

class Mutex {
public:
  void Lock() {
      count_.fetch_add(1);
      while (state_.exchange(1) == 1) {
          state_.wait(1);
      }
  }

  void Unlock() {
      state_.store(0);
      if (count_.fetch_sub(1) > 1) {
          state_.notify_one();
      }
  }

private:
  std::atomic<int> state_{0};
  std::atomic<int> count_{0};
};

class SharedMutex {
public:
    void lock() {
        mutex_.Lock();
        while (exclusive_cnt + shared_cnt) {
            mutex_.Unlock();
            std::this_thread::yield();
            mutex_.Lock();
        }
        ++exclusive_cnt;
        mutex_.Unlock();
    }

    void unlock() {
        mutex_.Lock();
        if (exclusive_cnt) {
            --exclusive_cnt;
        }
        mutex_.Unlock();
    }

    void lock_shared() {
        mutex_.Lock();
        while (exclusive_cnt) {
            mutex_.Unlock();
            std::this_thread::yield();
            mutex_.Lock();
        }
        ++shared_cnt;
        mutex_.Unlock();
    }

    void unlock_shared() {
        mutex_.Lock();
        if (shared_cnt) {
            --shared_cnt;
        }
        mutex_.Unlock();
    }

private:
    Mutex mutex_;
    int exclusive_cnt = 0;
    int shared_cnt = 0;
};
