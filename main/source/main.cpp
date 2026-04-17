#include <emu/devices/8250_uart.hpp>
#include <emu/devices/ram.hpp>
#include <emu/devices/riscv/mmu.hpp>
#include <emu/literals.hpp>
#include <emu/riscv/cpu.hpp>
#include <mutex>
#if defined(__EMSCRIPTEN__)
    #include <emscripten/emscripten.h>
#endif

using namespace ds;
using namespace ds::emu;
using namespace ds::literals;

constexpr std::uint8_t LinuxKernel[] = {
    #embed "boot/Image"
};

constexpr std::uint8_t DeviceTreeBlob[] = {
    #embed "boot/device_tree.dtb"
};

constexpr std::uint8_t InitRamFs[] = {
    #embed "boot/initramfs.cpio"
};

static std::string outputQueue;
static std::uint32_t outputReadEnd = 0;

struct RiscVSoC {
    RiscVSoC() : ram(512_MiB) {
        uart8250.output_callback([](std::uint8_t c) {
            #if defined(__EMSCRIPTEN__)
                outputQueue += char(c);
            #else
                putchar(c);
                fflush(stdout);
            #endif
        });

        emulator.address_space().map(0x0000'0000, &ram);
        emulator.address_space().map(0xF400'0000, &uart8250);
        emulator.address_space().add_address_translator(&riscv_mmu);

        emulator.power_up();

        ram.write(0x00, LinuxKernel);

        constexpr static auto DeviceTreeBlobLoadAddress = 512_MiB - 1_MiB;
        constexpr static auto InitRamFsLoadAddress = 0x1F700000;
        ram.write(DeviceTreeBlobLoadAddress, DeviceTreeBlob);
        ram.write(InitRamFsLoadAddress, InitRamFs);
        emulator.cores()[0].a1() = DeviceTreeBlobLoadAddress;
    }

    auto step() -> void {
        emulator.step();
    }

private:
    riscv::Cpu<1> emulator;
    dev::Ram ram;
    dev::UART8250 uart8250;
    dev::riscv::MMU<std::uint32_t> riscv_mmu;
};

#if defined (__EMSCRIPTEN__)
    namespace {
        constexpr static auto StepsPerIteration = 500'000;
        double tickBudgetMs = 8.0;

        auto emulatorMainLoop(void* arg) -> void {
            auto* soc = static_cast<RiscVSoC*>(arg);
            const auto tickStart = emscripten_get_now();

            do {
                for (std::uint32_t i = 0; i < StepsPerIteration; ++i) {
                    soc->step();
                }
            } while (emscripten_get_now() - tickStart < tickBudgetMs);
        }
    }

    extern "C" EMSCRIPTEN_KEEPALIVE auto getOutputQueue() -> const char* {
        auto result = &outputQueue[outputReadEnd];
        outputReadEnd = outputQueue.size();

        return result;
    }

    extern "C" EMSCRIPTEN_KEEPALIVE auto startEmulator() -> void {
        static RiscVSoC soc;
        static bool started = false;

        if (started) {
            return;
        }

        started = true;
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 0);
        emscripten_set_main_loop_arg(emulatorMainLoop, &soc, 0, false);
    }

    extern "C" EMSCRIPTEN_KEEPALIVE auto stopEmulator() -> void {
        emscripten_cancel_main_loop();
    }
#else
    auto main() -> int {
        RiscVSoC emulator;
        while (true) {
            emulator.step();
        }
    }
#endif