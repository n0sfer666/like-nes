#include "json.hpp"

#include <algorithm>
#include <utility>

#include "json_scan.hpp"

namespace framework::json {
namespace {

using detail::Cursor;

// Дети контейнера копятся на `stack` и уходят в `out` одним блоком при закрытии скобки: так у
// каждого контейнера дети лежат подряд, хотя вложенные контейнеры закрываются раньше родителя.
struct Parser {
    Cursor c;
    std::vector<Node>* out = nullptr;
    std::vector<Node> stack;
    std::vector<std::size_t> keys;

    void skip_ws() {
        for (int b = c.peek(); b == ' ' || b == '\t' || b == '\n' || b == '\r'; b = c.peek())
            ++c.pos;
    }

    void close(Node& node, std::size_t base, uint32_t count) {
        node.first = static_cast<uint32_t>(out->size());
        node.count = count;
        out->insert(out->end(), stack.begin() + static_cast<std::ptrdiff_t>(base), stack.end());
        stack.resize(base);
    }

    bool push(const Node& node) {
        if (out->size() + stack.size() >= MAX_NODES)
            return c.error(node.at, "more than 2097152 values in the document");
        stack.push_back(node);
        return true;
    }

    bool literal(Node& node, std::string_view word, Kind kind, int64_t i) {
        if (c.size - c.pos < word.size() || std::string_view(c.data + c.pos, word.size()) != word)
            return c.error(c.pos, "expected a value");
        c.pos += word.size();
        node.kind = kind;
        node.i = i;
        return true;
    }

    bool value(Node& node, uint32_t depth) {
        skip_ws();
        node = Node{};
        node.at = static_cast<uint32_t>(c.pos);
        const int b = c.peek();
        if (b < 0) return c.error(c.pos, "unexpected end of input");
        if (b == '{') return object(node, depth + 1);
        if (b == '[') return array(node, depth + 1);
        if (b == '"') {
            node.kind = Kind::String;
            return detail::scan_string(c, node.s);
        }
        if (b == 't') return literal(node, "true", Kind::Bool, 1);
        if (b == 'f') return literal(node, "false", Kind::Bool, 0);
        if (b == 'n') return literal(node, "null", Kind::Null, 0);
        if (b == '-' || (b >= '0' && b <= '9')) return detail::scan_number(c, node);
        return c.error(c.pos, "expected a value");
    }

    bool array(Node& node, uint32_t depth) {
        if (depth > MAX_DEPTH) return c.error(c.pos, "nesting deeper than 64");
        node.kind = Kind::Array;
        ++c.pos;
        const std::size_t base = stack.size();
        uint32_t count = 0;
        skip_ws();
        if (c.peek() == ']') {
            ++c.pos;
            close(node, base, 0);
            return true;
        }
        for (;;) {
            Node item;
            if (!value(item, depth) || !push(item)) return false;
            ++count;
            skip_ws();
            if (c.peek() == ']') break;
            if (c.peek() != ',') return c.error(c.pos, "expected ',' or ']'");
            ++c.pos;
        }
        ++c.pos;
        close(node, base, count);
        return true;
    }

    bool object(Node& node, uint32_t depth) {
        if (depth > MAX_DEPTH) return c.error(c.pos, "nesting deeper than 64");
        node.kind = Kind::Object;
        ++c.pos;
        const std::size_t base = stack.size();
        uint32_t count = 0;
        skip_ws();
        if (c.peek() == '}') {
            ++c.pos;
            close(node, base, 0);
            return true;
        }
        for (;;) {
            skip_ws();
            if (c.peek() != '"') return c.error(c.pos, "expected a string key");
            Node key;
            key.kind = Kind::String;
            key.at = static_cast<uint32_t>(c.pos);
            if (!detail::scan_string(c, key.s)) return false;
            skip_ws();
            if (c.peek() != ':') return c.error(c.pos, "expected ':'");
            ++c.pos;
            Node val;
            if (!value(val, depth) || !push(key) || !push(val)) return false;
            ++count;
            skip_ws();
            if (c.peek() == '}') break;
            if (c.peek() != ',') return c.error(c.pos, "expected ',' or '}'");
            ++c.pos;
        }
        ++c.pos;
        if (!unique_keys(base, count)) return false;
        close(node, base, count);
        return true;
    }

    // Сортировка, а не попарное сравнение: объект на миллион ключей — законный вход до лимита
    // 64 МиБ, и квадрат по нему подвесил бы бейк. Отказ называет ВТОРОЕ вхождение — его и правят.
    bool unique_keys(std::size_t base, uint32_t count) {
        keys.clear();
        for (uint32_t k = 0; k < count; ++k) keys.push_back(base + 2 * std::size_t{k});
        std::sort(keys.begin(), keys.end(), [this](std::size_t a, std::size_t b) {
            return stack[a].s != stack[b].s ? stack[a].s < stack[b].s : stack[a].at < stack[b].at;
        });
        for (std::size_t k = 1; k < keys.size(); ++k) {
            const Node& dup = stack[keys[k]];
            if (stack[keys[k - 1]].s != dup.s) continue;
            return c.error(dup.at, "duplicate key \"" + detail::printable(dup.s) + "\"");
        }
        return true;
    }
};

bool is_bom(std::span<const std::byte> in) {
    return in.size() >= 3 && in[0] == std::byte{0xEF} && in[1] == std::byte{0xBB} &&
           in[2] == std::byte{0xBF};
}

} // namespace

bool parse(std::span<const std::byte> in, asset::ByteArena& arena, std::vector<Node>& out,
           JsonError& err) {
    out.clear();
    if (in.size() > MAX_DOCUMENT) {
        err = error_at(in, 0, "input larger than 64 MiB");
        return false;
    }
    if (is_bom(in)) {
        err = error_at(in, 0, "UTF-8 BOM is not allowed, save the file as UTF-8 without BOM");
        return false;
    }
    Parser p;
    // Вход — сырые байты файла, а срезы строк дерева — `std::string_view`: zero-parse взгляд на
    // те же байты, без копии (ADR 0003). Ширина `char` и `std::byte` одна по стандарту.
    p.c.data = reinterpret_cast<const char*>(in.data());
    p.c.size = in.size();
    p.c.arena = &arena;
    p.out = &out;
    out.emplace_back();
    Node root;
    bool ok = p.value(root, 0);
    if (ok) {
        p.skip_ws();
        if (p.c.pos != p.c.size)
            ok = p.c.error(p.c.pos, "unexpected characters after the document");
    }
    if (!ok) {
        err = error_at(in, static_cast<uint32_t>(p.c.fail_at), std::move(p.c.fail));
        out.clear();
        return false;
    }
    out[0] = root;
    return true;
}

const Node* member(std::span<const Node> doc, const Node& obj, std::string_view key) {
    if (obj.kind != Kind::Object) return nullptr;
    if (obj.first + 2 * std::size_t{obj.count} > doc.size()) return nullptr;
    for (uint32_t k = 0; k < obj.count; ++k) {
        const std::size_t i = obj.first + 2 * std::size_t{k};
        if (doc[i].s == key) return &doc[i + 1];
    }
    return nullptr;
}

} // namespace framework::json
