#include <cstdio>
#include <string>

#include "framework_json_test_run.hpp"

// Дерево, отказы и лимиты разборщика (спека #24, В3). Лимиты проверяются на ОБЕИХ сторонах
// границы: отказ на 65-м уровне без приёма 64-го так же зелен у разборщика, отвергающего всё.
namespace {

using namespace json_test;

void test_tree() {
    const std::string text = R"({"a": 7, "b": [true, false, null], "c": {"d": "x"}, "e": []})";
    Run r;
    if (!parsed(r, text, "a plain document parses")) return;
    const json::Node& root = r.doc[0];
    check(root.kind == json::Kind::Object && root.count == 4, "the root is out[0], 4 pairs");
    const json::Node* a = json::member(r.doc, root, "a");
    check(a != nullptr && a->kind == json::Kind::Int && a->i == 7 && a->at == 6, "int member");
    const json::Node* b = json::member(r.doc, root, "b");
    check(b != nullptr && b->kind == json::Kind::Array && b->count == 3, "array member");
    if (b != nullptr && b->count == 3) {
        check(r.doc[b->first].kind == json::Kind::Bool && r.doc[b->first].i == 1, "true");
        check(r.doc[b->first + 1].kind == json::Kind::Bool && r.doc[b->first + 1].i == 0, "false");
        check(r.doc[b->first + 2].kind == json::Kind::Null, "null");
    }
    const json::Node* c = json::member(r.doc, root, "c");
    const json::Node* d = c != nullptr ? json::member(r.doc, *c, "d") : nullptr;
    check(d != nullptr && d->kind == json::Kind::String && d->s == "x", "nested object member");
    const json::Node* e = json::member(r.doc, root, "e");
    check(e != nullptr && e->kind == json::Kind::Array && e->count == 0, "empty array");
    check(json::member(r.doc, root, "z") == nullptr, "a missing key is nullptr");
    check(a != nullptr && json::member(r.doc, *a, "a") == nullptr, "member of a non-object");

    Run n;
    if (!parsed(n, "[[1, 2], [3]]", "nested arrays parse")) return;
    const json::Node& outer = n.doc[0];
    check(outer.count == 2, "outer array has two children");
    if (outer.count == 2) {
        const json::Node& first = n.doc[outer.first];
        const json::Node& second = n.doc[outer.first + 1];
        check(first.count == 2 && n.doc[first.first + 1].i == 2,
              "children of the first lie together");
        check(second.count == 1 && n.doc[second.first].i == 3,
              "children of the second lie together");
    }
    Run w;
    check(parse(w, " \t\r\n{}\r\n "), "whitespace around the document");
}

void test_refusals() {
    check(refused_with("{\n  \"a\": 1\n  \"b\": 2}", "expected ',' or '}'", 3, 3), "missing comma");
    check(refused_with("[\"\xD0\xB6\" x]", "expected ',' or ']'", 1, 6),
          "column counts characters");
    check(refused_with("[1,]", "expected a value", 1, 4), "trailing comma in an array");
    check(refused_with("{\"a\":1,}", "expected a string key", 1, 8), "trailing comma in an object");
    check(refused_with("{\"a\" 1}", "expected ':'", 1, 6), "missing colon");
    check(refused_with("{1:2}", "expected a string key", 1, 2), "a number as a key");
    check(refused_with("", "unexpected end of input", 1, 1), "empty input");
    check(refused_with(" \n ", "unexpected end of input", 2, 2), "whitespace only");
    check(refused_with("[1", "expected ',' or ']'", 1, 3), "unclosed array");
    check(refused_with("{} x", "unexpected characters after the document", 1, 4), "trailing junk");
    check(refused_with("tru", "expected a value", 1, 1), "truncated literal");
    check(refused_with("[nul]", "expected a value", 1, 2), "misspelled literal");
    check(refused_with("\xEF\xBB\xBF{}",
                       "UTF-8 BOM is not allowed, save the file as UTF-8 without BOM", 1, 1),
          "BOM");
    check(refused_with("{\"a\":1,\"b\":2,\"a\":3}", "duplicate key \"a\"", 1, 14),
          "duplicate key names the second occurrence");
    check(refused_with("{\"a\":1,\"\\u0061\":2}", "duplicate key \"a\"", 1, 8),
          "an escaped key duplicates its plain twin");
    check(refused_with("{\"a\\nb\":1,\"a\\nb\":2}", "duplicate key \"a\\x0ab\"", 1, 11),
          "a control character in a duplicate key is printed escaped");
    check(refused_with("{\"\xD0\xB6\":1,\"\xD0\xB6\":2}", "duplicate key \"\\xd0\\xb6\"", 1, 8),
          "a non-ASCII duplicate key is printed in ASCII");
    const std::string long_key = "\"" + std::string(65, 'k') + "\"";
    check(refused_with("{" + long_key + ":1," + long_key + ":2}",
                       ("duplicate key \"" + std::string(64, 'k') + "...\"").c_str(), 1, 72),
          "a long duplicate key is cut to 64 bytes");
    Run same;
    check(parse(same, R"({"x": {"a": 1}, "y": {"a": 2}})"),
          "one key in two objects is no duplicate");
    json::JsonError err{12, 7, "expected ','"};
    check(json::format_error("level1.tmj", err) == "level1.tmj:12:7: expected ','", "error format");
}

std::string nested(uint32_t depth) {
    std::string s(depth, '[');
    s.append(depth, ']');
    return s;
}

void test_limits() {
    Run deep;
    check(parse(deep, nested(json::MAX_DEPTH)), "depth 64 is accepted");
    check(refused_with(nested(json::MAX_DEPTH + 1), "nesting deeper than 64", 1, 65),
          "depth 65 is refused");
    std::string objects;
    for (uint32_t i = 0; i < json::MAX_DEPTH; ++i) objects += "{\"k\":";
    objects += "0";
    objects.append(json::MAX_DEPTH, '}');
    Run obj;
    check(parse(obj, objects), "64 nested objects are accepted");
    check(refused_with("[" + objects + "]", "nesting deeper than 64", 1, 317),
          "an array over 64 objects is refused");

    std::string values = "[0";
    for (std::size_t i = 2; i < json::MAX_NODES; ++i) values += ",0";
    Run most;
    check(parse(most, values + "]") && most.doc.size() == json::MAX_NODES,
          "a document of exactly 2^21 values is accepted");
    check(refused_with(values + ",0]", "more than 2097152 values in the document", 1,
                       static_cast<uint32_t>(2 * json::MAX_NODES)),
          "one value more is refused, not thrown as bad_alloc");

    std::string big(json::MAX_DOCUMENT, ' ');
    big.front() = '[';
    big.back() = ']';
    Run top;
    check(parse(top, big), "input of exactly 64 MiB is accepted");
    big.push_back(' ');
    check(refused_with(big, "input larger than 64 MiB", 1, 1), "64 MiB + 1 byte is refused");
}

} // namespace

int main() {
    std::printf("json parser: tree, refusals, limits\n");
    test_tree();
    test_refusals();
    test_limits();
    return verdict("framework-json");
}
