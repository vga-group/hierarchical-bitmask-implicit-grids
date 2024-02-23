#include "thread_pool.hh"

namespace rb
{

thread_pool::thread_pool(
    size_t total_worker_count,
    size_t high_priority_worker_count,
    uint32_t high_priority
): ticket_counter(0)
{
    quit = false;
    total_worker_count = std::max(
        total_worker_count, 1lu
    );
    high_priority_worker_count = std::min(
        high_priority_worker_count,
        total_worker_count-1
    );
    for(size_t i = 0; i < total_worker_count; ++i)
    {
        threads.emplace_back(
            worker, this, i < high_priority_worker_count ? high_priority : 0
        );
    }
}

thread_pool::~thread_pool()
{
    quit = true;
    new_task_cv.notify_all();
    for(auto& thread: threads)
        thread.join();
}

thread_pool::ticket::ticket()
: pool(nullptr), ticket_id(0), internal_finished(true)
{
}

thread_pool::ticket::ticket(thread_pool& pool, uint64_t ticket_id)
: pool(&pool), ticket_id(ticket_id), internal_finished(false)
{
}

bool thread_pool::ticket::finished() const
{
    if(!internal_finished && pool)
    {
        std::shared_lock lk(pool->ticket_mutex);
        internal_finished = !pool->unfinished_tickets.count(ticket_id);
    }
    return internal_finished;
}

void thread_pool::ticket::wait()
{
    if(internal_finished || !pool) return;

    bool unfinished = false;
    bool running = false;

    std::shared_lock lk(pool->ticket_mutex);

    if(pool->unfinished_tickets.count(ticket_id))
    {
        // Unfinished, so we start processing related tasks on this thread too.
        // This approach can also resolve some deadlocks, as worker threads
        // implicitly waiting for other tickets start doing those tickets' work
        // as well (but please still use wait_tickets, it's faster)
        lk.unlock();
        pool->finish(*this);
    }
    internal_finished = true;
}

bool thread_pool::find_task(uint32_t min_priority, task& t)
{
    if(
        ready_tasks.size() > 0 &&
        ready_tasks.front().priority >= min_priority
    ){
        std::pop_heap(ready_tasks.begin(), ready_tasks.end());
        t = std::move(ready_tasks.back());
        ready_tasks.pop_back();
        return true;
    }
    return false;
}

void thread_pool::worker(thread_pool* pool, uint32_t min_priority)
{
    for(;;)
    {
        task t;
        {
            std::unique_lock lk(pool->task_mutex);
            while(!pool->find_task(min_priority, t))
            {
                if(pool->ready_tasks.size() == 0 && pool->quit)
                    return;
                pool->new_task_cv.wait(lk);
            }
        }

        pool->run_task_inner(t);
    }
}

thread_pool::ticket thread_pool::add_task(
    std::function<void()>&& f,
    uint32_t priority,
    argvec<ticket> wait_tickets
){
    uint64_t id = 0;
    {
        std::unique_lock lk(task_mutex);
        if(wait_tickets.size() == 0)
        {
            {
                std::unique_lock lk(ticket_mutex);
                id = ticket_counter++;
                unfinished_tickets.emplace(id, 1);
            }
            ready_tasks.emplace_back(task{std::move(f), id, priority, {}});
            std::push_heap(ready_tasks.begin(), ready_tasks.end());
        }
        else
        {
            std::unordered_set<uint64_t> wait_ticket_ids;
            {
                std::unique_lock lk(ticket_mutex);
                for(const ticket& t: wait_tickets)
                {
                    if(unfinished_tickets.count(t.ticket_id) && t.pool != nullptr)
                        wait_ticket_ids.insert(t.ticket_id);
                }
                id = ticket_counter++;
                unfinished_tickets.emplace(id, 1);
            }
            if(wait_ticket_ids.size() == 0)
            {
                ready_tasks.emplace_back(task{std::move(f), id, priority, {}});
                std::push_heap(ready_tasks.begin(), ready_tasks.end());
            }
            else
            {
                waiting_tasks.emplace_back(task{std::move(f), id, priority, std::move(wait_ticket_ids)});
            }
        }
    }
    new_task_cv.notify_one();
    return ticket(*this, id);
}

thread_pool::ticket thread_pool::add_tasks(
    argvec<std::function<void()>> f,
    uint32_t priority,
    argvec<ticket> wait_tickets
){
    if(f.size() == 0)
        return ticket(*this, -1);

    uint64_t id = 0;
    {
        std::unique_lock lk(task_mutex);
        if(wait_tickets.size() == 0)
        {
            {
                std::unique_lock lk(ticket_mutex);
                id = ticket_counter++;
                unfinished_tickets.emplace(id, f.size());
            }
            for(auto& func: f)
                ready_tasks.emplace_back(task{std::move(func), id, priority, {}});
            std::make_heap(ready_tasks.begin(), ready_tasks.end());
        }
        else
        {
            std::unordered_set<uint64_t> wait_ticket_ids;
            {
                std::unique_lock lk(ticket_mutex);
                for(const ticket& t: wait_tickets)
                {
                    if(unfinished_tickets.count(t.ticket_id))
                        wait_ticket_ids.insert(t.ticket_id);
                }
                id = ticket_counter++;
                unfinished_tickets.emplace(id, f.size());
            }
            for(auto& func: f)
                waiting_tasks.emplace_back(task{std::move(func), id, priority, wait_ticket_ids});
        }
    }
    new_task_cv.notify_all();
    return ticket(*this, id);
}

thread_pool::ticket thread_pool::add_barrier(
    argvec<ticket> wait_tickets
){
    if(wait_tickets.size() == 0)
        return ticket(*this, -1);

    uint64_t id = 0;
    {
        std::unique_lock lk(task_mutex);
        std::unordered_set<uint64_t> wait_ticket_ids;
        {
            std::unique_lock lk(ticket_mutex);
            for(const ticket& t: wait_tickets)
            {
                if(unfinished_tickets.count(t.ticket_id) && t.pool != nullptr)
                    wait_ticket_ids.insert(t.ticket_id);
            }
            id = ticket_counter++;
            unfinished_tickets.emplace(id, 1);
        }
        if(wait_ticket_ids.size() == 0)
            return ticket(*this, -1);
        else
            waiting_tasks.emplace_back(
                task{{}, id, UINT32_MAX, std::move(wait_ticket_ids)}
            );
    }
    return ticket(*this, id);
}

thread_pool::ticket thread_pool::add_manual_ticket()
{
    uint64_t id = 0;
    {
        std::unique_lock lk(ticket_mutex);
        id = ticket_counter++;
        unfinished_tickets.emplace(id, 1);
    }
    return ticket(*this, id);
}

void thread_pool::finish_manual_ticket(ticket t)
{
    {
        std::unique_lock lk(ticket_mutex);
        unfinished_tickets.erase(t.ticket_id);
    }

    ticket_cv.notify_all();
    update_waiting_tasks(t.ticket_id);
}

void thread_pool::bump_priority(const ticket& t)
{
    std::unique_lock lk(task_mutex);
    bump_priority_inner(t.ticket_id);
    std::make_heap(ready_tasks.begin(), ready_tasks.end());
}

void thread_pool::finish(const ticket& t)
{
    finish_inner(t.ticket_id);
}

void thread_pool::finish_all_existing()
{
    std::unique_lock lk(task_mutex);
    std::unordered_map<uint64_t, uint32_t> unfinished_tickets = this->unfinished_tickets;
    lk.unlock();

    for(auto [id, count]: unfinished_tickets)
        finish_inner(id);
}

size_t thread_pool::get_thread_count() const
{
    return threads.size();
}

void thread_pool::bump_priority_inner(uint64_t ticket_id)
{
    auto it = unfinished_tickets.find(ticket_id);
    uint32_t count = 0;
    if(it != unfinished_tickets.end())
    {
        count = it->second;
        for(task& t: ready_tasks)
        {
            if(t.ticket_id == ticket_id)
            {
                t.priority++;
                count--;
                if(count == 0)
                    return;
            }
        }
        for(task& t: waiting_tasks)
        {
            if(t.ticket_id == ticket_id)
            {
                t.priority++;
                for(uint64_t wait_id: t.wait_tickets)
                    bump_priority_inner(wait_id);
                count--;
                if(count == 0)
                    return;
            }
        }
    }
}

void thread_pool::finish_inner(uint64_t ticket_id)
{
    for(;;)
    {
        task local_task;
        uint64_t wait_ticket_id;
        bool found_ready = false;
        bool found_wait = false;
        {
            std::unique_lock lk(task_mutex);
            // Find the task in question.
            for(auto it = ready_tasks.begin(); it != ready_tasks.end(); ++it)
            {
                if(it->ticket_id == ticket_id)
                {
                    local_task = std::move(*it);
                    std::swap(*it, ready_tasks.back());
                    ready_tasks.pop_back();
                    std::make_heap(ready_tasks.begin(), ready_tasks.end());
                    found_ready = true;
                    break;
                }
            }

            if(!found_ready)
            { // Could not find the task from ready_tasks, so check if it's
              // waiting instead.
                for(task& t: waiting_tasks)
                {
                    if(t.ticket_id == ticket_id)
                    {
                        // Just pick a random needed task and start doing it.
                        wait_ticket_id = *t.wait_tickets.begin();
                        found_wait = true;
                        break;
                    }
                }
            }
        }

        if(found_ready)
        {
            run_task_inner(local_task);

            if(!unfinished_tickets.count(ticket_id))
                break;
        }
        else if(found_wait)
        {
            finish_inner(wait_ticket_id);
        }
        else
        { // Our task is neither ready nor waiting, so it's either already done
          // or running.
            std::shared_lock lk(ticket_mutex);

            if(unfinished_tickets.count(ticket_id))
            {
                // It's running, so just wait until it finishes.
                ticket_cv.wait(lk, [&]{return !unfinished_tickets.count(ticket_id);});
            }
            // By this point, the task isn't unfinished anymore (so it's
            // finished). We can quit.
            break;
        }
    }
}

void thread_pool::run_task_inner(task& t)
{
    if(t.func)
        t.func();

    bool ticket_finished = false;
    {
        std::unique_lock lk(ticket_mutex);
        auto it = unfinished_tickets.find(t.ticket_id);
        if(it != unfinished_tickets.end())
        {
            it->second--;
            if(it->second == 0)
            {
                unfinished_tickets.erase(t.ticket_id);
                ticket_finished = true;
            }
        }
    }

    if(ticket_finished)
    {
        ticket_cv.notify_all();

        update_waiting_tasks(t.ticket_id);
    }
}

void thread_pool::update_waiting_tasks(uint64_t ticket_id)
{
    int new_tasks = 0;
    {
        std::unique_lock lk(task_mutex);
        for(auto it = waiting_tasks.begin(); it != waiting_tasks.end();)
        {
            task& w = *it;
            w.wait_tickets.erase(ticket_id);
            if(w.wait_tickets.size() == 0)
            {
                ready_tasks.emplace_back(std::move(w));
                std::push_heap(ready_tasks.begin(), ready_tasks.end());
                it = waiting_tasks.erase(it);
                new_tasks++;
            }
            else ++it;
        }
    }

    if(new_tasks == 1)
        new_task_cv.notify_one();
    else if(new_tasks > 1)
        new_task_cv.notify_all();
}

bool thread_pool::task::operator<(const task& other) const
{
    return priority < other.priority;
}

}
