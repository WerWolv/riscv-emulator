#pragma once

#include <emu/utils.hpp>

namespace ds::emu::riscv::instr {

    namespace base {

        namespace fields {

            struct Quadrant {
                constexpr explicit Quadrant(uint32_t instruction) : quadrant(util::extract_bits<0, 1>(instruction)) {}

                uint8_t quadrant;
            };

            struct Opcode {
                constexpr explicit Opcode(uint32_t instruction) : opcode(util::extract_bits<2, 6>(instruction)) {}

                uint8_t opcode;
            };

            struct Rd {
                constexpr explicit Rd(uint32_t instruction) : rd(util::extract_bits<7, 11>(instruction)) {}

                uint8_t rd;
            };

            struct Rs1 {
                constexpr explicit Rs1(uint32_t instruction) : rs1(util::extract_bits<15, 19>(instruction)) {}

                uint8_t rs1;
            };

            struct Rs2 {
                constexpr explicit Rs2(uint32_t instruction) : rs2(util::extract_bits<20, 24>(instruction)) {}

                uint8_t rs2;
            };

            struct Rs3 {
                constexpr explicit Rs3(uint32_t instruction) : rs3(util::extract_bits<27, 31>(instruction)) {}

                uint8_t rs3;
            };

            struct Funct2 {
                constexpr explicit Funct2(uint32_t instruction) : funct2(util::extract_bits<25, 26>(instruction)) {}

                uint8_t funct2;
            };

            struct Funct3 {
                constexpr explicit Funct3(uint32_t instruction) : funct3(util::extract_bits<12, 14>(instruction)) {}

                uint8_t funct3;
            };

            struct Funct7 {
                explicit Funct7(uint32_t instruction) : funct7(util::extract_bits<25, 31>(instruction)) {}

                uint8_t funct7;
            };

            struct Bits { uint8_t from, to; };
            template<uint32_t StartBit, std::same_as<Bits> auto ... Ranges>
            struct Imm {
                explicit Imm(uint32_t instruction) : imm(extract_immediate<Ranges...>(instruction) << StartBit) {}

                uint32_t imm;

            private:
                template<std::same_as<Bits> auto First, std::same_as<Bits> auto ... Rest>
                constexpr auto extract_immediate(uint32_t instruction) -> std::uint32_t {
                    if constexpr (sizeof...(Rest) == 0) {
                        return util::extract_bits<First.from, First.to>(instruction);
                    } else {
                        constexpr static auto BitCount = (First.to - First.from) + 1;
                        return (extract_immediate<Rest...>(instruction) << BitCount) | util::extract_bits<First.from, First.to>(instruction);
                    }
                }
            };

        }

        namespace type {
            using namespace fields;

            struct R : Quadrant, Opcode, Rd, Funct3, Rs1, Rs2, Funct7 {
                constexpr explicit R(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Rd(instruction), Funct3(instruction), Rs1(instruction), Rs2(instruction), Funct7(instruction) {}
            };

            struct R4 : Quadrant, Opcode, Rd, Funct3, Rs1, Rs2, Funct2, Rs3 {
                constexpr explicit R4(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Rd(instruction), Funct3(instruction), Rs1(instruction), Rs2(instruction), Funct2(instruction), Rs3(instruction) {}
            };

            struct I : Quadrant, Opcode, Rd, Funct3, Rs1, Rs2, Imm<0, Bits{ 20,31 }> {
                constexpr explicit I(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Rd(instruction), Funct3(instruction), Rs1(instruction), Rs2(instruction), Imm(instruction) {}
            };

            struct S : Quadrant, Opcode, Funct3, Rs1, Rs2, Imm<0, Bits{7,11}, Bits{25,31}> {
                constexpr explicit S(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Funct3(instruction), Rs1(instruction), Rs2(instruction), Imm(instruction) {}
            };

            struct B : Quadrant, Opcode, Funct3, Rs1, Rs2, Imm<1, Bits{8,11}, Bits{25, 30}, Bits{7,7}, Bits{31,31}> {
                constexpr explicit B(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Funct3(instruction), Rs1(instruction), Rs2(instruction), Imm(instruction) {}
            };

            struct U : Quadrant, Opcode, Rd, Imm<12, Bits{12,31}> {
                constexpr explicit U(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Rd(instruction), Imm(instruction) {}
            };

            struct J : Quadrant, Opcode, Rd, Imm<1, Bits{21,30}, Bits{20,20}, Bits{12,19}, Bits{31,31}> {
                constexpr explicit J(uint32_t instruction) :
                    Quadrant(instruction), Opcode(instruction), Rd(instruction), Imm(instruction) {}
            };
        }

        template<typename Type_, std::uint8_t OpcodeBits>
        struct Opcode {
            constexpr static auto Value = OpcodeBits;
            using Type = Type_;
        };

        using Quadrant = Opcode<std::uint32_t, 0b11>;

        using LOAD      = Opcode<type::I,  0b00'000>;
        using STORE     = Opcode<type::S,  0b01'000>;
        using MADD      = Opcode<type::R4, 0b10'000>;
        using BRANCH    = Opcode<type::B,  0b11'000>;

        using LOAD_FP   = Opcode<type::I,  0b00'001>;
        using STORE_FP  = Opcode<type::S,  0b01'001>;
        using MSUB      = Opcode<type::R4, 0b10'001>;
        using JALR      = Opcode<type::I,  0b11'001>;

        using NMSUB     = Opcode<type::R4, 0b10'010>;

        using MISC_MEM  = Opcode<type::I,  0b00'011>;
        using AMO       = Opcode<type::R,  0b01'011>;
        using NMADD     = Opcode<type::R4, 0b10'011>;
        using JAL       = Opcode<type::J,  0b11'011>;

        using OP_IMM    = Opcode<type::I,  0b00'100>;
        using OP        = Opcode<type::R,  0b01'100>;
        using OP_FP     = Opcode<type::R,  0b10'100>;
        using SYSTEM    = Opcode<type::I,  0b11'100>;

        using AUIPC     = Opcode<type::U,  0b00'101>;
        using LUI       = Opcode<type::U,  0b01'101>;

        using OP_IMM_32 = Opcode<type::I,  0b00'110>;
        using OP_32     = Opcode<type::R,  0b01'110>;
    }



}