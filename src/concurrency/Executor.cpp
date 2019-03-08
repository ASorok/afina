#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {
  Executor::Executor(int low_watermark, int high_watermark, int max_queue_size, std::chrono::milliseconds idle_time){

      state = Executor::State::kRun;
      _low_watermark = low_watermark;
      _high_watermark = high_watermark;
      _max_queue_size = max_queue_size;
      _idle_time = idle_time;
      std::unique_lock<std::mutex> lock(mutex);
      std::thread tmp;
      for (int i = 0; i < _low_watermark; ++i) {
          tmp = std::thread(perform, this);
          tmp.detach();
      }
  };

  Executor::~Executor(){this->Stop(false);};

  template <typename F, typename... Types> bool Executor::Execute(F &&func, Types... args) {
      // Prepare "task"
      auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

      std::unique_lock<std::mutex> lock(mutex);
      if (state != Executor::State::kRun || (tasks.size() >= _max_queue_size)) {
          return false;
      }

      // Enqueue new task
      tasks.push_back(std::move(exec));
      if ((threads.size() < _high_watermark)) {
          std::thread thr(perform, this);
          threads.push_back(thr);
          thr.detach();
      }
      lock.unlock();
      empty_condition.notify_one();
      return true;
  };

  void perform(Executor *executor) {
      std::unique_lock<std::mutex> lock(executor->mutex);
      while (executor->state == Executor::State::kRun) {
          if (executor->threads.size() > executor->_low_watermark) {
              if ((executor->empty_condition.wait_for(lock, executor->_idle_time, [executor](){
                  return (executor->state == Executor::State::kRun) && (executor->tasks.empty()) ;
              }))) {
                  break;
              }
          } else {
              executor->empty_condition.wait(lock, [executor](){
                  return (executor->state != Executor::State::kRun) || !(executor->tasks.empty()) ;
              });
          }
          if (executor->state != Executor::State::kRun) {
              break;
          }
          std::function<void()> task = (executor->tasks).back();
          executor->tasks.pop_back();
          lock.unlock();
          try {
              (task)();
          } catch (const std::exception &e) {
              //Doesn't work

          }
          lock.lock();

      }
      executor -> threads.pop_back();
      if ((executor->state == Executor::State::kStopping) && (executor->threads.size() == 0)) {
          executor->state = Executor::State::kStopped;
          executor->stop_condition.notify_one();
      }
  }

  void Executor::Stop(bool await) {
        if (state == Executor::State::kStopped) {
            return;
        }
        std::unique_lock<std::mutex> lock(mutex);
        state = Executor::State::kStopping;
        if (threads.size() == 0) {
            state = Executor::State::kStopped;
            return;
        }
        empty_condition.notify_all();
        if (await == true) {
            stop_condition.wait(lock, [this]() { return (this->state == Executor::State::kStopped); });
        }
  }

} //namespace Concurrency
} // namespace Afina
