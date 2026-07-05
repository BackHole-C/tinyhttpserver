#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    size_t get_pool_size() const { return threads_.size(); }
    size_t get_task_count() const;

private:
    void worker_loop();

    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
};

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (stop_) {
            throw std::runtime_error("ThreadPool has been stopped");
        }

        tasks_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return res;
}

#endif // THREAD_POOL_H