import Testing
@testable import Engine

@Suite struct KeyboardStateTests {
    @Test func initialStateIsEmpty() {
        let s = KeyboardState()
        #expect(s.down.isEmpty)
        #expect(s.pressed.isEmpty)
        #expect(s.released.isEmpty)
        #expect(!s.isDown(.space))
    }

    @Test func pressPopulatesDownAndPressed() {
        var s = KeyboardState()
        s.setKey(.space, down: true)
        #expect(s.isDown(.space))
        #expect(s.wasPressed(.space))
        #expect(!s.wasReleased(.space))
    }

    @Test func repeatedPressDoesNotReFireEdge() {
        var s = KeyboardState()
        s.setKey(.space, down: true)
        s.endFrame()
        s.setKey(.space, down: true)
        #expect(s.isDown(.space))
        #expect(!s.wasPressed(.space), "pressed edge must only fire on transition")
    }

    @Test func releaseRemovesDownAndFiresReleased() {
        var s = KeyboardState()
        s.setKey(.a, down: true)
        s.endFrame()
        s.setKey(.a, down: false)
        #expect(!s.isDown(.a))
        #expect(s.wasReleased(.a))
        #expect(!s.wasPressed(.a))
    }

    @Test func releasingKeyNotDownIsNoOp() {
        var s = KeyboardState()
        s.setKey(.escape, down: false)
        #expect(!s.isDown(.escape))
        #expect(!s.wasReleased(.escape), "released edge must not fire for keys that were never down")
    }

    @Test func endFrameClearsEdgesButKeepsDown() {
        var s = KeyboardState()
        s.setKey(.w, down: true)
        s.setKey(.a, down: true)
        s.setKey(.a, down: false)
        #expect(s.wasPressed(.w))
        #expect(s.wasReleased(.a))

        s.endFrame()

        #expect(s.isDown(.w), "down state must persist across frames")
        #expect(!s.wasPressed(.w))
        #expect(!s.wasReleased(.a))
        #expect(s.pressed.isEmpty)
        #expect(s.released.isEmpty)
    }

    @Test func keysAreIndependent() {
        var s = KeyboardState()
        s.setKey(.leftShift, down: true)
        s.setKey(.rightShift, down: true)
        s.setKey(.leftShift, down: false)
        #expect(!s.isDown(.leftShift))
        #expect(s.isDown(.rightShift))
        #expect(s.wasPressed(.rightShift))
        #expect(s.wasReleased(.leftShift))
    }

    @Test func pressReleasePressInSameFrameLeavesKeyDown() {
        var s = KeyboardState()
        s.setKey(.space, down: true)
        s.setKey(.space, down: false)
        s.setKey(.space, down: true)
        #expect(s.isDown(.space))
        #expect(s.wasPressed(.space))
        #expect(s.wasReleased(.space), "an intra-frame release still counts as a released edge")
    }
}
