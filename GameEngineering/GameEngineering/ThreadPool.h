#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] { workerThread(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    // **�������**
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::move(task));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers; // �̳߳�
    std::queue<std::function<void()>> tasks; // �������
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;

    void workerThread() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this] { return stop || !tasks.empty(); });
                if (stop && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task(); // ִ������
        }
    }
};
