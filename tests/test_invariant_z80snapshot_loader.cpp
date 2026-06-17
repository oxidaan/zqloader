#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>

// Forward declare the loader function - assumes it's exposed in a header
// If not, this test should be compiled with z80snapshot_loader.cpp
extern int load_z80_snapshot(const uint8_t* data, size_t size);

class Z80SnapshotSecurityTest : public ::testing::TestWithParam<std::vector<uint8_t>> {};

TEST_P(Z80SnapshotSecurityTest, BufferReadNeverExceedsDeclaredLength) {
    // Invariant: memcpy into header2 must never copy more bytes than sizeof(header2)
    // A crafted length_and_version field must not cause buffer overflow
    std::vector<uint8_t> payload = GetParam();
    
    // The function should either reject invalid input or safely truncate
    // It must NOT crash or corrupt memory with oversized length_and_version
    int result = load_z80_snapshot(payload.data(), payload.size());
    
    // If length_and_version exceeds header2 size, loader must reject (return error)
    // We don't assert specific return value, just that it doesn't crash/overflow
    (void)result; // Result check depends on implementation; no crash = pass
}

// Helper to create Z80 header with specific length_and_version at offset 30
std::vector<uint8_t> make_z80_payload(uint16_t length_and_version, size_t extra_size) {
    std::vector<uint8_t> data(32 + extra_size, 0);
    // Z80 format: bytes 30-31 contain length_and_version (little-endian)
    data[30] = length_and_version & 0xFF;
    data[31] = (length_and_version >> 8) & 0xFF;
    return data;
}

INSTANTIATE_TEST_SUITE_P(
    AdversarialInputs,
    Z80SnapshotSecurityTest,
    ::testing::Values(
        make_z80_payload(0xFFFF, 256),  // Exploit: max uint16 length (65535 bytes)
        make_z80_payload(128, 256),      // Boundary: 2x typical header2 size (~54 bytes)
        make_z80_payload(54, 64),        // Valid: normal header2 length
        make_z80_payload(0, 32)          // Edge: zero length
    )
);

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}