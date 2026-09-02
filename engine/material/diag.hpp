#pragma once

#include <string>

namespace mat {

// Диагностика шейдера в терминах панели редактора (#7): файл, строка, колонка, текст.
// Отдельным заголовком от валидатора намеренно — у неё нет ни одной зависимости от GPU, и ровно
// поэтому её умеет линковать тест, доказывающий, что парсер редактора разбирает НАШУ строку
// обратно в те же поля. Совместимость, записанная соглашением, разъезжается молча.
struct ShaderDiag {
    std::string file;
    int line = 0;   // 0 — позиции в сообщении не было (отказ пайплайна, а не разбор модуля)
    int col = 0;
    std::string message;
};

// Разбор сообщения валидатора wgpu. Позиция едет ВНУТРИ текста ("wgsl:14:7"), а не отдельным
// полем: структурного канала для неё у wgpu-native нет, и это измерено, а не предположено —
// wgpuShaderModuleGetCompilationInfo в этой сборке не отдаёт ни одного сообщения.
ShaderDiag parse_wgpu_error(const std::string& file, const std::string& wgpu_message);

// Строка для панели: "file:line:col: error: message" — формат clang/gcc, который разбирает
// ide::build::parse_diagnostics. Не-ASCII из чужого текста здесь же гасится: консоль Windows не
// UTF-8, и рамка ┌─ из вывода naga превращала бы диагностику в мусор.
std::string format_diag(const ShaderDiag& d);

} // namespace mat
