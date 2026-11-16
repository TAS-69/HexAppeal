#pragma once
#ifdef HEX_NPC_RUNTIME_TESTS
#include <Windows.h>
#include <filesystem>
#include <string>
#endif
namespace hex {
void startNpcRuntime();
#ifdef HEX_NPC_RUNTIME_TESTS
bool runNpcTransactionTests(HMODULE surgery,const std::filesystem::path& preset,void* upload,void* model,void* actor,std::string& error);
bool runPhysicsTransactionTests(HMODULE surgery,void* upload,void* model,void* actor,void* skeleton,std::string& error);
#endif
}
