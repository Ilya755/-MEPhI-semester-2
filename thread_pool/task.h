#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <chrono>

/*
 * Требуется написать класс ThreadPool, реализующий пул потоков, которые выполняют задачи из общей очереди.
 * С помощью метода PushTask можно положить новую задачу в очередь
 * С помощью метода Terminate можно завершить работу пула потоков.
 * Если в метод Terminate передать флаг wait = true,
 *  то пул подождет, пока потоки разберут все оставшиеся задачи в очереди, и только после этого завершит работу потоков.
 * Если передать wait = false, то все невыполненные на момент вызова Terminate задачи, которые остались в очереди,
 *  никогда не будут выполнены.
 * После вызова Terminate в поток нельзя добавить новые задачи.
 * Метод IsActive позволяет узнать, работает ли пул потоков. Т.е. можно ли подать ему на выполнение новые задачи.
 * Метод GetQueueSize позволяет узнать, сколько задач на данный момент ожидают своей очереди на выполнение.
 * При создании нового объекта ThreadPool в аргументах конструктора указывается количество потоков в пуле. Эти потоки
 *  сразу создаются конструктором.
 * Задачей может являться любой callable-объект, обернутый в std::function<void()>.
 */

class ThreadPool {
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_get;
    std::condition_variable cv_empty;

    std::atomic<bool> exit = false;
    std::atomic<bool> term = false;

public:
    ThreadPool(size_t threadCount) {
        for (size_t i = 0; i < threadCount; ++i) {
            threads.emplace_back([this](){
                while (true) {
                    std::unique_lock lock(mutex_);
                    while (exit.load() == false && tasks.size() == 0) {
                        cv_get.wait(lock);
                    }
                    if (term.load() == true) {
                        return;
                    }
                    if (exit.load() == true && tasks.size() == 0) {
                        cv_empty.notify_one();
                        return;
                    }
                    auto task = tasks.front();
                    tasks.pop();
                    lock.unlock();
                    task();
                }
            });
        }
    }

    void PushTask(const std::function<void()>& task) {
        std::unique_lock lock(mutex_);
        if (exit.load() == false) {
            tasks.push(task);
            cv_get.notify_one();
        }
        else {
            throw std::runtime_error("Cannot push tasks after termination");
        }
    }

    void Terminate(bool wait) {
        std::unique_lock lock(mutex_);
        exit.store(true);
        if (wait == true) {
            while (tasks.size() != 0) {
                cv_empty.wait(lock);
            }
        }
        else {
            term.store(true);
        }
        cv_get.notify_all();
        lock.unlock();
        for (auto& thread : threads) {
            thread.join();
        }
    }

    bool IsActive() const {
        std::unique_lock lock(mutex_);
        return exit.load() == false;
    }

    size_t QueueSize() const {
        std::unique_lock lock(mutex_);
        return tasks.size();
    }
};