#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#elif TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif

#import "PongApplication.h"

#if TARGET_OS_OSX
// Create system menu and set default shortcuts typically expected by macOS users.
static void createApplicationMenu(NSApplication* app)
{
    app.mainMenu = [[NSMenu alloc] init];

    NSString* bundleName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"] ?: @"Pong";

    // Create about menu
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];

    [appMenu addItemWithTitle:[@"About " stringByAppendingString:bundleName]
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];

    [appMenu addItem:[NSMenuItem separatorItem]];

    [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:bundleName] action:@selector(hide:) keyEquivalent:@"h"];

    NSMenuItem* hide_other_item = [appMenu addItemWithTitle:@"Hide Others"
                                                     action:@selector(hideOtherApplications:)
                                              keyEquivalent:@"h"];
    hide_other_item.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;

    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

    [appMenu addItem:[NSMenuItem separatorItem]];

    [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:bundleName]
                       action:@selector(terminate:)
                keyEquivalent:@"q"];

    appMenuItem.submenu = appMenu;

    [app.mainMenu addItem:appMenuItem];

    // Create window menu
    NSMenu* windowsMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    NSMenuItem* windowsMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];

    [windowsMenu addItemWithTitle:NSLocalizedString(@"Minimize", @"")
                           action:@selector(performMiniaturize:)
                    keyEquivalent:@"m"];
    windowsMenuItem.submenu = windowsMenu;
    [app.mainMenu addItem:windowsMenuItem];

    app.windowsMenu = windowsMenu;
}

int main(int argc, const char* argv[])
{
    NSApplication* application = [NSApplication sharedApplication];
    createApplicationMenu(application);

    // Set up the application and window delegate:
    PongApplication* gameApplication = [[PongApplication alloc] init];
    application.delegate = gameApplication;

    // Yield the game loop to the `NSApplication`, which calls
    // application:didFinishLaunchingWithOptions: from the application delegate
    // (an instance of `macOS/GameApplication`), where this sample then sets
    // up view, and game.
    return NSApplicationMain(argc, argv);
}
#endif
