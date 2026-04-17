#pragma once

#include <cstdint>
#include <cstring>
#include <bit>
#include <emu/riscv/core.hpp>

namespace ds::emu::riscv::m_mode {

    template<auto ... Fs>
    struct Functions {

    };

    struct ExtensionId {
        constexpr ExtensionId(std::uint32_t id) : value(id) {}
        constexpr ExtensionId(const char (&id)[5])
            : value((id[0] << 24) | (id[1] << 16) | (id[2] << 8) | id[3]) { }
        std::uint32_t value;
    };

    template<ExtensionId ExtensionID>
    struct Extension {
        constexpr static auto ID = ExtensionID.value;

        template<std::uint32_t FunctionID, auto F>
        struct Function {
            constexpr static auto ID = FunctionID;
            constexpr static auto Func = F;
        };
    };

    enum class SBICallErrorCode : std::int32_t {
        Success = 0,
        Failed = -1,
        NotSupported = -2,
        InvalidParam = -3,
        Denied = -4,
        InvalidAddress = -5,
        AlreadyAvailable = -6,
        AlreadyStarted = -7,
        AlreadyStopped = -8,
        NoSharedMemory = -9,
    };

    struct SBICallResult {
        SBICallErrorCode error;
        std::uint32_t return_value;
    };

    template<typename Extensions>
    class MachineModeFirmware {
    public:
        auto sbi_call(
            Core &core,
            std::uint32_t extension_id, std::uint32_t function_id,
            std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3, std::uint32_t arg4, std::uint32_t arg5
        ) -> SBICallResult {
            const auto result = dispatch_call_to_extension(
                core,
                extension_id, function_id,
                { arg0, arg1, arg2, arg3, arg4, arg5 }
            );

            if (result.error == SBICallErrorCode::NotSupported) {
                char extension_string[5] = {};
                auto swapped = std::byteswap(extension_id);
                std::memcpy(extension_string, &swapped, sizeof(swapped));
                printf("Unimplemented SBI Extension Function Call to [0x%08X (%s)](0x%08X)\n", extension_id, extension_string, function_id);
            }

            return result;
        }

        auto update(Core &core) -> void {
            // Call the update function of each extension if it's available
            std::apply(
                [&](auto& ...extensions) {
                    (..., update_extension(core, extensions));
                },
                m_extensions
            );
        }

        auto reset() -> void {
            // Call the reset function of each extension if it's available
            std::apply(
                [&](auto& ...extensions) {
                    (..., reset_extension(extensions));
                },
                m_extensions
            );
        }

    private:
        constexpr static auto update_extension(Core& core, auto &extension) -> void {
            if constexpr (requires { extension.update(core); })
                extension.update(core);
            else if constexpr (requires { extension.update(); })
                extension.update();
        }

        constexpr static auto reset_extension(auto &extension) -> void {
            if constexpr (requires { extension.reset(); }) {
                extension.reset();
            }
        }

        template<typename FunctionSignature>
        constexpr static auto takes_core_parameter() {
            if constexpr (FunctionSignature::ArgumentCount == 0) {
                return false;
            } else {
                return std::same_as<std::tuple_element_t<0, typename FunctionSignature::Arguments>, Core&>;
            }
        }

        template<typename Extension, auto Function>
        constexpr auto call_sbi_extension_function(
            Core &core,
            Extension &extension,
            const std::array<std::uint32_t, 6> &args
        ) -> SBICallResult {
            using Signature = util::FunctionSignature<decltype(Function)>;

            static_assert(
                std::same_as<typename Signature::ReturnType, SBICallResult>,
                "SBI Extension function must return a SBICallResult!"
            );

            constexpr auto N = Signature::ArgumentCount;
            static_assert(N <= args.size(), "SBI Extension Function may not have more than 6 parameters!");

            // Wrapper to uniformly call both a static and non-static member function
            auto invoke_extension_function = [&]<typename... Ts>(Ts&&... params) -> SBICallResult {
                constexpr static bool StaticMemberFunction = std::same_as<typename Signature::Class, void>;

                if constexpr (StaticMemberFunction)
                    return Function(std::forward<Ts>(params)...);
                else
                    return (extension.*Function)(std::forward<Ts>(params)...);
            };

            // Invoke extension function, passing in the requested number of parameters
            // and optionally a reference to the calling core
            constexpr static bool TakesCoreParameter = takes_core_parameter<Signature>();
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                if constexpr (TakesCoreParameter)
                    return invoke_extension_function(core, args[I]...);
                else
                    return invoke_extension_function(args[I]...);
            }(std::make_index_sequence<TakesCoreParameter ? N - 1 : N>{});
        }

        template<typename Extension, std::size_t Index = 0>
        constexpr auto dispatch_call_to_function(
            Core &core,
            Extension &extension,
            std::uint32_t function_id,
            const std::array<std::uint32_t, 6> &args
        ) -> SBICallResult {
            using Functions = Extension::Functions;

            if constexpr (Index >= std::tuple_size_v<Functions>) {
                // No extension with the given ID was found in this extension
                return { SBICallErrorCode::NotSupported, 0 };
            } else {
                using Function = std::tuple_element_t<Index, Functions>;

                if (Function::ID == function_id) {
                    // Function with requested id was found, delegate execution to it
                    return call_sbi_extension_function<Extension, Function::Func>(
                        core,
                        extension,
                        args
                    );
                }

                // Check the next function in the list
                return dispatch_call_to_function<Extension, Index + 1>(
                    core,
                    extension,
                    function_id,
                    args
                );
            }
        }

        template<std::size_t Index = 0>
        constexpr auto dispatch_call_to_extension(
            Core &core,
            std::uint32_t extension_id, std::uint32_t function_id,
            const std::array<std::uint32_t, 6> &args
        ) -> SBICallResult {
            if constexpr (Index >= std::tuple_size_v<Extensions>) {
                // No extension with the given ID was found
                return { SBICallErrorCode::NotSupported, 0 };
            } else {
                using Extension = std::tuple_element_t<Index, Extensions>;

                if (Extension::ID == extension_id) {
                    // Extension with requested id was found, delegate execution to it
                    auto &extension = std::get<Index>(m_extensions);
                    return dispatch_call_to_function<Extension>(
                        core,
                        extension,
                        function_id,
                        args
                    );
                }

                // Check the next extension in the list
                return dispatch_call_to_extension<Index + 1>(
                    core,
                    extension_id, function_id,
                    args
                );
            }
        }

    private:
        Extensions m_extensions;
    };

}
