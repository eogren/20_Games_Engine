#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalDisplayLink.h>

@interface PongApplication : NSObject <NSApplicationDelegate, NSWindowDelegate, CAMetalDisplayLinkDelegate>
@end
#endif
