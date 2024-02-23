#ifndef RAYBASE_PHYS_CONTEXT_HH
#define RAYBASE_PHYS_CONTEXT_HH

#include "core/thread_pool.hh"
#include <memory>

namespace rb::phys
{

class context
{
public:
    context(thread_pool* pool = nullptr, uint32_t task_priority = 1000);
    context(const context& other) = delete;
    ~context();

    thread_pool& get_thread_pool() const;

private:
    struct impl_data;
    std::unique_ptr<impl_data> data;
};

}

#endif

