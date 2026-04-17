#pragma once

#include <emu/address_space.hpp>
#include <emu/riscv/core.hpp>
#include <emu/literals.hpp>
#include <map>

namespace ds::emu::dev::riscv {

    using namespace literals;

    template<typename T>
    class MMU : public AddressTranslator<T> {
    public:
        constexpr static auto PageSize = 4_KiB;
        constexpr static uint32_t PteSize = 4;

        constexpr auto translate(Core &core, T virtual_address, AccessType access) -> std::expected<T, AccessResult> final {
            auto &r = static_cast<emu::riscv::Core &>(core);

            // Check if MMU is enabled
            const auto mode = r.satp().get_bit(31);
            if (mode == 0)
                return virtual_address;

            // Sv32: root PPN is satp.PPN[21:0]
            const auto root_ppn = util::extract_bits<0,21>(static_cast<uint32_t>(r.satp()));
            const T root_page_table = root_ppn * PageSize;

            const auto vpn0 = util::extract_bits<12,21>(virtual_address);
            const auto vpn1 = util::extract_bits<22,31>(virtual_address);

            const auto virtual_page_address = virtual_address & ~(PageSize - 1);
            const auto offset = virtual_address & (PageSize - 1);
            if (auto it = m_tlb.find(virtual_page_address); it != m_tlb.end()) [[likely]] {
                // TLB hit
                const T physical_page_address = it->second;
                return physical_page_address | offset;
            } else {
                // TLB miss
                auto physical_address = get_physical_address(r, virtual_address, { vpn0, vpn1 }, root_page_table, 1, access);
                if (physical_address.has_value()) [[likely]]
                    m_tlb.emplace(virtual_page_address, physical_address.value() & ~(PageSize - 1));
                return physical_address;
            }
        }

        constexpr auto get_physical_address(emu::riscv::Core &core, T va,
                                           std::array<T,2> vpns, T page_table_addr,
                                           uint8_t level, AccessType access) -> std::expected<T, AccessResult> {
            const auto index = vpns[level];
            const auto entry_addr = page_table_addr + index * PteSize;

            std::uint32_t page_table_entry = 0;
            if (core.address_space().read_physical(entry_addr, util::to_byte_span(page_table_entry)) != AccessResult::Success)
                return std::unexpected(AccessResult::LoadPageFault);

            constexpr static auto V = 1u << 0;
            constexpr static auto R = 1u << 1;
            constexpr static auto W = 1u << 2;
            constexpr static auto X = 1u << 3;
            constexpr static auto U = 1u << 4;
            constexpr static auto G = 1u << 5;
            constexpr static auto A = 1u << 6;
            constexpr static auto D = 1u << 7;

            // Check if page table entry is valid
            if (!(page_table_entry & V)) {
                return std::unexpected(access == AccessType::Store ? AccessResult::StorePageFault :
                                       access == AccessType::Instruction ? AccessResult::FetchPageFault :
                                       AccessResult::LoadPageFault);
            }

            // Non-leaf: V=1 and R=W=X=0
            if ((page_table_entry & (R|W|X)) == 0) {
                // next-level base = PPN (bits 10..31) << 12
                const uint32_t ppn = util::extract_bits<10,31>(page_table_entry);
                const T next_base = static_cast<T>(ppn) * PageSize;
                if (level == 0) {
                    // shouldn't happen: level 0 non-leaf is invalid in Sv32
                    return std::unexpected(AccessResult::LoadPageFault);
                }
                return get_physical_address(core, va, vpns, next_base, level - 1, access);
            }

            // Leaf PTE. Check permissions based on access type and current privilege.
            const bool is_user_access = (core.privilege_level() == emu::riscv::PrivilegeLevel::User);
            const bool pte_user = page_table_entry & U;

            // If user access and PTE U==0 -> fault
            if (is_user_access && !pte_user) {
                return std::unexpected(access == AccessType::Store ? AccessResult::StorePageFault :
                                       access == AccessType::Instruction ? AccessResult::FetchPageFault :
                                       AccessResult::LoadPageFault);
            }

            // If supervisor access and pte_user==1 and SUM==0 -> fault (unless SUM handling allows)
            if (!is_user_access && pte_user && !core.sstatus().get_bit(18)) {
                // access to user page from supervisor without SUM
                return std::unexpected(access == AccessType::Store ? AccessResult::StorePageFault :
                                       access == AccessType::Instruction ? AccessResult::FetchPageFault :
                                       AccessResult::LoadPageFault);
            }

            // W implies R: a PTE with W=1 and R=0 is illegal as a leaf.
            if ((page_table_entry & W) && !(page_table_entry & R)) {
                return std::unexpected(access == AccessType::Store ? AccessResult::StorePageFault :
                                       access == AccessType::Instruction ? AccessResult::FetchPageFault :
                                       AccessResult::LoadPageFault);
            }

            // Check permissions
            if (access == AccessType::Instruction) {
                if (!(page_table_entry & X))
                    return std::unexpected(AccessResult::FetchPageFault);
            } else if (access == AccessType::Load) {
                if (!(page_table_entry & R))
                    return std::unexpected(AccessResult::LoadPageFault);
            } else { // Store
                if (!(page_table_entry & W))
                    return std::unexpected(AccessResult::StorePageFault);
            }

            // Set A bit if needed
            bool need_writeback = false;
            if (!(page_table_entry & A)) {
                page_table_entry |= A;
                need_writeback = true;
            }

            // If we're writing data, set the D bit if needed
            if (access == AccessType::Store && !(page_table_entry & D)) {
                page_table_entry |= D;
                need_writeback = true;
            }

            if (need_writeback) {
                // "w"rite the updated PTE back (little endian) — handle error if write fails
                core.address_space().write_physical(entry_addr, util::to_byte_span(page_table_entry));
            }

            // Build physical address
            const uint32_t ppn1 = util::extract_bits<20,31>(page_table_entry);
            const uint32_t ppn0 = util::extract_bits<10,19>(page_table_entry);
            const T offset = va & (PageSize - 1);

            T physical_page_address;
            if (level == 1) {
                // 4 MB superpage
                physical_page_address = (static_cast<T>(ppn1) << 22) | (static_cast<T>(vpns[0]) << 12);
            } else {
                physical_page_address = (static_cast<T>(ppn1) << 22) | (static_cast<T>(ppn0) << 12);
            }

            m_tlb[entry_addr] = physical_page_address;
            return physical_page_address | offset;
        }

        constexpr auto invalidate() -> void final {
            m_tlb.clear();
        }

    private:
        std::map<std::uint32_t, std::uint32_t> m_tlb;
    };

}