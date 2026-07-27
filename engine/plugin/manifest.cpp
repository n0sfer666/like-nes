#include "manifest.hpp"
#include <sstream>

#include "platform_fs.hpp"

const char* dock_name(DockSlot s) {
    switch (s) {
        case DockSlot::Left: return "left";
        case DockSlot::Right: return "right";
        case DockSlot::Bottom: return "bottom";
        default: return "center";
    }
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

static bool parse_uint(const std::string& s, int& out) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    try { out = std::stoi(s); } catch (...) { return false; }
    return true;
}

static DockSlot parse_dock(const std::string& v) {
    if (v == "left") return DockSlot::Left;
    if (v == "right") return DockSlot::Right;
    if (v == "bottom") return DockSlot::Bottom;
    return DockSlot::Center;
}

Manifest parse_manifest(const std::string& path) {
    Manifest m;
    std::string text;
    if (!platform::read_text(path, text)) { m.error = "cannot open " + path; return m; }
    std::istringstream in(text);

    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ls(t);
        std::string tok;
        ls >> tok;
        if (tok == "plugin") {
            ls >> m.id >> m.version;
            std::string kv;
            while (ls >> kv) {
                if (kv.rfind("api=", 0) == 0) parse_uint(kv.substr(4), m.api_version);
            }
        } else if (tok == "panel") {
            PanelDecl p;
            ls >> p.id;
            std::string rest;
            std::getline(ls, rest);
            rest = trim(rest);
            std::string dock;
            if (!rest.empty() && rest.front() == '"') {
                size_t close = rest.find('"', 1);
                if (close != std::string::npos) {
                    p.title = rest.substr(1, close - 1);
                    std::string tail = trim(rest.substr(close + 1));
                    size_t dpos = tail.rfind("dock=");
                    if (dpos != std::string::npos) dock = trim(tail.substr(dpos + 5));
                } else {
                    p.title = unquote(rest);
                }
            } else {
                size_t dpos = rest.rfind("dock=");
                if (dpos != std::string::npos) {
                    dock = trim(rest.substr(dpos + 5));
                    rest = trim(rest.substr(0, dpos));
                }
                p.title = rest;
            }
            p.dock = parse_dock(dock);
            m.panels.push_back(p);
        } else if ((tok == "text" || tok == "button" || tok == "checkbox" || tok == "slider")
                   && !m.panels.empty()) {
            WidgetDecl w;
            if (tok == "text") w.kind = WidgetKind::Text;
            else if (tok == "button") w.kind = WidgetKind::Button;
            else if (tok == "checkbox") w.kind = WidgetKind::Checkbox;
            else w.kind = WidgetKind::SliderInt;
            std::string rest;
            std::getline(ls, rest);
            w.label = unquote(trim(rest));
            m.panels.back().widgets.push_back(w);
        }
    }
    m.ok = !m.id.empty() && !m.panels.empty();
    if (!m.ok && m.error.empty()) m.error = "no plugin id or no panels in " + path;
    return m;
}
