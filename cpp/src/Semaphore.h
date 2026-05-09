#pragma once
#include <dispatch/dispatch.h>

// Owns a dispatch_semaphore_t. Destructor is out-of-line because dispatch_release
// is unavailable under OS_OBJECT_USE_OBJC=1 (i.e. ARC-enabled .mm translation
// units). Keeping ~Semaphore in Semaphore.cpp confines the release call to a
// pure-C++ TU where OS_OBJECT_USE_OBJC=0.
class Semaphore
{
public:
    explicit Semaphore(long initialValue);
    ~Semaphore();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&&) = delete;
    Semaphore& operator=(Semaphore&&) = delete;

    void wait()
    {
        dispatch_semaphore_wait(handle_, DISPATCH_TIME_FOREVER);
    }
    void signal()
    {
        dispatch_semaphore_signal(handle_);
    }

private:
    dispatch_semaphore_t handle_;
};
