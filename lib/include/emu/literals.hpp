#pragma once

namespace ds::literals {

    constexpr auto operator""_Bytes(unsigned long long bytes) { return bytes; }
    constexpr auto operator""_KiB(unsigned long long kiB) { return kiB * 1024_Bytes; }
    constexpr auto operator""_MiB(unsigned long long MiB) { return MiB * 1024_KiB; }
    constexpr auto operator""_GiB(unsigned long long GiB) { return GiB * 1024_MiB; }

}