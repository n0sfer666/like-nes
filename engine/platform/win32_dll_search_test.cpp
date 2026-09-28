#include <windows.h>

#include <cstdio>
#include <string>

#include "platform_fs.hpp"
#include "platform_module.hpp"
#include "win32_utf.hpp"

// A·1·2 аудита #21: зависимость плагина не ищется в рабочем каталоге процесса. Плагин
// импортирует dll_search_dep.dll, которую CMake кладёт в подкаталог, — рядом с exe её нет, иначе
// каталог приложения из DEFAULT_DIRS нашёл бы её и отказ стал бы недостижим.
//
// Сломанность фикстуры доказывается в каждом прогоне, а не один раз руками: тот же плагин из
// того же каталога грузится старым флагом LOAD_WITH_ALTERED_SEARCH_PATH и ОБЯЗАН найти
// зависимость в CWD. Без этого контроля отказ Module::open мог бы значить «фикстура не
// собралась», а не «CWD закрыт».
namespace {

constexpr DWORD MOD_NOT_FOUND = 126;

bool fail(const char* what) {
    std::fprintf(stderr, "[win32-dll-search] FAIL: %s\n", what);
    return false;
}

bool set_cwd(const std::string& dir) {
    return SetCurrentDirectoryW(platform::win32::widen(dir).c_str()) != 0;
}

// LOAD_WITH_ALTERED_SEARCH_PATH судит о «пути» по обратным слэшам: прямой слэш из TARGET_FILE
// увёл бы контроль в неопределённое поведение. Module::open разворачивает путь так же.
std::wstring full_path(const std::string& path) {
    constexpr DWORD CAP = MAX_PATH * 4;
    wchar_t buf[CAP];
    const DWORD n = GetFullPathNameW(platform::win32::widen(path).c_str(), CAP, buf, nullptr);
    return n == 0 || n >= CAP ? std::wstring() : std::wstring(buf, n);
}

bool dep_unloaded() { return GetModuleHandleW(L"dll_search_dep.dll") == nullptr; }

bool refused_from_cwd(const std::string& plugin) {
    platform::Module m;
    if (m.open(plugin)) return fail("Module::open resolved the dependency from the CWD");
    const std::string err = platform::Module::last_error();
    if (err.find("code " + std::to_string(MOD_NOT_FOUND) + ")") == std::string::npos)
        return fail(("refused for a foreign reason: " + err).c_str());
    return dep_unloaded() || fail("the dependency stayed loaded after the refusal");
}

bool old_flag_finds_cwd(const std::string& plugin) {
    const std::wstring full = full_path(plugin);
    if (full.empty()) return fail("control: GetFullPathNameW");
    const HMODULE h = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) return fail("control: the old flag did not find the dependency in the CWD either");
    FreeLibrary(h);
    return dep_unloaded() || fail("control: the dependency stayed loaded after FreeLibrary");
}

bool finds_next_to_plugin(const std::string& plugin) {
    platform::Module m;
    if (!m.open(plugin)) return fail(("dependency next to the plugin: " +
                                      std::string(platform::Module::last_error())).c_str());
    using Probe = int (*)();
    const auto probe = reinterpret_cast<Probe>(m.symbol("dll_search_probe"));
    if (!probe) return fail("dll_search_probe is not exported");
    return probe() == 42 || fail("dll_search_probe returned a foreign value");
}

bool run() {
    const std::string root = platform::exe_dir() + "/dll_search_run";
    const std::string plug_dir = root + "/plugin";
    const std::string cwd_dir = root + "/cwd";
    const std::string plugin = plug_dir + "/dll_search_plugin.dll";
    const std::string dep_name = "/dll_search_dep.dll";
    if (!platform::ensure_dir(plug_dir) || !platform::ensure_dir(cwd_dir)) return fail("mkdir");
    platform::remove_file(plug_dir + dep_name);
    if (!platform::copy_file(DLL_SEARCH_PLUGIN, plugin) ||
        !platform::copy_file(DLL_SEARCH_DEP, cwd_dir + dep_name))
        return fail("copying the fixture");
    if (!dep_unloaded()) return fail("the dependency is loaded before the first case");
    if (!set_cwd(cwd_dir)) return fail("SetCurrentDirectoryW");
    const bool cwd_cases = refused_from_cwd(plugin) && old_flag_finds_cwd(plugin);
    if (!set_cwd(platform::exe_dir())) return fail("restoring the CWD");
    if (!cwd_cases) return false;
    if (!platform::copy_file(DLL_SEARCH_DEP, plug_dir + dep_name)) return fail("copying the dep");
    return finds_next_to_plugin(plugin);
}

} // namespace

int main() {
    if (!run()) return 1;
    std::printf("[win32-dll-search] PASS: CWD refused (126), old flag finds it there, "
                "dependency next to the plugin resolves\n");
    return 0;
}
