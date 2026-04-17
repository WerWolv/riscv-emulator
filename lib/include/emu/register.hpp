#pragma once

#include <cstdint>
#include <concepts>
#include <tuple>
#include <array>
#include <utility>

namespace ds::emu {

    template<std::integral T>
    class RegisterBase {
    public:
        using Type = T;

        constexpr virtual ~RegisterBase() = default;

        constexpr auto operator+=(Type type) -> RegisterBase& {
            return this->operator=(*this + type);
        }
        constexpr auto operator-=(Type type) -> RegisterBase& {
            return this->operator=(*this - type);
        }
        constexpr auto operator|=(Type type) -> RegisterBase& {
            return this->operator=(*this | type);
        }
        constexpr auto operator&=(Type type) -> RegisterBase& {
            return this->operator=(*this & type);
        }

        constexpr auto get() const -> Type { return operator Type(); }

        constexpr auto operator=(std::derived_from<RegisterBase> auto &other) -> RegisterBase& {
            return operator=(static_cast<T>(other));
        }

        constexpr auto set_bit(std::uint8_t index, bool value) -> void {
            const auto bit = 1U << index;

            if (value)
                (*this) |= bit;
            else
                (*this) &= ~bit;
        }

        constexpr auto get_bit(std::uint8_t index) -> bool {
            const auto bit = 1ULL << index;

            return (*this) & bit;
        }

        constexpr virtual auto operator=(Type type) -> RegisterBase& = 0;
        constexpr virtual operator Type() const = 0;
    };

    template<std::integral T>
    class GeneralPurposeRegister : public RegisterBase<T> {
    public:
        constexpr auto operator=(T type) -> GeneralPurposeRegister& final {
            m_value = type;
            return *this;
        }

        constexpr operator T() const final {
            return m_value;
        }

    private:
        T m_value = 0x00;
    };

    template<typename T>
    class ReadOnlyRegister : public RegisterBase<T> {
    public:
        explicit ReadOnlyRegister(T value) : m_value(value) {}
        constexpr auto operator=(T type) -> ReadOnlyRegister& final {
            std::ignore = type;
            return *this;
        }

        constexpr operator T() const final {
            return m_value;
        }

    private:
        T m_value = 0x00;
    };

    template<typename T>
    class ZeroRegister : public RegisterBase<T> {
    public:
        constexpr auto operator=(T type) -> ZeroRegister& final {
            std::ignore = type;
            return *this;
        }

        constexpr operator T() const final {
            return 0x00;
        }
    };

}
