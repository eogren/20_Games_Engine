#include "Semaphore.h"

Semaphore::Semaphore(long initialValue) : handle_(dispatch_semaphore_create(initialValue)) {}

Semaphore::~Semaphore()
{
    if (handle_) dispatch_release(handle_);
}
