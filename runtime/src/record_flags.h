#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace hex {

template<std::size_t Count>
class RecordFlags {
public:
    bool test(std::size_t index) const noexcept {
        return index < Count && (words[index / 64].load() & bit(index)) != 0;
    }

    bool exchange(std::size_t index, bool enabled) noexcept {
        if (index >= Count) return false;
        auto& word = words[index / 64];
        const auto mask = bit(index);
        const auto previous = enabled ? word.fetch_or(mask) : word.fetch_and(~mask);
        return (previous & mask) != 0;
    }

    bool any() const noexcept {
        for (const auto& word : words)
            if (word.load()) return true;
        return false;
    }

    void clear() noexcept {
        for (auto& word : words) word.store(0);
    }

private:
    static constexpr std::uint64_t bit(std::size_t index) noexcept {
        return std::uint64_t{1} << (index % 64);
    }

    std::array<std::atomic<std::uint64_t>, (Count + 63) / 64> words{};
};

}
