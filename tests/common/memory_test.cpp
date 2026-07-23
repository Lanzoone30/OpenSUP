#include <gtest/gtest.h>
#include "opensup/common/memory.h"

namespace opensup {
namespace common {
namespace {

TEST(Memory, Alloc) {
    auto mem = memory_c::alloc(1024);
    EXPECT_NE(mem, nullptr);
    EXPECT_EQ(mem->get_size(), 1024);
    EXPECT_NE(mem->get_buffer(), nullptr);
}

TEST(Memory, AllocZero) {
    auto mem = memory_c::alloc(0);
    EXPECT_NE(mem, nullptr);
    EXPECT_EQ(mem->get_size(), 0);
}

TEST(Memory, Clone) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto mem = memory_c::clone(data);
    EXPECT_EQ(mem->get_size(), 5);
    EXPECT_EQ(mem->get_buffer()[0], 1);
    EXPECT_EQ(mem->get_buffer()[4], 5);
}

TEST(Memory, CloneRaw) {
    uint8_t raw[] = {10, 20, 30};
    auto mem = memory_c::clone(raw, 3);
    EXPECT_EQ(mem->get_size(), 3);
    EXPECT_EQ(mem->get_buffer()[0], 10);
    EXPECT_EQ(mem->get_buffer()[2], 30);
}

TEST(Memory, ToVector) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto mem = memory_c::clone(data);
    auto vec = mem->to_vector();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[2], 3);
}

TEST(Memory, Move) {
    auto mem1 = memory_c::alloc(100);
    auto ptr = mem1->get_buffer();
    auto mem2 = std::move(*mem1);
    EXPECT_EQ(mem2.get_size(), 100);
    EXPECT_EQ(mem2.get_buffer(), ptr);
    EXPECT_EQ(mem1->get_size(), 0);
}

} // namespace
} // namespace common
} // namespace opensup
