#pragma once

#include <cstdint>
#include <limits>
#include <concepts>
#include <span>

namespace ds::emu::util {

    template<std::size_t N>
    struct SizedType;;
    template<> struct SizedType<1> { using Signed = std::int8_t;  using Unsigned = std::uint8_t;  };
    template<> struct SizedType<2> { using Signed = std::int16_t; using Unsigned = std::uint16_t; };
    template<> struct SizedType<4> { using Signed = std::int32_t; using Unsigned = std::uint32_t; };
    template<> struct SizedType<8> { using Signed = std::int64_t; using Unsigned = std::uint64_t; };


    template<auto BitNumber, typename T = std::uint32_t>
    constexpr auto bit() -> T {
        return (T(1) << T(BitNumber));
    }

    template<std::uint8_t Size, typename T = std::uint32_t>
    constexpr auto mask() -> T {
        if constexpr (Size == sizeof(T) * 8)
            return ~T(0);
        else
            return (T(1) << Size) - 1;
    }

    template<std::uint8_t From, std::uint8_t To>
    constexpr auto extract_bits(auto value) -> std::remove_cvref_t<decltype(value)> {
        static_assert(From <= To, "To > From");

        constexpr static auto Mask = mask<(To - From) + 1>() << From;
        return (value & Mask) >> From;
    }

    template<std::integral auto ... Widths>
    constexpr auto combine_bits(std::integral auto ... values) -> auto {
        static_assert(sizeof...(values) == sizeof...(Widths), "Provided value count must match the number of bit widths");

        SizedType<((Widths + ...) + 7) / 8> result = 0;
        (
            (result <<= Widths, result |= (values & mask<Widths>())),
            ...
        );

        return result;
    }

    template<std::unsigned_integral T, std::size_t N>
    constexpr auto sign_extend(T value) -> std::make_signed_t<T> {
        static_assert(N > 0, "Bit width N must be greater than 0");
        static_assert(N <= std::numeric_limits<T>::digits, "Bit width N exceeds type width");

        using SignedT = std::make_signed_t<T>;
        constexpr T sign_bit = T(1) << (N - 1);

        if (value & sign_bit) {
            constexpr T mask = (~T(0)) << N;
            return static_cast<SignedT>(value | mask);
        } else {
            return static_cast<SignedT>(value);
        }
    }

    constexpr auto to_byte_span(auto &value) -> std::span<std::uint8_t> {
        return {
            reinterpret_cast<std::uint8_t *>(&value),
            sizeof(value)
        };
    }

    constexpr auto to_byte_span(const auto &value) -> std::span<const std::uint8_t> {
        return {
            reinterpret_cast<const std::uint8_t *>(&value),
            sizeof(value)
        };
    }


    template<typename T>
    struct FunctionSignature { };

    template<typename R, typename... Args>
    struct FunctionSignature<R(*)(Args...)> {
    public:
        using Signature = R(Args...);

        using Arguments = std::tuple<Args...>;
        using ReturnType = R;
        using Class = void;

        constexpr static auto ArgumentCount = sizeof...(Args);
    };

    template<typename ClassType, typename R, typename... Args>
    struct FunctionSignature<R(ClassType::*)(Args...)> {
    public:
        using Signature = R(Args...);

        using Arguments = std::tuple<Args...>;
        using ReturnType = R;
        using Class = ClassType;

        constexpr static auto ArgumentCount = sizeof...(Args);
    };

}