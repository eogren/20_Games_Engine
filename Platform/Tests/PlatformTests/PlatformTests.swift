import Testing
@testable import Platform

#if os(macOS)
import AppKit

@Suite @MainActor struct HostMainMenuTests {
    @Test func exposesSingleAppSubmenu() throws {
        let menu = Host.makeMainMenu()
        #expect(menu.items.count == 1)
        let appItem = try #require(menu.items.first)
        #expect(appItem.submenu != nil)
    }

    @Test func quitItemBindsCmdQToTerminate() throws {
        let menu = Host.makeMainMenu()
        let appMenu = try #require(menu.items.first?.submenu)
        let quit = try #require(appMenu.items.first)

        #expect(quit.keyEquivalent == "q")
        #expect(quit.keyEquivalentModifierMask == .command)
        #expect(quit.action == #selector(NSApplication.terminate(_:)))
    }

    @Test func quitItemTitleNamesTheProcess() throws {
        let menu = Host.makeMainMenu()
        let quit = try #require(menu.items.first?.submenu?.items.first)
        #expect(quit.title == "Quit \(ProcessInfo.processInfo.processName)")
    }
}
#endif
