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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: Students are not expected to implement TaskSystemParallelSpawn in Part 5.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: Students are not expected to implement TaskSystemParallelSpawn in Part 5.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: Students are not expected to implement TaskSystemParallelSpawn in Part 5.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: Students are not expected to implement TaskSystemParallelSpawn in Part 5.
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

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: Students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part 5.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: Students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part 5.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: Students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part 5.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: Students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part 5.
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
    : ITaskSystem(num_threads), next_launch_id_(0), shutdown_(false) {
    for (int i = 0; i < std::max(1, num_threads); i++) {
        workers_.emplace_back(&TaskSystemParallelThreadPoolSleeping::workerLoop, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    sync();  // let outstanding launches finish before stopping the workers
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
        // Sleep until some launch is ready to run, or shutdown.
        work_cv_.wait(lock, [this] { return shutdown_ || !ready_.empty(); });
        if (shutdown_) {
            return;
        }
        TaskID id = ready_.front();
        Launch& launch = launches_.at(id);
        int task_id = launch.next_task++;
        if (launch.next_task == launch.num_total_tasks) {
            ready_.pop_front();  // all tasks handed out; later workers take the next ready launch
        }
        IRunnable* runnable = launch.runnable;
        int num_total_tasks = launch.num_total_tasks;
        lock.unlock();

        runnable->runTask(task_id, num_total_tasks);

        lock.lock();
        // `launch` is still valid: a launch is erased only after its last task finishes.
        if (++launch.tasks_done == num_total_tasks) {
            launchFinished(id);
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::launchReady(TaskID id) {
    if (launches_.at(id).num_total_tasks == 0) {
        launchFinished(id);  // nothing to run
        return;
    }
    ready_.push_back(id);
    work_cv_.notify_all();
}

void TaskSystemParallelThreadPoolSleeping::launchFinished(TaskID id) {
    std::vector<TaskID> dependents;
    dependents.swap(launches_.at(id).dependents);
    launches_.erase(id);
    // Only direct dependents can change state: those whose last unmet
    // dependency was this launch become ready.
    for (TaskID d : dependents) {
        if (--launches_.at(d).unmet_deps == 0) {
            launchReady(d);
        }
    }
    if (launches_.empty()) {
        done_cv_.notify_all();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    runAsyncWithDeps(runnable, num_total_tasks, std::vector<TaskID>());
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    std::lock_guard<std::mutex> lock(mutex_);
    TaskID id = next_launch_id_++;
    Launch& launch = launches_[id];
    launch.runnable = runnable;
    launch.num_total_tasks = num_total_tasks;
    launch.next_task = 0;
    launch.tasks_done = 0;
    launch.unmet_deps = 0;
    // A dependency that is no longer in launches_ has already finished.
    for (TaskID dep : deps) {
        auto it = launches_.find(dep);
        if (it != launches_.end()) {
            it->second.dependents.push_back(id);
            launch.unmet_deps++;
        }
    }
    if (launch.unmet_deps == 0) {
        launchReady(id);
    }
    return id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this] { return launches_.empty(); });
}
