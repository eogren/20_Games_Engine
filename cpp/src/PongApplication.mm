#import "PongApplication.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

@interface GameWindow : NSWindow
@end

@implementation GameWindow
// Overriding this function allows to prevent clicking noise when using keyboard and esc key to go windowed
- (void)keyDown:(NSEvent *)event
{
}
@end

@interface GameView : NSView
@end

@implementation GameView

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    [self updateDrawableSize];
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self updateDrawableSize];
}

- (void)updateDrawableSize
{
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    if (![metalLayer isKindOfClass:[CAMetalLayer class]]) return;

    CGFloat scale = self.window.backingScaleFactor;
    if (scale <= 0) scale = 1.0;

    metalLayer.contentsScale = scale;
    metalLayer.drawableSize  = CGSizeMake(self.bounds.size.width  * scale,
                                          self.bounds.size.height * scale);
}

@end

@implementation PongApplication
{
    GameWindow*  _window;
    NSView*                    _view;
    CAMetalLayer*              _metalLayer;
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)showWindow
{
    [_window setIsVisible:YES];
    [_window makeMainWindow];
    [_window makeKeyAndOrderFront:_window];
}

- (void)createWindow
{
    NSWindowStyleMask mask = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    NSScreen * screen = [NSScreen mainScreen];
    
    NSRect contentRect = NSMakeRect(0, 0, 1280, 720);
    
    // Center window to the middle of the screen
    contentRect.origin.x = (screen.frame.size.width / 2) - (contentRect.size.width / 2);
    contentRect.origin.y = (screen.frame.size.height / 2) - (contentRect.size.height / 2);
    
    _window = [[GameWindow alloc] initWithContentRect:contentRect
                                            styleMask:mask
                                              backing:NSBackingStoreBuffered
                                                defer:NO
                                               screen:screen];
    
    _window.releasedWhenClosed = NO;
    _window.minSize = NSMakeSize(640, 360);
    _window.delegate = self;
    _window.title = @"Pong";
}

// Create the view and Metal layer backing it that renders the game
- (void)createView
{
    NSAssert(_window, @"You need to create the window before the view");

    _metalLayer = [[CAMetalLayer alloc] init];
    _metalLayer.device = MTLCreateSystemDefaultDevice();
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.opaque = YES;
    _metalLayer.framebufferOnly = YES;

    _view = [[GameView alloc] initWithFrame:_window.contentLayoutRect];
    _view.layer = _metalLayer;
    _view.wantsLayer = YES;

    // Setting the contentView triggers GameView's viewDidChangeBackingProperties,
    // which seeds drawableSize / contentsScale from the window's backing scale.
    _window.contentView = _view;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self createWindow];
    [self createView];
    [self showWindow];
}

@end
