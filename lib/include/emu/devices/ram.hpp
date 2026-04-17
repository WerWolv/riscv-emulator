#pragma once

#include <emu/address_space.hpp>

namespace ds::emu::dev {

    class Ram : public MemoryMappedPeripheral<std::uint32_t> {
    public:
        explicit Ram(std::size_t size) : MemoryMappedPeripheral(size) {
            m_data.resize(size);
        }

        auto read(Offset offset, std::span<std::uint8_t> buffer) -> AccessResult final {
            std::memcpy(buffer.data(), m_data.data() + offset, buffer.size_bytes());
            return AccessResult::Success;
        }

        auto write(Offset offset, std::span<const std::uint8_t> buffer) -> AccessResult final {
            std::memcpy(m_data.data() + offset, buffer.data(), buffer.size_bytes());
            return AccessResult::Success;
        }

        auto reset() -> void final {
            std::memset(m_data.data(), 0x00, m_data.size());
        }

    private:
        std::vector<std::uint8_t> m_data;
    };

}