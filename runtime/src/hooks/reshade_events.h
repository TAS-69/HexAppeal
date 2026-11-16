#pragma once

#include <cstdint>

namespace ml::hooks::reshade_api
{








    inline constexpr uint32_t version = 20;
    inline constexpr uint32_t destroySwapchain = 8;


    inline constexpr uint32_t present = 74;

    inline constexpr uint32_t reshadePresent = 75;
    inline constexpr uint32_t d3d12 = 0xc000;


    inline constexpr uint32_t colorSpaceSrgb = 1;
    inline constexpr uint32_t colorSpaceScrgb = 2;
    inline constexpr uint32_t colorSpaceHdr10Pq = 3;



    struct Object
    {
        virtual uint64_t get_native() const = 0;
        virtual void get_private_data(const uint8_t guid[16], uint64_t *data) const = 0;
        virtual void set_private_data(const uint8_t guid[16], uint64_t data) = 0;
    };
    struct Device : Object
    {
        virtual uint32_t get_api() const = 0;
    };
    struct DeviceObject : Object
    {
        virtual Device *get_device() = 0;
    };
    struct CommandQueue : DeviceObject
    {
        virtual uint32_t get_type() const = 0;
        virtual void wait_idle() const = 0;
        virtual void flush_immediate_command_list() const = 0;
    };
    struct Swapchain : DeviceObject
    {
        virtual void *get_hwnd() const = 0;
        virtual uint64_t get_back_buffer(uint32_t index) = 0;
        virtual uint32_t get_back_buffer_count() const = 0;
        virtual uint32_t get_current_back_buffer_index() const = 0;
        virtual bool check_color_space_support(uint32_t space) const = 0;
        virtual uint32_t get_color_space() const = 0;
    };


    struct EffectRuntime : DeviceObject
    {
        virtual void *get_hwnd() const = 0;
        virtual uint64_t get_back_buffer(uint32_t index) = 0;
        virtual uint32_t get_back_buffer_count() const = 0;
        virtual uint32_t get_current_back_buffer_index() const = 0;
        virtual CommandQueue *get_command_queue() = 0;
    };

    using RegisterAddon = bool (*)(void *module, uint32_t version);
    using UnregisterAddon = void (*)(void *module);
    using RegisterEvent = void (*)(void *module, uint32_t event, void *callback);
}
