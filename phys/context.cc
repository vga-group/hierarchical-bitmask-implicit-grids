#include "context.hh"
#include "core/stack_allocator.hh"
#include "core/math.hh"
#include <cstdarg>
#include <atomic>
#include "LinearMath/btThreads.h"

void btPushThreadsAreRunning();
void btPopThreadsAreRunning();

namespace
{
using namespace rb;

class custom_task_scheduler: public btITaskScheduler
{
public:
    custom_task_scheduler(thread_pool* pool, uint32_t priority)
    : btITaskScheduler("Raybase"), pool(pool), priority(priority)
    {
    }

    ~custom_task_scheduler()
    {
    }

    int getMaxNumThreads() const override
    {
        return pool->get_thread_count()+1;
    }

    int getNumThreads() const override
    {
        return pool->get_thread_count()+1;
    }

    void setNumThreads(int) override {}

    void parallelFor(int begin, int end, int grain_size, const btIParallelForBody& body) override
    {
        btPushThreadsAreRunning();

        int count = ((end - begin) + grain_size-1) / grain_size;
        auto funcs = stack_allocate<std::function<void()>>(count);
        for(int i = 0; i < count; ++i)
        {
            funcs[i] = [&, i=i](){
                body.forLoop(
                    begin + i * grain_size,
                    rb::min(begin + (i+1)*grain_size, end)
                );
            };
        }
        pool->add_tasks(funcs, priority).wait();

        btPopThreadsAreRunning();
    }

    btScalar parallelSum(int begin, int end, int grain_size, const btIParallelSumBody& body) override
    {
        btPushThreadsAreRunning();
        int count = ((end - begin) + grain_size-1) / grain_size;

        auto temporaries = stack_allocate<btScalar>(count);
        auto funcs = stack_allocate<std::function<void()>>(count);
        for(int i = 0; i < count; ++i)
        {
            funcs[i] = [&, i=i](){
                temporaries[i] = body.sumLoop(
                    begin + i * grain_size,
                    rb::min(begin + (i+1)*grain_size, end)
                );
            };
        }

        pool->add_tasks(funcs, priority).wait();

        btScalar sum = btScalar(0);
        for(int i = 0; i < count; ++i)
            sum += temporaries[i];

        btPopThreadsAreRunning();
        return sum;
    }

private:
    thread_pool* pool;
    uint32_t priority;
};

}

namespace rb::phys
{

struct context::impl_data
{
    impl_data(thread_pool* pool, uint32_t priority)
    :   pool(pool),
        owned_threads(pool ? nullptr : new thread_pool()),
        task_scheduler(pool ? pool : owned_threads.get(), priority)
    {
        if(!pool) this->pool = owned_threads.get();
    }

    thread_pool* pool;
    std::unique_ptr<thread_pool> owned_threads;
    custom_task_scheduler task_scheduler;
};

context::context(thread_pool* pool, uint32_t task_priority)
{
    data.reset(new impl_data(pool, task_priority));
    btSetTaskScheduler(&data->task_scheduler);
}

context::~context()
{
    // Stall while threads are running, it isn't safe to reset the scheduler
    // otherwise.
    while(btThreadsAreRunning())
        ;
    btSetTaskScheduler(nullptr);
    data.reset();
}

thread_pool& context::get_thread_pool() const
{
    return *data->pool;
}

}
