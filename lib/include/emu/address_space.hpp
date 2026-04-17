#pragma once

#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <set>
#include <vector>

#include <emu/core.hpp>

namespace ds::emu {

    enum class AccessResult {
        Success,
        LoadMisalign,
        StoreMisalign,
        LoadAccessFault,
        StoreAccessFault,
        LoadPageFault,
        StorePageFault,
        FetchPageFault
    };

    template<typename T>
    class MemoryMappedPeripheral {
    public:
        using Offset = T;

        constexpr explicit MemoryMappedPeripheral(std::size_t size) : m_size(size) {}
        virtual ~MemoryMappedPeripheral() = default;

        virtual auto read(Offset offset, std::span<std::uint8_t> buffer) -> AccessResult = 0;
        virtual auto write(Offset offset, std::span<const std::uint8_t> buffer) -> AccessResult = 0;
        virtual auto reset() -> void = 0;

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return m_size; }

    private:
        std::size_t m_size;
    };

    template<typename T>
    class AddressSpace;

    enum class AccessType { Instruction, Load, Store };

    template<typename T>
    class AddressTranslator {
    public:
        virtual ~AddressTranslator() = default;
        constexpr virtual auto translate(Core &core, T virtual_address, AccessType access_type) -> std::expected<T, AccessResult> = 0;
        constexpr virtual auto invalidate() -> void = 0;
    };

    template<typename T>
    class AddressSpace {
    public:
        constexpr auto read(Core &core, T virtual_address, std::span<std::uint8_t> buffer) -> AccessResult {
            const auto physical_address = translate_address(core, virtual_address, AccessType::Load);
            if (!physical_address.has_value()) [[unlikely]] {
                return AccessResult::LoadPageFault;
            }
            return read_physical(*physical_address, buffer);
        }

        constexpr auto read_physical(T address, std::span<std::uint8_t> buffer) -> AccessResult {
            if (auto entry = get(address); entry != nullptr)
                return entry->peripheral->read(address - entry->base_address, buffer);

            return AccessResult::LoadAccessFault;
        }

        constexpr auto write(Core &core, T virtual_address, std::span<const std::uint8_t> buffer) -> AccessResult {
            const auto physical_address = translate_address(core, virtual_address, AccessType::Store);
            if (!physical_address.has_value()) [[unlikely]] {
                return AccessResult::StorePageFault;
            }
            return write_physical(*physical_address, buffer);
        }

        constexpr auto write_physical(T address, std::span<const std::uint8_t> buffer) -> AccessResult {
            if (auto entry = get(address); entry != nullptr)
                return entry->peripheral->write(address - entry->base_address, buffer);

            return AccessResult::StoreAccessFault;
        }

        struct PeripheralEntry {
            T base_address;
            MemoryMappedPeripheral<T> *peripheral;

            auto operator<=>(const PeripheralEntry &other) const noexcept -> auto {
                return this->base_address <=> other.base_address;
            }
        };

        constexpr auto get(T address) -> const PeripheralEntry* {
            auto it = std::find_if(m_peripherals.begin(), m_peripherals.end(),[address](const PeripheralEntry &peripheral) {
                const T start = peripheral.base_address;
                const T end   = start + peripheral.peripheral->size();

                return address >= start and address < end;
            });

            if (it == m_peripherals.end()) [[unlikely]] {
                return nullptr;
            } else {
                return &*it;
            }
        }

        constexpr auto map(T base_address, MemoryMappedPeripheral<T> *peripheral) -> void {
            m_peripherals.insert(PeripheralEntry{ base_address, peripheral });
        }

        constexpr auto add_address_translator(AddressTranslator<T> *translator) -> void {
            m_address_translators.emplace_back(translator);
        }

        constexpr auto invalidate() -> void {
            for (const auto &entry : m_address_translators) {
                entry->invalidate();
            }
        }

        constexpr auto reset() -> void {
            this->invalidate();
            for (const auto &entry : m_peripherals) {
                entry.peripheral->reset();
            }
        }

        constexpr auto translate_address(Core &core, T virtual_address, AccessType access) -> std::expected<T, AccessResult> {
            T physical_address = virtual_address;
            for (const auto &translator : m_address_translators) {
                const auto result = translator->translate(core, physical_address, access);
                if (!result.has_value())
                    return result;

                physical_address = result.value();
            }

            return physical_address;
        }

    private:
        std::set<PeripheralEntry> m_peripherals;
        std::vector<AddressTranslator<T>*> m_address_translators;
    };

}