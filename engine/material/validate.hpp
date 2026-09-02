#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diag.hpp"
#include "table.hpp"

namespace mat {

// Отчёт по ОДНОМУ целевому бэкенду. Недоступный бэкенд — не ошибка и не молчание: он назван и
// объяснён, потому что «проверено везде» и «проверять было негде» иначе выглядят одинаково
// (риск-строка спеки #18: локальный бейк честно сообщает, какие бэкенды пропущены).
struct BackendReport {
    std::string backend;
    bool available = false;
    std::string skip_reason;
    uint32_t pipelines = 0;   // сколько уникальных (точка входа, смешивание) собралось
    std::vector<ShaderDiag> diags;
};

struct ValidateResult {
    std::vector<BackendReport> backends;
    uint32_t materials = 0;

    uint32_t checked() const;        // бэкендов, на которых проверка реально шла
    uint32_t diagnostics() const;    // находок суммарно
};

// Собирает модуль и ВСЕ пайплайны таблицы на каждом целевом бэкенде, который есть на этой машине.
// false — хоть один доступный бэкенд отказал; отказ подробно лежит в `out`.
//
// Пайплайны, а не только модуль: разбор WGSL у naga общий, а генерация кода — своя на каждый
// бэкенд, и отказывает она именно на создании пайплайна. Проверка одного модуля называлась бы
// кросс-бэкендной, ничем таковой не будучи.
bool validate_library(const std::string& file, const char* wgsl, const Table& table,
                      ValidateResult& out);

} // namespace mat
