#ifndef RAYBASE_THREAD_POOL_HH
#define RAYBASE_THREAD_POOL_HH
#include "argvec.hh"
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <queue>
#include <deque>
#include <atomic>
#include <unordered_set>
#include <functional>

namespace rb
{

class thread_pool
{
public:
    // total_worker_count is the total number of worker threads.
    // high_priority_worker_count must be less than total_worker_count. Those
    // threads only accept jobs with priority at least equal to "high_priority".
    // This prevents all workers being stuck with long-running low-priority
    // tasks.
    thread_pool(
        size_t total_worker_count = std::thread::hardware_concurrency(),
        size_t high_priority_worker_count = 1,
        uint32_t high_priority = 1
    );
    thread_pool(thread_pool&& other) = delete;
    thread_pool(const thread_pool& other) = delete;
    ~thread_pool();

    class ticket
    {
    friend class thread_pool;
    public:
        ticket();

        bool finished() const;
        void wait();

    private:
        ticket(thread_pool& pool, uint64_t ticket_id);

        thread_pool* pool;
        uint64_t ticket_id;
        mutable bool internal_finished;
    };

    ticket add_task(
        std::function<void()>&& f,
        uint32_t priority = 0,
        argvec<ticket> wait_tickets = {}
    );
    ticket add_tasks(
        argvec<std::function<void()>> f,
        uint32_t priority = 0,
        argvec<ticket> wait_tickets = {}
    );

    // Effectively condenses multiple tickets into one.
    ticket add_barrier(argvec<ticket> wait_tickets = {});

    // Adds a ticket that will finish immediately when finish_manual_ticket() is
    // called on it, but will otherwise never run. This ticket type will freeze
    // on finish() until finish_manual_ticket() is called!
    ticket add_manual_ticket();
    void finish_manual_ticket(ticket t);

    // You can lift the priority of a task after-the-fact if you know better!
    void bump_priority(const ticket& t);

    // Starts running tasks required for 'ticket' to finish on this thread as
    // well, and returns once the ticket is done.
    void finish(const ticket& t);

    // Waits for all currently existing tickets to finish. If more tickets are
    // added during this time, they're not handled.
    void finish_all_existing();

    size_t get_thread_count() const;

private:
    struct task
    {
        std::function<void()> func;
        uint64_t ticket_id;
        uint32_t priority;
        std::unordered_set<uint64_t> wait_tickets;

        bool operator<(const task& other) const;
    };

    void bump_priority_inner(uint64_t ticket_id);
    void finish_inner(uint64_t ticket_id);
    void run_task_inner(task& t);
    bool find_task(uint32_t min_priority, task& t);
    void update_waiting_tasks(uint64_t ticket_id);
    static void worker(thread_pool* pool, uint32_t min_priority);

    std::mutex task_mutex;
    std::atomic_bool quit;
    std::condition_variable new_task_cv;
    std::vector<task> ready_tasks;
    std::deque<task> waiting_tasks;
    std::vector<std::thread> threads;

    std::shared_mutex ticket_mutex;
    std::condition_variable_any ticket_cv;
    uint64_t ticket_counter;
    std::unordered_map<uint64_t, uint32_t> unfinished_tickets;
};

}

#endif
