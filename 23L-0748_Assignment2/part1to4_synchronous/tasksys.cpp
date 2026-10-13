#include "tasksys.h"
#include <algorithm>


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), num_threads_(std::max(1, num_threads)) {
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // Dynamic assignment: each worker repeatedly claims the next unclaimed
    // task id from a shared atomic counter, so every id is handed out exactly
    // once. runnable and num_total_tasks are only read by the workers.
    std::atomic<int> next_task(0);
    auto worker = [&]() {
        for (int id = next_task++; id < num_total_tasks; id = next_task++) {
            runnable->runTask(id, num_total_tasks);
        }
    };

    // Never more threads than num_threads, and none that would have no task.
    int num_workers = std::min(num_threads_, num_total_tasks);
    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; i++) {
        workers.emplace_back(worker);
    }
    // join() waits for every task and makes the tasks' writes visible here.
    for (auto& w : workers) {
        w.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads), runnable_(nullptr), num_total_tasks_(0), next_task_(0),
      has_work_(false), tasks_done_(0), shutdown_(false) {
    // The workers are created once and reused by every run().
    for (int i = 0; i < std::max(1, num_threads); i++) {
        workers_.emplace_back(&TaskSystemParallelThreadPoolSpinning::workerLoop, this);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    shutdown_ = true;
    for (auto& w : workers_) {
        w.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::workerLoop() {
    while (!shutdown_) {
        if (!has_work_) {
            continue;  // spin: nothing to claim yet
        }
        std::unique_lock<std::mutex> lock(mutex_);
        if (next_task_ >= num_total_tasks_) {
            continue;  // another worker claimed the last task first
        }
        int task_id = next_task_++;
        if (next_task_ == num_total_tasks_) {
            has_work_ = false;
        }
        IRunnable* runnable = runnable_;
        int num_total_tasks = num_total_tasks_;
        lock.unlock();

        runnable->runTask(task_id, num_total_tasks);
        tasks_done_++;
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        runnable_ = runnable;
        num_total_tasks_ = num_total_tasks;
        next_task_ = 0;
        tasks_done_ = 0;
        has_work_ = num_total_tasks > 0;
    }
    // Spin until every task has finished (not merely been claimed).
    while (tasks_done_ < num_total_tasks) {
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads), runnable_(nullptr), num_total_tasks_(0), next_task_(0),
      tasks_done_(0), shutdown_(false) {
    for (int i = 0; i < std::max(1, num_threads); i++) {
        workers_.emplace_back(&TaskSystemParallelThreadPoolSleeping::workerLoop, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    work_cv_.notify_all();
    for (auto& w : workers_) {
        w.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::workerLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        // Sleep until the current launch has an unclaimed task, or shutdown.
        work_cv_.wait(lock, [this] { return shutdown_ || next_task_ < num_total_tasks_; });
        if (shutdown_) {
            return;
        }
        int task_id = next_task_++;
        IRunnable* runnable = runnable_;
        int num_total_tasks = num_total_tasks_;
        lock.unlock();

        runnable->runTask(task_id, num_total_tasks);

        lock.lock();
        // Count a task only once it has finished; the last one wakes run().
        if (++tasks_done_ == num_total_tasks_) {
            done_cv_.notify_one();
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    std::unique_lock<std::mutex> lock(mutex_);
    runnable_ = runnable;
    num_total_tasks_ = num_total_tasks;
    next_task_ = 0;
    tasks_done_ = 0;
    work_cv_.notify_all();
    done_cv_.wait(lock, [this] { return tasks_done_ == num_total_tasks_; });
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    // Implemented in Part 5 (see part5_async_deps/).
    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    // Implemented in Part 5 (see part5_async_deps/).
    return;
}
