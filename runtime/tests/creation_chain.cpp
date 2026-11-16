#include "hooks/dx12/creation_chain.h"
#include "hooks/dx12/creation_settle.h"
#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, const char* name) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static void Absolute(unsigned char* from, const void* to) {
    std::memset(from, 0x90, 16);
    from[0] = 0xFF; from[1] = 0x25;
    std::memset(from + 2, 0, 4);
    std::memcpy(from + 6, &to, sizeof to);
}
int main() {
    using namespace hex::creation;
    FactorySettle settle(100);
    Check(!settle.observe(100,1),"initial dispatch is not settled");
    Check(!settle.observe(1700,1),"minimum startup interval");
    Check(!settle.observe(1800,2),"late proxy resets stability interval");
    Check(!settle.observe(2400,2),"proxy needs full stable interval");
    Check(settle.observe(2450,2),"settled proxy dispatch accepted");
    FactorySettle deadline(10);
    Check(!deadline.observe(5009,0),"missing entry waits within bound");
    Check(deadline.observe(5010,0),"startup wait is bounded");
    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(page != nullptr, "allocate fixture");
    std::memset(page, 0x90, 4096);
    void* ours = page + 512;
    Absolute(page, page + 64);
    Absolute(page + 64, ours);
    Check(Observe(page, ours).state == EntryState::Ours, "ours through another relay");



    Absolute(page + 64, page + 128);
    std::array<unsigned char, 4096> before{};
    std::memcpy(before.data(), page, before.size());
    DWORD old{};
    Check(VirtualProtect(page, 4096, PAGE_READONLY, &old) != FALSE, "make foreign relays read-only");
    for (int i = 0; i < 10000; ++i) {
        const auto observation = Observe(page, ours);
        Check(observation.state == EntryState::Other && observation.destination == page + 128, "foreign detour remains first");
    }
    Check(!std::memcmp(before.data(), page, before.size()), "no relay or entry rewrite");
    Check(VirtualProtect(page, 4096, PAGE_READWRITE, &old) != FALSE, "restore relay fixture");

    Absolute(page + 128, page + 64);
    Check(Observe(page, ours).state == EntryState::Cycle, "foreign two-node cycle is bounded");
    Absolute(page + 64, page + 64);
    Check(Observe(page, ours).state == EntryState::Cycle, "self cycle is bounded");
    for (int i = 0; i < 18; ++i) Absolute(page + i * 32, page + (i + 1) * 32);
    Check(Observe(page, page + 2048).state == EntryState::HopLimit, "long chain is bounded");

    std::memset(page, 0x90, 4096);
    page[0] = 0xE9; int32_t relative = 64 - 5; std::memcpy(page + 1, &relative, 4);
    page[64] = 0xEB; page[65] = 30;
    page[96] = 0x48; page[97] = 0xB8; std::memcpy(page + 98, &ours, sizeof ours);
    page[106] = 0xFF; page[107] = 0xE0;
    Check(Observe(page, ours).state == EntryState::Ours, "relative short and absolute register jumps");
    Check(Observe(nullptr, ours).state == EntryState::Unreadable, "null entry fails closed");
    Check(Observe(reinterpret_cast<void*>(1), ours).state == EntryState::Unreadable, "invalid entry is contained");
    Check(VirtualProtect(page, 4096, PAGE_NOACCESS, &old) != FALSE, "protect fixture");
    Check(Observe(page, ours).state == EntryState::Unreadable, "freed or inaccessible relay is contained");
    Check(VirtualProtect(page, 4096, PAGE_READWRITE, &old) != FALSE, "restore fixture");
    Absolute(page, reinterpret_cast<void*>(1));
    Check(Observe(page, ours).state == EntryState::Unreadable, "unreadable indirect destination is contained");

    relative = 4096 - 6; std::memcpy(page + 2, &relative, 4);
    auto* guard = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(guard != nullptr, "allocate guarded slot");
    Absolute(guard, ours);
    std::memcpy(guard + 2, &relative, 4);
    Check(VirtualProtect(guard + 4096, 4096, PAGE_NOACCESS, &old) != FALSE, "guard indirect slot");
    Check(Observe(guard, ours).state == EntryState::Unreadable, "unreadable indirect slot is contained");
    VirtualFree(guard, 0, MEM_RELEASE);
    VirtualFree(page, 0, MEM_RELEASE);
    std::puts("PASS: read-only creation hook inspection; displaced relay bytes preserved; cycles and invalid reads bounded.");
}
