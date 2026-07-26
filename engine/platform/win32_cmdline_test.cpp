#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "win32_cmdline.hpp"

// Гейт 8 (спека #12): цитирование argv для CreateProcessW проверяется round-trip'ом через сам
// CommandLineToArgvW — тот самый разборщик, которым пользуется ребёнок. Эталон здесь не наше
// представление о правилах, а поведение ОС: наивная склейка через пробел этот тест валит.
//
// argv[0] у CommandLineToArgvW разбирается ОСОБЫМ правилом (обратные слэши не экранируют,
// строка идёт до следующей кавычки), поэтому нулевым аргументом везде стоит реалистичный путь
// к exe, а патологии живут с первого — как оно и бывает у пекаря.
namespace {

const wchar_t* kExe = L"C:\\Program Files\\like-nes\\tint.exe";

bool roundtrip(const std::vector<std::wstring>& argv) {
    const std::wstring cmd = platform::win32::command_line(argv);
    int n = 0;
    wchar_t** back = CommandLineToArgvW(cmd.c_str(), &n);
    if (!back) {
        std::fprintf(stderr, "[win32-cmdline] CommandLineToArgvW failed\n");
        return false;
    }
    bool ok = static_cast<size_t>(n) == argv.size();
    for (int i = 0; ok && i < n; ++i) ok = argv[static_cast<size_t>(i)] == back[i];
    if (!ok) {
        std::fprintf(stderr, "[win32-cmdline] FAIL: %d args back, expected %zu\n", n, argv.size());
        for (int i = 0; i < n; ++i) std::fwprintf(stderr, L"  back[%d] = <%ls>\n", i, back[i]);
        for (size_t i = 0; i < argv.size(); ++i)
            std::fwprintf(stderr, L"  want[%zu] = <%ls>\n", i, argv[i].c_str());
    }
    LocalFree(back);
    return ok;
}

} // namespace

int main() {
    const std::vector<std::vector<std::wstring>> cases = {
        {kExe, L"shader.wgsl", L"--format", L"spirv"},
        {kExe, L"C:\\Program Files\\assets\\sprite.wgsl"},   // пробел в аргументе
        {kExe, L"C:\\out\\"},                                // хвостовой слэш перед кавычкой
        {kExe, L"C:\\Users\\\u041f\u0451\u0442\u0440\\art"}, // кириллический профиль
        {kExe, L"a\"b"},                                     // кавычка внутри
        {kExe, L"a\\\\\"b"},                                 // слэши перед кавычкой
        {kExe, L""},                                         // пустой аргумент
        {kExe, L"tab\there", L"-o", L"out dir\\x.spv"},
    };
    for (const auto& c : cases)
        if (!roundtrip(c)) return 1;
    std::printf("[win32-cmdline] PASS %zu cases: argv -> command line -> CommandLineToArgvW\n",
                cases.size());
    return 0;
}
