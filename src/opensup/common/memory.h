#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace opensup {
namespace common {

class memory_c;

using memory_cptr = std::shared_ptr<memory_c>;

class memory_c {
public:
    static memory_cptr alloc(size_t size);
    static memory_cptr clone(const uint8_t* data, size_t size);
    static memory_cptr clone(const std::vector<uint8_t>& data);
    static memory_cptr borrow(uint8_t* data, size_t size);

    memory_c(const memory_c&) = delete;
    memory_c& operator=(const memory_c&) = delete;

    memory_c(memory_c&& other) noexcept;
    memory_c& operator=(memory_c&& other) noexcept;

    ~memory_c() = default;

    [[nodiscard]] const uint8_t* get_buffer() const noexcept { return m_data.get(); }
    [[nodiscard]] uint8_t* get_buffer() noexcept { return m_data.get(); }
    [[nodiscard]] size_t get_size() const noexcept { return m_size; }

    [[nodiscard]] std::vector<uint8_t> to_vector() const;

private:
    memory_c() = default;

    struct deleter_c {
        void operator()(uint8_t* p) noexcept { std::free(p); }
    };

    std::unique_ptr<uint8_t[], deleter_c> m_data;
    size_t m_size = 0;
};

} // namespace common
} // namespace opensup
