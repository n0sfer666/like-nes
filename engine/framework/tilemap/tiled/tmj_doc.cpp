#include "tmj_doc.hpp"

namespace framework::tiled {
namespace {

const char* kind_name(json::Kind kind) {
    switch (kind) {
    case json::Kind::Bool: return "true or false";
    case json::Kind::Int: return "an integer";
    case json::Kind::Fix: return "a number";
    case json::Kind::String: return "a string";
    case json::Kind::Array: return "an array";
    case json::Kind::Object: return "an object";
    default: return "null";
    }
}

} // namespace

bool Doc::load(std::string& error) {
    arena.init(in.size());
    json::JsonError e;
    if (!json::parse(in, arena, nodes, e)) {
        error = json::format_error(file, e);
        return false;
    }
    if (root().kind != json::Kind::Object) return fail(root(), "expected a JSON object at the top level", error);
    return true;
}

std::span<const json::Node> Doc::items(const json::Node& array) const {
    return std::span<const json::Node>(nodes).subspan(array.first, array.count);
}

bool Doc::fail(const json::Node& at, const std::string& message, std::string& error) const {
    error = json::format_error(file, json::error_at(in, at.at, message));
    return false;
}

const json::Node* Doc::field(const json::Node& obj, std::string_view key) const {
    return json::member(nodes, obj, key);
}

bool Doc::expect(const json::Node& obj, std::string_view key, json::Kind kind, Need need,
                 const json::Node*& out, std::string& error) const {
    out = field(obj, key);
    if (out == nullptr)
        return need == Need::Optional || fail(obj, "missing '" + std::string(key) + "'", error);
    if (out->kind == kind || (kind == json::Kind::Fix && out->kind == json::Kind::Int)) return true;
    return fail(*out, "'" + std::string(key) + "' must be " + kind_name(kind), error);
}

bool Doc::get(const json::Node& obj, std::string_view key, int64_t& out, std::string& error, Need need) const {
    const json::Node* n = nullptr;
    if (!expect(obj, key, json::Kind::Int, need, n, error)) return false;
    if (n != nullptr) out = n->i;
    return true;
}

bool Doc::get(const json::Node& obj, std::string_view key, fix32& out, std::string& error, Need need) const {
    const json::Node* n = nullptr;
    if (!expect(obj, key, json::Kind::Fix, need, n, error)) return false;
    if (n != nullptr && !json::as_fix(*n, out))
        return fail(*n, "'" + std::string(key) + "' is out of range, keep it above -32768 and below 32768", error);
    return true;
}

bool Doc::get(const json::Node& obj, std::string_view key, bool& out, std::string& error, Need need) const {
    const json::Node* n = nullptr;
    if (!expect(obj, key, json::Kind::Bool, need, n, error)) return false;
    if (n != nullptr) out = n->i != 0;
    return true;
}

bool Doc::get(const json::Node& obj, std::string_view key, std::string_view& out, std::string& error,
              Need need) const {
    const json::Node* n = nullptr;
    if (!expect(obj, key, json::Kind::String, need, n, error)) return false;
    if (n != nullptr) out = n->s;
    return true;
}

bool Doc::prop(const json::Node& obj, std::string_view name, std::string_view type, const json::Node*& value,
               std::string& error) const {
    value = nullptr;
    const json::Node* list = nullptr;
    if (!expect(obj, "properties", json::Kind::Array, Need::Optional, list, error)) return false;
    if (list == nullptr) return true;
    for (const json::Node& p : items(*list)) {
        if (p.kind != json::Kind::Object) return fail(p, "a property must be an object", error);
        std::string_view n, t = "string";
        if (!get(p, "name", n, error, Need::Required) || !get(p, "type", t, error)) return false;
        if (n != name) continue;
        if (t != type)
            return fail(p, "property '" + std::string(name) + "' must be of type " + std::string(type) +
                               ", found " + std::string(t), error);
        return expect(p, "value", type == "bool" ? json::Kind::Bool : json::Kind::String, Need::Required,
                      value, error);
    }
    return true;
}

} // namespace framework::tiled
