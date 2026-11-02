#pragma once
#include <string>

namespace mobile {

// Услуги оболочки игре. Определяет оболочка своей ОС (platform/ios/host.mm,
// platform/android/android_host.cpp).

// Путь к файлу из ассетов приложения: iOS — `<.app>/assets/<имя>`, Android — копия из APK в
// песочнице (exe_dir у приложения Android — app_process системы, рядом с ним ассетов нет).
std::string asset_path(const char* name);

// Флаг запуска (`--demo`): iOS — аргументы процесса, Android — boolean extra demo интента.
bool has_arg(const char* flag);

// Подсказка планировщику: текущий рабочий поток обслуживает интерактивную задачу.
void interactive_thread();

// Строка устройства для журнала бенча: ОС, версия, модель.
std::string device_info();

} // namespace mobile
