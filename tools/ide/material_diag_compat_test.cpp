#include <cstdio>
#include <string>

#include "../../engine/material/diag.hpp"
#include "compile/diagnostics.hpp"

// Совместимость диагностики шейдера с панелью #7 — УТВЕРЖДЕНИЕМ, а не соглашением в комментарии.
// Валидатор бейка (гейт 2 спеки #18) печатает строку, а разбирает её парсер редактора; между ними
// нет ни одного общего типа, поэтому разъехаться они могут молча — и увидеть это можно было бы
// только глазами в панели, где вместо ошибки стоит пустота.
//
// GPU здесь не нужен и не поднимается: формат строки — чистая функция от текста валидатора.

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "[material-diag] FAIL: %s\n", what);
        ++failures;
    }
}

// Сообщение wgpu-native дословно (замерено пробой на битом WGSL): рамка naga, позиция внутри
// текста, повтор сути в хвосте. Фикстура ЖИВАЯ — придуманный формат проверял бы фантазию.
const char* module_error() {
    return "Validation Error\n"
           "\n"
           "Caused by:\n"
           "    In wgpuDeviceCreateShaderModule\n"
           "      note: label = `effects`\n"
           "    \n"
           "Shader 'effects' parsing error: expected expression, found ';'\n"
           "  \xe2\x94\x8c\xe2\x94\x80 wgsl:3:18\n"
           "  \xe2\x94\x82\n"
           "3 \xe2\x94\x82     let x: f32 = ;\n"
           "  \xe2\x94\x82                  ^ expected expression\n"
           "\n"
           "\n"
           "    expected expression, found ';'\n";
}

const char* pipeline_error() {
    return "Validation Error\n"
           "\n"
           "Caused by:\n"
           "    In wgpuDeviceCreateRenderPipeline\n"
           "    Error matching ShaderStages(FRAGMENT) shader requirements against the pipeline\n"
           "    Unable to find entry point 'fs_nope'\n";
}

} // namespace

int main() {
    const std::string file = "engine/material/library/sprite_effects.wgsl";

    const mat::ShaderDiag m = mat::parse_wgpu_error(file, module_error());
    check(m.line == 3 && m.col == 18, "position of a module error comes from the wgsl: marker");
    check(m.message == "expected expression, found ';'", "message is the reason, not the frame");

    const mat::ShaderDiag p = mat::parse_wgpu_error(file, pipeline_error());
    check(p.line == 0 && p.col == 0, "a pipeline error carries no position and does not invent one");
    check(p.message == "Unable to find entry point 'fs_nope'", "pipeline reason is the last line");

    const std::vector<ide::build::Diagnostic> panel =
        ide::build::parse_diagnostics(mat::format_diag(m) + "\n" + mat::format_diag(p) + "\n");
    check(panel.size() == 2, "the panel parser sees both lines");
    if (panel.size() == 2) {
        check(panel[0].file == file && panel[0].line == 3 && panel[0].col == 18,
              "click-to-open lands on the same file:line:col");
        check(panel[0].severity == "error", "severity survives the round trip");
        check(panel[0].message == m.message, "message survives the round trip");
        check(panel[1].file == file && panel[1].line == 0 && panel[1].message == p.message,
              "the positionless diagnostic survives too");
    }

    // Контроль наоборот: сырое сообщение валидатора панель разбирать НЕ обязана, и если бы
    // разбирала, круг выше проходил бы независимо от нашего форматирования — то есть ничего бы
    // не доказывал.
    check(ide::build::parse_diagnostics(module_error()).empty(),
          "the raw wgpu text is not a panel diagnostic by itself");

    std::printf("material-diag: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
