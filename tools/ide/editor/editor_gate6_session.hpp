#pragma once
#include <string>

// Паспорт сессии для гейта 6. Отдельно от самого гейта, потому что отвечает на другой вопрос:
// не «работает ли редактор», а «ГДЕ именно он сейчас работает» — а ровно на этом гейт и ломается.
namespace ide::editor {

struct SessionPassport {
    std::string glfw_platform;   // чем GLFW реально управляет окном
    std::string session_type;    // XDG_SESSION_TYPE — чем управляет рабочий стол
    std::string display;         // DISPLAY / WAYLAND_DISPLAY, как их видит процесс
    bool xwayland = false;       // сессия Wayland, а окно — X11-клиент
};

SessionPassport probe_session();

// Печатает паспорт и, если поймана подмена, объясняет её. Возврат false = гейт провален ещё до
// первого кадра.
bool report_session(const SessionPassport& s);

} // namespace ide::editor
