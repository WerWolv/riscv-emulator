#include <emu/riscv/core.hpp>
#include <emu/riscv/instructions.hpp>

#include <cstdio>

namespace ds::emu::riscv {

    auto Core::handle_system(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause> {
        const std::uint32_t old       = csr(instruction.imm);
        std::uint32_t write_val = x(instruction.rs1);

        switch (instruction.funct3) {
            case 0b000: // PRIV
                switch (instruction.imm) {
                    case 0b000000000000: { // ECALL
                        switch (m_privilege_level) {
                            case PrivilegeLevel::User:
                                stval() = 0;
                                return std::unexpected(ExceptionCause::ECallUser);
                            case PrivilegeLevel::Supervisor:
                                stval() = 0;
                                return std::unexpected(ExceptionCause::ECallSupervisor);
                            default:
                                return std::unexpected(ExceptionCause::IllegalInstruction);
                        }
                    }
                    case 0b000000000001: // EBREAK
                        return std::unexpected(ExceptionCause::Breakpoint);
                    case 0b000100100000 ... 0b000100111111: // SFENCE.VMA
                        m_address_space->invalidate();
                        return {};
                    case 0b000100000010: { // SRET
                        pc() = sepc() - 4;
                        m_address_space->invalidate();

                        const auto spp  = sstatus().get_bit(8);
                        const auto spie = sstatus().get_bit(5);

                        m_privilege_level = spp ? PrivilegeLevel::Supervisor : PrivilegeLevel::User;

                        sstatus().set_bit(1, spie);         // SIE = SPIE
                        sstatus().set_bit(8, false);   // SPP = 0
                        sstatus().set_bit(5, true);    // SPIE = 1
                        return {};
                    }
                    case 0b000100000101: { // WFI
                        m_powered_up = false;
                        return {};
                    }
                    default:
                        return std::unexpected(ExceptionCause::IllegalInstruction);
                }
            case 0b001: // CSRRW
                csr(instruction.imm) = write_val;
                x(instruction.rd) = old;
                return {};
            case 0b101: // CSRRWI
                csr(instruction.imm) = instruction.rs1;
                x(instruction.rd) = old;
                return {};
            case 0b010: // CSRRS
                if (instruction.rs1 != 0)
                    csr(instruction.imm) = old | write_val;
                x(instruction.rd) = old;
                return {};
            case 0b110: // CSRRSI
                if (instruction.rs1 != 0)
                    csr(instruction.imm) = old | instruction.rs1;
                x(instruction.rd) = old;
                return {};
            case 0b011: // CSRRC
                if (instruction.rs1 != 0)
                    csr(instruction.imm) = old & ~write_val;
                x(instruction.rd) = old;
                return {};
            case 0b111: // CSRRCI
                if (instruction.rs1 != 0)
                    csr(instruction.imm) = old & ~instruction.rs1;
                x(instruction.rd) = old;
                return {};
            default:
                return std::unexpected(ExceptionCause::UnimplementedInstruction);
        }
    }

    auto Core::handle_load(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause> {
        const auto offset = util::sign_extend<std::uint32_t, 12>(instruction.imm);
        const auto address = x(instruction.rs1) + offset;
        const bool sign_extend = util::extract_bits<2, 2>(instruction.funct3) == 0b0;
        const auto width = 1U << util::extract_bits<0, 1>(instruction.funct3);

        std::expected<std::uint32_t, ExceptionCause> value;
        switch (width) {
            case 1: // LB
                value = read<std::uint8_t>(address);
                if (!value.has_value()) [[unlikely]]
                    return std::unexpected(value.error());
                if (sign_extend)
                    value = util::sign_extend<std::uint32_t, 8>(*value);
                break;
            case 2: // LH
                value = read<std::uint16_t>(address);
                if (!value.has_value()) [[unlikely]]
                    return std::unexpected(value.error());
                if (sign_extend)
                    value = util::sign_extend<std::uint32_t, 16>(*value);
                break;
            case 4: // LW
                value = read<std::uint32_t>(address);
                if (!value.has_value()) [[unlikely]]
                    return std::unexpected(value.error());
                if (!sign_extend) [[unlikely]]
                    return std::unexpected(ExceptionCause::IllegalInstruction);
                break;
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }

        x(instruction.rd) = *value;
        return {};
    }

    auto Core::handle_store(const instr::base::type::S &instruction) -> std::expected<void, ExceptionCause> {
        const auto offset = util::sign_extend<std::uint32_t, 12>(instruction.imm);
        const std::uint32_t base = x(instruction.rs1);
        const auto width = 1U << util::extract_bits<0, 1>(instruction.funct3);

        switch (width) {
            case 1: // SB
                if (auto result = write<std::uint8_t>(base + offset, x(instruction.rs2)); !result.has_value())
                    return std::unexpected(result.error());
                break;
            case 2: // SH
                if (auto result = write<std::uint16_t>(base + offset, x(instruction.rs2)); !result.has_value())
                    return std::unexpected(result.error());
                break;
            case 4: // SW
                if (auto result = write<std::uint32_t>(base + offset, x(instruction.rs2)); !result.has_value())
                    return std::unexpected(result.error());
                break;
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }

        return {};
    }

    auto Core::handle_lui(const instr::base::type::U &instruction) -> std::expected<void, ExceptionCause> {
        x(instruction.rd) = instruction.imm;
        return {};
    }

    auto Core::handle_auipc(const instr::base::type::U &instruction) -> std::expected<void, ExceptionCause> {
        x(instruction.rd) = instruction.imm + pc();
        return {};
    }

    auto Core::handle_jal(const instr::base::type::J &instruction) -> std::expected<void, ExceptionCause> {
        const auto offset = util::sign_extend<std::uint32_t, 21>(instruction.imm);
        const auto destination = pc() + offset;

        x(instruction.rd) = pc() + 4;
        pc() = destination - 4;

        return {};
    }

    auto Core::handle_jalr(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause> {
        const auto offset = util::sign_extend<std::uint32_t, 12>(instruction.imm);
        const auto destination = (x(instruction.rs1) + offset) & ~0x0000'0001;

        x(instruction.rd) = pc() + 4;
        pc() = destination - 4;

        return {};
    }

    auto Core::handle_op_imm(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause> {
        const bool alternative = (instruction.imm >> 5) == 0b010'0000;
        const auto shamt = instruction.imm & 0b11111;
        switch (instruction.funct3) {
            case 0b000: { // ADDI
                x(instruction.rd) =
                    x(instruction.rs1) +
                    util::sign_extend<std::uint32_t, 12>(instruction.imm);
                return {};
            }
            case 0b111: { // ANDI
                x(instruction.rd) =
                    x(instruction.rs1) &
                    util::sign_extend<std::uint32_t, 12>(instruction.imm);
                return {};
            }
            case 0b110: { // ORI
                x(instruction.rd) =
                    x(instruction.rs1) |
                    util::sign_extend<std::uint32_t, 12>(instruction.imm);
                return {};
            }
            case 0b100: { // XORI
                x(instruction.rd) =
                    x(instruction.rs1) ^
                    util::sign_extend<std::uint32_t, 12>(instruction.imm);
                return {};
            }
            case 0b001: { // SLLI
                x(instruction.rd) =
                    x(instruction.rs1) <<
                    shamt;
                return {};
            }
            case 0b010: { // SLTI
                x(instruction.rd) =
                    static_cast<std::int32_t>(x(instruction.rs1)) <
                    util::sign_extend<std::uint32_t, 12>(instruction.imm);
                return {};
            }
            case 0b011: { // SLTIU
                x(instruction.rd) =
                    x(instruction.rs1) <
                    instruction.imm;
                return {};
            }
            case 0b101: { // SRLI / SRAI
                if (!alternative) {
                    x(instruction.rd) =
                        x(instruction.rs1) >>
                        shamt;
                } else {
                    x(instruction.rd) =
                        static_cast<std::int32_t>(x(instruction.rs1)) >>
                        shamt;
                }
                return {};
            }
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }
    }

    auto Core::handle_op(const instr::base::type::R &instruction) -> std::expected<void, ExceptionCause> {
        switch (instruction.funct7) {
            case 0b000'0000: {
                switch (instruction.funct3) {
                    case 0b000: // ADD
                        x(instruction.rd) =
                           x(instruction.rs1) +
                           x(instruction.rs2);
                        return {};
                    case 0b001: // SLL
                        x(instruction.rd) =
                            x(instruction.rs1) <<
                            x(instruction.rs2);
                        return {};
                    case 0b101: // SRL
                        x(instruction.rd) =
                            x(instruction.rs1) >>
                            x(instruction.rs2);
                        return {};
                    case 0b010: // SLT
                        x(instruction.rd) =
                            static_cast<std::int32_t>(x(instruction.rs1)) <
                            static_cast<std::int32_t>(x(instruction.rs2));
                        return {};
                    case 0b011: // SLTU
                        x(instruction.rd) =
                            x(instruction.rs1) <
                            x(instruction.rs2);
                        return {};
                    case 0b110: // OR
                        x(instruction.rd) =
                           x(instruction.rs1) |
                           x(instruction.rs2);
                        return {};
                    case 0b111: // AND
                        x(instruction.rd) =
                           x(instruction.rs1) &
                           x(instruction.rs2);
                        return {};
                    case 0b100: // XOR
                        x(instruction.rd) =
                           x(instruction.rs1) ^
                           x(instruction.rs2);
                        return {};
                    default:
                        return std::unexpected(ExceptionCause::IllegalInstruction);
                }
            }
            case 0b000'0001: { // MULDIV
                switch (instruction.funct3) {
                    case 0b000: { // MUL
                        const std::int64_t left  = static_cast<std::int32_t>(x(instruction.rs1));
                        const std::int64_t right = static_cast<std::int32_t>(x(instruction.rs2));
                        x(instruction.rd) = static_cast<std::uint64_t>(left * right) & util::mask<32>();
                        return {};
                    }
                    case 0b001: { // MULH
                        const std::int64_t left  = static_cast<std::int32_t>(x(instruction.rs1));
                        const std::int64_t right = static_cast<std::int32_t>(x(instruction.rs2));
                        x(instruction.rd) = static_cast<std::uint64_t>(left * right) >> 32;
                        return {};
                    }
                    case 0b010: { // MULHSU
                        const std::int64_t  left  = static_cast<std::int32_t>(x(instruction.rs1));
                        const std::uint64_t right = static_cast<std::uint32_t>(x(instruction.rs2));
                        x(instruction.rd) = static_cast<std::uint64_t>(left * right) >> 32;
                        return {};
                    }
                    case 0b011: { // MULHU
                        const std::uint64_t left  = static_cast<std::uint32_t>(x(instruction.rs1));
                        const std::uint64_t right = static_cast<std::uint32_t>(x(instruction.rs2));
                        x(instruction.rd) = static_cast<std::uint64_t>(left * right) >> 32;
                        return {};
                    }
                    case 0b100: { // DIV
                        const std::uint32_t left  = x(instruction.rs1);
                        const std::uint32_t right = x(instruction.rs2);

                        if (right == 0) {
                            x(instruction.rd) = std::numeric_limits<std::uint32_t>::max();
                        } else if (left == 0x8000'0000 and static_cast<std::int32_t>(right) == -1) {
                            x(instruction.rd) = 0x80000000;
                        } else {
                            x(instruction.rd) = static_cast<std::int32_t>(left) / static_cast<std::int32_t>(right);
                        }

                        return {};
                    }
                    case 0b101: { // DIVU
                        const std::uint32_t left  = x(instruction.rs1);
                        const std::uint32_t right = x(instruction.rs2);

                        if (right == 0) {
                            x(instruction.rd) = std::numeric_limits<std::uint32_t>::max();
                        } else {
                            x(instruction.rd) = left / right;
                        }

                        return {};
                    }
                    case 0b110: { // REM
                        const std::uint32_t left  = x(instruction.rs1);
                        const std::uint32_t right = x(instruction.rs2);

                        if (right == 0) {
                            x(instruction.rd) = left;
                        } else if (left == 0x8000'0000 and static_cast<std::int32_t>(right) == -1) {
                            x(instruction.rd) = 0;
                        } else {
                            x(instruction.rd) = static_cast<std::int32_t>(left) % static_cast<std::int32_t>(right);
                        }

                        return {};
                    }
                    case 0b111: { // REMU
                        const std::uint32_t left  = x(instruction.rs1);
                        const std::uint32_t right = x(instruction.rs2);

                        if (right == 0) {
                            x(instruction.rd) = left;
                        } else {
                            x(instruction.rd) = left % right;
                        }

                        return {};
                    }
                    default:
                        return std::unexpected(ExceptionCause::IllegalInstruction);
                }
            }
            case 0b010'0000: {
                switch (instruction.funct3) {
                    case 0b000: // SUB
                        x(instruction.rd) =
                           x(instruction.rs1) -
                           x(instruction.rs2);
                        return {};
                    case 0b101: // SRA
                        x(instruction.rd) =
                           static_cast<std::int32_t>(x(instruction.rs1)) >>
                           x(instruction.rs2);
                        return {};
                    default:
                        return std::unexpected(ExceptionCause::IllegalInstruction);
                }
            }
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }
    }

    auto Core::handle_branch(const instr::base::type::B &instruction) -> std::expected<void, ExceptionCause> {
        const auto branch_address = pc() + util::sign_extend<std::uint32_t, 13>(instruction.imm) - 4;
        const bool unsigned_compare = util::extract_bits<1, 1>(instruction.funct3) == 0b1;
        switch (instruction.funct3 & 0b101) {
            case 0b000: // BEQ
                if (x(instruction.rs1) == x(instruction.rs2))
                    pc() = branch_address;
                return {};
            case 0b001: // BNE
                if (x(instruction.rs1) != x(instruction.rs2))
                    pc() = branch_address;
                return {};
            case 0b100: // BLT / BLTU
                if (unsigned_compare) {
                    if (x(instruction.rs1) < x(instruction.rs2))
                        pc() = branch_address;
                } else {
                    if (static_cast<std::int32_t>(x(instruction.rs1)) < static_cast<std::int32_t>(x(instruction.rs2)))
                        pc() = branch_address;
                }

                return {};
            case 0b101: // BGE / BGEU
                if (unsigned_compare) {
                    if (x(instruction.rs1) >= x(instruction.rs2))
                        pc() = branch_address;
                } else {
                    if (static_cast<std::int32_t>(x(instruction.rs1)) >= static_cast<std::int32_t>(x(instruction.rs2)))
                        pc() = branch_address;
                }

                return {};
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }
    }

    auto Core::handle_misc_mem(const instr::base::type::I &instruction) -> std::expected<void, ExceptionCause> {
        switch (instruction.funct3) {
            case 0b000: // FENCE
            case 0b001: // FENCE.I
                // Nothing to do here
                return {};
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }
    }

    auto Core::handle_amo(const instr::base::type::R &instruction) -> std::expected<void, ExceptionCause> {
        switch (instruction.funct3) {
            case 0b010: { // RV32A
                const auto rl    = util::extract_bits<0, 0>(instruction.funct7);
                const auto aq    = util::extract_bits<1, 1>(instruction.funct7);
                const auto funct5 = util::extract_bits<2, 5>(instruction.funct7);

                std::ignore = rl;
                std::ignore = aq;

                const std::uint32_t address = x(instruction.rs1);
                const std::uint32_t value   = x(instruction.rs2);
                switch (funct5) {
                    case 0b00010: { // LR.W
                        if (address % 4 != 0)
                            return std::unexpected(ExceptionCause::LoadMisalign);

                        const auto result = read<std::uint32_t>(address);
                        if (!result.has_value())
                            return std::unexpected(result.error());

                        const auto physical_address = m_address_space->translate_address(*this, address, AccessType::Load);
                        if (!physical_address.has_value()) {
                            switch (physical_address.error()) {
                                using enum AccessResult;
                                default:
                                case LoadAccessFault: return std::unexpected(ExceptionCause::LoadFault);
                                case LoadPageFault: return std::unexpected(ExceptionCause::LoadPageFault);
                            }
                        }

                        this->m_lr_reservation = *physical_address | 0b1;
                        x(instruction.rd) = *result;

                        return {};
                    }
                    case 0b00011: { // SC.W
                        if (address % 4 != 0)
                            return std::unexpected(ExceptionCause::StoreMisalign);

                        x(instruction.rd) = 1;

                        const auto physical_address = m_address_space->translate_address(*this, address, AccessType::Store);
                        if (!physical_address.has_value()) {
                            switch (physical_address.error()) {
                                using enum AccessResult;
                                default:
                                case LoadAccessFault: return std::unexpected(ExceptionCause::StoreFault);
                                case LoadPageFault: return std::unexpected(ExceptionCause::StorePageFault);
                            }
                        }

                        if (m_lr_reservation != (*physical_address | 0b1))
                            return {};

                        const auto result = write<std::uint32_t>(address, value);
                        if (!result.has_value())
                            return std::unexpected(result.error());

                        // TODO: Needs to be cleared on all harts that match the address
                        if ((m_lr_reservation & 0b1) and (m_lr_reservation & ~0b11) == (*physical_address & ~0b11))
                            m_lr_reservation = 0;
                        x(instruction.rd) = 0;

                        return {};
                    }
                    case 0b00001: { // AMOSWAP.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        if (const auto write_result = write<std::uint32_t>(address, value); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        x(instruction.rd) = *read_result;
                        return {};
                    }
                    case 0b00000: { // AMOADD.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, *read_result +value); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b00100: { // AMOXOR.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, *read_result ^ value); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b01100: { // AMOAND.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, *read_result & value); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b01000: { // AMOOR.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, *read_result | value); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b10000: { // AMOMIN.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, std::min<std::int32_t>(*read_result, value)); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b10100: { // AMOMAX.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, std::max<std::int32_t>(*read_result, value)); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b11000: { // AMOMINU.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, std::min<std::uint32_t>(*read_result, value)); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    case 0b11100: { // AMOMAXU.W
                        const auto read_result = read<std::uint32_t>(address);
                        if (!read_result.has_value())
                            return std::unexpected(read_result.error());

                        x(instruction.rd) = *read_result;
                        if (const auto write_result = write<std::uint32_t>(address, std::max<std::uint32_t>(*read_result, value)); !write_result.has_value())
                            return std::unexpected(write_result.error());

                        return {};
                    }
                    default:
                        return std::unexpected(ExceptionCause::IllegalInstruction);
                }
            }
            default:
                return std::unexpected(ExceptionCause::IllegalInstruction);
        }
    }

    auto Core::handle_unimplemented(std::uint32_t instruction) -> std::expected<void, ExceptionCause> {
        std::ignore = instruction;
        return std::unexpected(ExceptionCause::UnimplementedInstruction);
    }

    auto Core::handle_std_instructions(std::uint32_t instruction) -> std::expected<void, ExceptionCause> {
        constexpr static auto Instructions = jumpTable<2, 6,
            Entry<instr::base::LOAD,        &Core::handle_load>,
            Entry<instr::base::STORE,       &Core::handle_store>,
            Entry<instr::base::MADD,        &Core::handle_unimplemented>,
            Entry<instr::base::BRANCH,      &Core::handle_branch>,
            Entry<instr::base::LOAD_FP,     &Core::handle_unimplemented>,
            Entry<instr::base::STORE_FP,    &Core::handle_unimplemented>,
            Entry<instr::base::MSUB,        &Core::handle_unimplemented>,
            Entry<instr::base::JALR,        &Core::handle_jalr>,
            Entry<instr::base::NMSUB,       &Core::handle_unimplemented>,
            Entry<instr::base::MISC_MEM,    &Core::handle_misc_mem>,
            Entry<instr::base::AMO,         &Core::handle_amo>,
            Entry<instr::base::NMADD,       &Core::handle_unimplemented>,
            Entry<instr::base::JAL,         &Core::handle_jal>,
            Entry<instr::base::OP_IMM,      &Core::handle_op_imm>,
            Entry<instr::base::OP,          &Core::handle_op>,
            Entry<instr::base::OP_FP,       &Core::handle_unimplemented>,
            Entry<instr::base::SYSTEM,      &Core::handle_system>,
            Entry<instr::base::AUIPC,       &Core::handle_auipc>,
            Entry<instr::base::LUI,         &Core::handle_lui>,
            Entry<instr::base::OP_IMM_32,   &Core::handle_unimplemented>,
            Entry<instr::base::OP_32,       &Core::handle_unimplemented>
        >();

        const auto result = Instructions(this, instruction);

        pc() += 4;

        return result;
    }

    constexpr auto highest_priority_supervisor_interrupt(uint64_t pending_mask) -> std::optional<std::uint32_t> {
        constexpr uint64_t SSIP = util::bit<1>();
        constexpr uint64_t STIP = util::bit<5>();
        constexpr uint64_t SEIP = util::bit<9>();

        if (pending_mask & SEIP)
            return 9; // supervisor external interrupt
        if (pending_mask & STIP)
            return 5; // supervisor timer interrupt
        if (pending_mask & SSIP)
            return 1; // supervisor software interrupt
        return -1;
    }

    auto Core::handle_interrupts() -> void {
        const auto pending = sip() & sie();
        if (!pending) [[likely]]
            return;

        const auto delegated = pending & mideleg(); // interrupts delegated to Supervisor by Machine
        const auto not_delegated = pending & ~mideleg();

        m_powered_up = true;

        if (delegated) {
            // Check S-mode global interrupt enable (sstatus.SIE)
            if (!sstatus().get_bit(1)) {
                // Supervisor interrupts are globally disabled; do nothing.
                return;
            }

            // Choose highest-priority supervisor interrupt
            const auto cause_num = highest_priority_supervisor_interrupt(delegated);
            if (!cause_num.has_value())
                return;

            const auto interrupt_index = std::countr_zero(pending);

            // Set interrupt bit
            scause() = util::bit<31>() | interrupt_index;
            stval() = 0;
            trap(ExceptionCause::Interrupt);

            return;
        }

        if (not_delegated) {
            std::printf("Non-delegated interrupt occurred but no Machine-mode present\n");
        }
    }

    auto Core::handle_exception(std::uint32_t start_pc, ExceptionCause cause) -> bool {
        scause() = static_cast<std::uint32_t>(cause);
        switch (cause) {
            using enum ExceptionCause;
        case ECallSupervisor: // ECALL from Supervisor mode, delegate it to machine mode
            set_privilege_level(PrivilegeLevel::Machine);
            return false;
        case ECallUser: // ECALL from User mode, jump to supervisor
            pc() = start_pc;

            break;
        case UnimplementedInstruction: // Treat unimplemented instructions the same as illegal instructions
            scause() = static_cast<std::uint32_t>(IllegalInstruction);
            break;
        case Breakpoint:
            scause() = 0;
            break;
        default:
            pc() = start_pc;
            break;
        }

        return true;
    }

    auto Core::trap(ExceptionCause cause) -> void {
        switch (cause) {
            using enum ExceptionCause;

            case IllegalInstruction:
            case Breakpoint:
            case ECallSupervisor:
            case ECallUser:
                stval() = pc();
                break;
            default:
                break;
        }

        // Set SSTATUS.SPIE to SSTATUS.SIE
        sstatus().set_bit(5, sstatus().get_bit(1));

        // Set SSTATUS.SPP to the current privilege level
        sstatus().set_bit(8, m_privilege_level == PrivilegeLevel::Supervisor);

        // Set SEPC to the value of PC where the exception happened
        sepc() = pc();

        // Disable interrupts
        sstatus().set_bit(1, false);

        // Invalidate MMU
        m_address_space->invalidate();

        // Enter supervisor mode
        m_privilege_level = PrivilegeLevel::Supervisor;

        // Jump to the supervisor interrupt vector address
        {
            const auto mode = stvec() & 0b11;
            const auto base = stvec() & ~0b11;

            pc() = base;
            if (mode == 0b01) {
                pc() += (scause() & util::mask<31>()) * 4;
            }
        }
    }

    auto Core::step() -> std::expected<void, ExceptionCause> {
        const std::uint32_t start_pc = pc();
        constexpr static auto Instructions = jumpTable<0, 1,
            Entry<instr::base::Quadrant, &Core::handle_std_instructions>
        >();

        handle_interrupts();

        if (!m_powered_up)
            return {};

        std::expected<void, ExceptionCause> result;
        const auto instruction = fetch<std::uint32_t>(pc());
        if (instruction.has_value()) [[likely]] {
            result = Instructions(this, *instruction);
        } else {
            result = std::unexpected(instruction.error());
        }

        if (!result.has_value()) {
            const auto exception = result.error();

            if (const auto needs_trap = handle_exception(start_pc, exception); needs_trap) {
                trap(exception);
            }
        }

        return result;
    }

}
