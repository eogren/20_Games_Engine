#include "Platform.h"
#import <QuartzCore/QuartzCore.h>

namespace platform
{
    double currentTime()
    {
        return CACurrentMediaTime();
    }
} // namespace platform