#include "utils/thread_pool.h"

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    if (num_threads == 0) {
        num_threads = 4;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }

    cv_.notify_all();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

size_t ThreadPool::get_task_count() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();
    }
}