#pragma once

#include <emu/utils.hpp>
#include <emu/register.hpp>
#include <emu/address_space.hpp>
#include <functional>
#include <cstring>

namespace ds::emu::dev {

    class UART8250 : public MemoryMappedPeripheral<std::uint32_t> {
    public:
        explicit UART8250() : MemoryMappedPeripheral(0x100000) { }

        auto read(Offset offset, std::span<std::uint8_t> buffer) -> AccessResult final {
            const auto reg = get_register(offset);
            if (reg == nullptr)
                return AccessResult::LoadPageFault;

            std::memset(buffer.data(), 0, buffer.size());
            buffer[0] = *reg;

            return AccessResult::Success;
        }

        auto write(Offset offset, std::span<const std::uint8_t> buffer) -> AccessResult final {
            const auto reg = get_register(offset);
            if (reg == nullptr)
                return AccessResult::StorePageFault;

            *reg = buffer[0];

            return AccessResult::Success;
        }

        auto reset() -> void final {
            m_registers.LSR = util::bit<5>() | util::bit<6>(); // THRE and TSRE bit
        }

        void output_callback(std::function<void(std::uint8_t)> callback) {
            m_registers.ReceiveTransmitBuffer.write_callback = std::move(callback);
        }

    private:
        constexpr auto get_register(Offset offset) -> RegisterBase<std::uint8_t>* {
            switch (offset) {
                case 0:
                    if (m_registers.DLAB())
                        return &m_registers.DLLS;
                    else
                        return &m_registers.ReceiveTransmitBuffer;
                case 1:
                    if (m_registers.DLAB())
                        return &m_registers.DLMS;
                    else
                        return &m_registers.IER;
                case 2:
                    return &m_registers.IIR;
                case 3:
                    return &m_registers.LCR;
                case 4:
                    return &m_registers.MCR;
                case 5:
                    return &m_registers.LSR;
                case 6:
                    return &m_registers.MSR;
                default:
                    return nullptr;
            }
        }

        struct Registers;
        struct InputOutputRegister : public RegisterBase<std::uint8_t> {
            InputOutputRegister(Registers *registers){}
            constexpr auto operator=(Type value) -> InputOutputRegister& final {
                if (value != '\r') [[likely]]
                    write_callback(value);

                return *this;
            }

            constexpr operator Type() const final {
                return 0x00;
            }

            std::function<void(Type)> write_callback;
        };

        struct Registers {
            InputOutputRegister ReceiveTransmitBuffer = { this };
            GeneralPurposeRegister<std::uint8_t> IER;
            GeneralPurposeRegister<std::uint8_t> IIR;
            GeneralPurposeRegister<std::uint8_t> LCR;
            GeneralPurposeRegister<std::uint8_t> MCR;
            GeneralPurposeRegister<std::uint8_t> LSR;
            GeneralPurposeRegister<std::uint8_t> MSR;
            GeneralPurposeRegister<std::uint8_t> DLLS;
            GeneralPurposeRegister<std::uint8_t> DLMS;

            [[nodiscard]] constexpr auto DLAB() const -> bool { return LCR & util::bit<7>(); }
        };

        Registers m_registers;
    };

}