#ifndef TOOLS__THREAD_SAFE_QUEUE_HPP
#define TOOLS__THREAD_SAFE_QUEUE_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace tools
{

template <typename T>
class ThreadSafeQueue
{
public:
  explicit ThreadSafeQueue(size_t max_size = 10) : max_size_(max_size) {}

  void push(const T & value)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return queue_.size() < max_size_; });
    queue_.push(value);
    lock.unlock();
    cv_.notify_one();
  }

  void pop(T & value)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty(); });
    value = queue_.front();
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
  }

  bool try_pop_for(T & value, std::chrono::milliseconds timeout)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
      return false;
    }
    value = queue_.front();
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
    return true;
  }

  bool empty() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  size_t size() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

private:
  std::queue<T> queue_;
  size_t max_size_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
};

}  // namespace tools

#endif  // TOOLS__THREAD_SAFE_QUEUE_HPP