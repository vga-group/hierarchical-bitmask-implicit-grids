#ifndef RAYBASE_ARGVEC_HH
#define RAYBASE_ARGVEC_HH
#include <initializer_list>
#include <cstdio>
#include <vector>
#include <array>

namespace rb
{

// Wrapper that allows passing an initializer list or array or vector as an
// argument to a function without additional memory allocations.
// The lifetimes are only handled safely in that specific case, so don't use
// this type outside of function parameters.
//
// You can think of this as std::span that
// * Doesn't require C++20
// * Is const-only
// * Also allows initializer_list and is less safe for that reason ;)
template<typename T>
struct argvec
{
public:
    argvec(): mdata(nullptr), msize(0) {}

    argvec(const T& init)
    : mdata(&init), msize(1) {}

    argvec(const T* data, std::size_t count)
    : mdata(data), msize(count) {}

    argvec(std::initializer_list<T> init)
    : mdata(std::data(init)), msize(std::size(init)) {}

    template<size_t size>
    argvec(const T(&array)[size])
    : mdata(array), msize(size) {}

    template<typename U,
        typename = std::enable_if_t<std::is_convertible_v<decltype(std::data(std::declval<U>())), const T*>>
    > argvec(const U& init)
    : mdata(std::data(init)), msize(std::size(init)) {}

    bool empty() {return msize == 0;}
    std::size_t size() {return msize;}
    const T* data() {return mdata;}
    const T& operator[](std::size_t i) {return mdata[i];}

    const T* begin() const {return mdata;}
    const T* end() const {return mdata+msize;}

    // Assuming T is a pointer type, 'voidifies' it.
    argvec<void*> make_void_ptr() const
    {
        return argvec<void*>((void**)mdata, msize);
    }

    // Returns an argvec slice with duplicate entries from its front and back
    // removed.
    argvec<T> clip() const
    {
        const T* new_begin = mdata;
        const T* new_end = mdata+msize;
        while(new_begin+1 < new_end && new_begin[0] == new_begin[1])
            new_begin++;
        while(new_end > new_begin+2 && new_end[-1] == new_end[-2])
            new_end--;
        return argvec<T>(new_begin, new_end - new_begin);
    }

    argvec<T> slice(size_t begin, size_t length)
    {
        begin = std::min(begin, msize);
        return argvec<T>(mdata+begin, std::min(length, msize-begin));
    }

    template<typename F>
    auto map(F&& f) const -> std::vector<decltype(f(std::declval<const T&>()))>
    {
        std::vector<decltype(f(std::declval<const T&>()))> res(msize);
        for(std::size_t i = 0; i < msize; ++i)
            res[i] = f(mdata[i]);
        return res;
    }

private:
    const T* mdata;
    std::size_t msize;
};

}
#endif
