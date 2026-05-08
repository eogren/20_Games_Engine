#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>

@interface PongApplication : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end
#endif