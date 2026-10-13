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
    //
    // TODO: Student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    for (int i = 0; i < std::max(1, num_threads); i++) {
        workers_.emplace_back(&TaskSystemParallelThreadPoolSleeping::workerLoop, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: Student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    sync();
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
        work_cv_.wait(lock, [this] { return shutdown_ || !ready_.empty(); });
        if (shutdown_) {
            return;
        }
        TaskID id = ready_.front();
        Launch& launch = launches_.at(id);
        int task_id = launch.next_task++;
        if (launch.next_task == launch.num_total_tasks) {
            ready_.pop_front();
        }
        IRunnable* runnable = launch.runnable;
        int num_total_tasks = launch.num_total_tasks;
        lock.unlock();

        runnable->runTask(task_id, num_total_tasks);

        lock.lock();
        if (++launch.tasks_done == num_total_tasks) {
            launchFinished(id);
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::launchReady(TaskID id) {
    if (launches_.at(id).num_total_tasks == 0) {
        launchFinished(id);
        return;
    }
    ready_.push_back(id);
    work_cv_.notify_all();
}

void TaskSystemParallelThreadPoolSleeping::launchFinished(TaskID id) {
    std::vector<TaskID> dependents;
    dependents.swap(launches_.at(id).dependents);
    launches_.erase(id);
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


    //
    // TODO: Students will modify the implementation of this
    // method in Parts 3 and 5.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    runAsyncWithDeps(runnable, num_total_tasks, std::vector<TaskID>());
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: Students will implement this method in Part 5.
    //

    std::lock_guard<std::mutex> lock(mutex_);
    TaskID id = next_launch_id_++;
    Launch& launch = launches_[id];
    launch.runnable = runnable;
    launch.num_total_tasks = num_total_tasks;
    launch.next_task = 0;
    launch.tasks_done = 0;
    launch.unmet_deps = 0;
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

    //
    // TODO: Students will modify the implementation of this method in Part 5.
    //

    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this] { return launches_.empty(); });
}
