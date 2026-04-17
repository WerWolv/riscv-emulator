#pragma once

#include <cstring>
#include <expected>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

#include <emu/core.hpp>
#include <emu/address_space.hpp>
#include <emu/register.hpp>
#include <emu/riscv/instructions.hpp>
#include <emu/utils.hpp>

namespace ds::emu::riscv {

    enum class ExceptionCause {
        PCMisalign          = 0,
        FetchFault          = 1,
        IllegalInstruction  = 2,
        Breakpoint          = 3,
        LoadMisalign        = 4,
        LoadFault           = 5,
        StoreMisalign       = 6,
        StoreFault          = 7,
        ECallUser           = 8,
        ECallSupervisor     = 9,
        FetchPageFault      = 12,
        LoadPageFault       = 13,
        StorePageFault      = 15,

        // Custom Exception codes
        UnimplementedInstruction    = 16,
        Interrupt                   = 17,
        CoreStopped                 = 18
    };

    constexpr static auto get_exception_string(ExceptionCause cause) {
        switch (cause) {
            using enum ExceptionCause;
            case PCMisalign:                return "Instruction address misaligned";
            case FetchFault:                return "Instruction access fault";
            case IllegalInstruction:        return "Illegal instruction";
            case Breakpoint:                return "Breakpoint";
            case LoadMisalign:              return "Load address misaligned";
            case LoadFault:                 return "Load access fault";
            case StoreMisalign:             return "Store/AMO address misaligned";
            case StoreFault:                return "Store/AMO access fault";
            case ECallUser:                 return "Environment call from U-mode";
            case ECallSupervisor:           return "Environment call from S-mode";
            case FetchPageFault:            return "Instruction page fault";
            case LoadPageFault:             return "Load page fault";
            case StorePageFault:            return "Store/AMO page fault";
            case UnimplementedInstruction:  return "Instruction unimplemented";
            case CoreStopped:               return "Core stopped";
            case Interrupt:                 return "Interrupt";
        }

        return "";
    }

    enum class PrivilegeLevel {
        User,
        Supervisor,
        Hypervisor,
        Machine
    };

    using Register = RegisterBase<std::uint32_t>;

    class Core : public emu::Core {
    public:
        Core() = default;
        Core(std::uint16_t hart, AddressSpace<std::uint32_t> *address_space)
            : m_hart(hart), m_address_space(address_space) {
            reset();
        }

        Core(const Core &other) = delete;
        Core(Core &&other) = default;

        Core &operator=(const Core &other) = delete;
        Core &operator=(Core &&other) = default;

        std::expected<void, ExceptionCause> step();

        [[nodiscard]] constexpr auto privilege_level() const -> PrivilegeLevel {
            return m_privilege_level;
        }

        constexpr void set_privilege_level(PrivilegeLevel privilege_level) {
            m_privilege_level = privilege_level;
        }

        constexpr auto x(std::uint8_t number) -> Register& {
            if (number == 0) {
                return m_zeroRegister;
            } else if (number <= 31) {
                return m_registers[number - 1];
            } else {
                std::unreachable();
            }
        }

        constexpr auto csr(std::uint16_t number) -> Register& {
            return m_csrs[number];
        }

        auto hart_id() -> std::uint16_t {
            return m_hart;
        }

        auto pc()   -> auto& { return m_program_counter; }
        auto zero() -> auto& { return x(0);  }
        auto ra()   -> auto& { return x(1);  }
        auto sp()   -> auto& { return x(2);  }
        auto gp()   -> auto& { return x(3);  }
        auto tp()   -> auto& { return x(4);  }
        auto t0()   -> auto& { return x(5);  }
        auto t1()   -> auto& { return x(6);  }
        auto t2()   -> auto& { return x(7);  }
        auto s0()   -> auto& { return x(8);  }
        auto fp()   -> auto& { return x(8);  }
        auto s1()   -> auto& { return x(9);  }
        auto a0()   -> auto& { return x(10); }
        auto a1()   -> auto& { return x(11); }
        auto a2()   -> auto& { return x(12); }
        auto a3()   -> auto& { return x(13); }
        auto a4()   -> auto& { return x(14); }
        auto a5()   -> auto& { return x(15); }
        auto a6()   -> auto& { return x(16); }
        auto a7()   -> auto& { return x(17); }
        auto s2()   -> auto& { return x(18); }
        auto s3()   -> auto& { return x(19); }
        auto s4()   -> auto& { return x(20); }
        auto s5()   -> auto& { return x(21); }
        auto s6()   -> auto& { return x(22); }
        auto s7()   -> auto& { return x(23); }
        auto s8()   -> auto& { return x(24); }
        auto s9()   -> auto& { return x(25); }
        auto s10()  -> auto& { return x(26); }
        auto s11()  -> auto& { return x(27); }
        auto t3()   -> auto& { return x(28); }
        auto t4()   -> auto& { return x(29); }
        auto t5()   -> auto& { return x(30); }
        auto t6()   -> auto& { return x(31); }

        auto sstatus()      -> auto& { return csr(0x100); }
        auto sie()          -> auto& { return csr(0x104); }
        auto stvec()        -> auto& { return csr(0x105); }
        auto scounteren()   -> auto& { return csr(0x106); }

        auto sscratch()     -> auto& { return csr(0x140); }
        auto sepc()         -> auto& { return csr(0x141); }
        auto scause()       -> auto& { return csr(0x142); }
        auto stval()        -> auto& { return csr(0x143); }
        auto sip()          -> auto& { return csr(0x144); }

        auto satp()         -> auto& { return csr(0x180); }

        auto mip()          -> auto& { return csr(0x344); }
        auto mie()          -> auto& { return csr(0x304); }

        auto cycle()        -> auto& { return csr(0xC00); }
        auto time()         -> auto& { return csr(0xC01); }

        auto cycleh()       -> auto& { return csr(0xC80); }
        auto timeh()        -> auto& { return csr(0xC81); }

        auto mideleg()      -> auto& { return csr(0x303); }


        auto reset() -> void {
            m_registers    = {};
            m_csrs         = {};
            m_program_counter = 0x0000'0000;
            m_powered_up = true;
            a0() = m_hart;

            mideleg() = 0xFFFF'FFFF;
        }

        [[nodiscard]] constexpr auto address_space() const -> AddressSpace<std::uint32_t>& {
            return *m_address_space;
        }

        template<typename T>
        auto read(std::uint32_t address) -> std::expected<T, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::LoadMisalign);
            }
            T data;
            const auto result = m_address_space->read(*this, address, util::to_byte_span(data));
            switch (result) {
                using enum AccessResult;
                case Success: return data;

                default:
                case LoadAccessFault: stval() = address; return std::unexpected(ExceptionCause::LoadFault);
                case LoadPageFault:   stval() = address; return std::unexpected(ExceptionCause::LoadPageFault);
            }
        }

        template<typename T>
        auto read_physical(std::uint32_t address) -> std::expected<T, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::LoadMisalign);
            }
            T data;
            const auto result = m_address_space->read_physical(address, util::to_byte_span(data));
            switch (result) {
                using enum AccessResult;
                case Success: return data;

                default:
                case LoadAccessFault: stval() = address; return std::unexpected(ExceptionCause::LoadFault);
                case LoadPageFault:   stval() = address; return std::unexpected(ExceptionCause::LoadPageFault);
            }
        }

        template<typename T>
        auto fetch(std::uint32_t address) -> std::expected<T, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::PCMisalign);
            }

            T data;
            const auto result = m_address_space->read(*this, address, util::to_byte_span(data));
            switch (result) {
                using enum AccessResult;
                case Success: return data;

                default:
                case LoadAccessFault: stval() = address; return std::unexpected(ExceptionCause::FetchFault);
                case LoadPageFault:   stval() = address; return std::unexpected(ExceptionCause::FetchPageFault);
            }
        }

        template<typename T>
        auto fetch_physical(std::uint32_t address) -> std::expected<T, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::PCMisalign);
            }

            T data;
            const auto result = m_address_space->read_physical(address, util::to_byte_span(data));
            switch (result) {
                using enum AccessResult;
                case Success: return data;

                default:
                case LoadAccessFault: stval() = address; return std::unexpected(ExceptionCause::FetchFault);
                case LoadPageFault:   stval() = address; return std::unexpected(ExceptionCause::FetchPageFault);
            }
        }

        template<typename T>
        auto write(std::uint32_t address, T value) -> std::expected<void, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::StoreMisalign);
            }

            const auto result = m_address_space->write(*this, address, util::to_byte_span(value));
            switch (result) {
                using enum AccessResult;
                case Success: return {};

                default:
                case StoreAccessFault: stval() = address; return std::unexpected(ExceptionCause::StoreFault);
                case StorePageFault:   stval() = address; return std::unexpected(ExceptionCause::StorePageFault);
            }
        }

        template<typename T>
        auto write_physical(std::uint32_t address, T value) -> std::expected<void, ExceptionCause> {
            if (address % alignof(T) != 0) [[unlikely]] {
                stval() = address;
                return std::unexpected(ExceptionCause::StoreMisalign);
            }

            const auto result = m_address_space->write_physical(address, util::to_byte_span(value));
            switch (result) {
                using enum AccessResult;
                case Success: return {};

                default:
                case StoreAccessFault: stval() = address; return std::unexpected(ExceptionCause::StoreFault);
                case StorePageFault:   stval() = address; return std::unexpected(ExceptionCause::StorePageFault);
            }
        }

    private:
        auto handle_std_instructions(std::uint32_t instruction) -> std::expected<void, ExceptionCause>;

        auto handle_unimplemented(std::uint32_t instruction) -> std::expected<void, ExceptionCause>;
        auto handle_system(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_jal(const instr::base::type::J &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_jalr(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_load(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_store(const instr::base::type::S &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_lui(const instr::base::type::U &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_auipc(const instr::base::type::U &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_op_imm(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_op(const instr::base::type::R &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_branch(const instr::base::type::B &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_misc_mem(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause>;
        auto handle_amo(const instr::base::type::R &instruction) -> std::expected<void, ExceptionCause>;

        auto handle_interrupts() -> void;
        auto handle_exception(std::uint32_t start_pc, ExceptionCause cause) -> bool;
        auto trap(ExceptionCause cause) -> void;

    private:
        using HandlerFunction = std::expected<void, ExceptionCause>(Core::*)(std::uint32_t instruction);

        template<typename Instr, auto HandlerFunction>
        struct Entry {
            using Instruction = Instr;
            constexpr static auto Handler = HandlerFunction;
        };

        template<typename Entry>
        auto decode_instruction(std::uint32_t instruction) -> std::expected<void, ExceptionCause> {
            if constexpr (requires { (this->*Entry::Handler)(typename Entry::Instruction::Type(instruction)); }) {
                return (this->*Entry::Handler)(typename Entry::Instruction::Type(instruction));
            } else {
                return (this->*Entry::Handler)(instruction);
            }
        }

        template<typename First, typename ... Rest>
        constexpr static auto jumpTableImpl(auto &table) -> void {
            table[First::Instruction::Value] = &Core::decode_instruction<First>;
            if constexpr (sizeof...(Rest) > 0) {
                return jumpTableImpl<Rest...>(table);
            }
        }

        template<std::size_t From, std::size_t To, typename ... Entries>
        constexpr static auto jumpTable() -> auto {
            constexpr static auto NumBits = (To - From) + 1;
            std::array<HandlerFunction, 1 << NumBits> table = {};
            for (auto &handler : table) {
                handler = &Core::handle_unimplemented;
            }
            jumpTableImpl<Entries...>(table);

            return [table](Core *emulator, std::uint32_t instruction) {
                const auto index = (instruction & (util::mask<NumBits>() << From)) >> From;
                const HandlerFunction handler = table[index];

                return (emulator->*handler)(instruction);
            };
        }

    private:
        bool m_powered_up = true;
        std::uint16_t m_hart = 0;
        AddressSpace<std::uint32_t> *m_address_space = nullptr;

        ZeroRegister<std::uint32_t> m_zeroRegister;
        std::array<GeneralPurposeRegister<std::uint32_t>, 31> m_registers = {};
        GeneralPurposeRegister<std::uint32_t> m_program_counter = {};
        std::uint32_t m_lr_reservation = 0x00;

        std::array<GeneralPurposeRegister<std::uint32_t>, 4096> m_csrs;
        PrivilegeLevel m_privilege_level = PrivilegeLevel::Supervisor;
    };

}