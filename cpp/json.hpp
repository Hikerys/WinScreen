#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <cwchar>

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::wstring strVal;
    std::vector<JsonValue> arrVal;
    std::map<std::wstring, JsonValue> objVal;

    static JsonValue Null() { return JsonValue(); }
    static JsonValue FromBool(bool b) { JsonValue v; v.type = JsonType::Bool; v.boolVal = b; return v; }
    static JsonValue FromNumber(double n) { JsonValue v; v.type = JsonType::Number; v.numVal = n; return v; }
    static JsonValue FromString(const std::wstring& s) { JsonValue v; v.type = JsonType::String; v.strVal = s; return v; }
    static JsonValue Object() { JsonValue v; v.type = JsonType::Object; return v; }
    static JsonValue Array() { JsonValue v; v.type = JsonType::Array; return v; }

    bool isObject() const { return type == JsonType::Object; }
    bool isString() const { return type == JsonType::String; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isBool() const { return type == JsonType::Bool; }
    bool isNull() const { return type == JsonType::Null; }

    bool has(const std::wstring& k) const {
        return type == JsonType::Object && objVal.find(k) != objVal.end();
    }

    std::wstring getString(const std::wstring& k, const std::wstring& def = L"") const {
        if (type != JsonType::Object) return def;
        auto it = objVal.find(k);
        if (it != objVal.end() && it->second.type == JsonType::String) return it->second.strVal;
        return def;
    }

    int getInt(const std::wstring& k, int def = 0) const {
        if (type != JsonType::Object) return def;
        auto it = objVal.find(k);
        if (it != objVal.end() && it->second.type == JsonType::Number) return (int)it->second.numVal;
        return def;
    }

    bool getBool(const std::wstring& k, bool def = false) const {
        if (type != JsonType::Object) return def;
        auto it = objVal.find(k);
        if (it != objVal.end() && it->second.type == JsonType::Bool) return it->second.boolVal;
        return def;
    }

    JsonValue getObj(const std::wstring& k) const {
        if (type != JsonType::Object) return JsonValue::Object();
        auto it = objVal.find(k);
        if (it != objVal.end() && it->second.type == JsonType::Object) return it->second;
        return JsonValue::Object();
    }

    void set(const std::wstring& k, const JsonValue& v) {
        if (type != JsonType::Object) type = JsonType::Object;
        objVal[k] = v;
    }
    void set(const std::wstring& k, const wchar_t* s) { set(k, FromString(s ? s : L"")); }
    void set(const std::wstring& k, const std::wstring& s) { set(k, FromString(s)); }
    void set(const std::wstring& k, int n) { set(k, FromNumber(n)); }
    void set(const std::wstring& k, bool b) { set(k, FromBool(b)); }

    std::wstring serialize(int indent = 0) const {
        std::wstring ind(indent * 2, L' ');
        std::wstring nextInd((indent + 1) * 2, L' ');
        switch (type) {
            case JsonType::Null: return L"null";
            case JsonType::Bool: return boolVal ? L"true" : L"false";
            case JsonType::Number: {
                if ((double)(long long)numVal == numVal) return std::to_wstring((long long)numVal);
                return std::to_wstring(numVal);
            }
            case JsonType::String: {
                std::wstring out = L"\"";
                for (wchar_t c : strVal) {
                    if (c == L'\"') out += L"\\\"";
                    else if (c == L'\\') out += L"\\\\";
                    else if (c == L'\n') out += L"\\n";
                    else if (c == L'\r') out += L"\\r";
                    else if (c == L'\t') out += L"\\t";
                    else out += c;
                }
                out += L"\"";
                return out;
            }
            case JsonType::Array: {
                if (arrVal.empty()) return L"[]";
                std::wstring out = L"[\n";
                for (size_t i = 0; i < arrVal.size(); ++i) {
                    out += nextInd + arrVal[i].serialize(indent + 1);
                    if (i + 1 < arrVal.size()) out += L",";
                    out += L"\n";
                }
                out += ind + L"]";
                return out;
            }
            case JsonType::Object: {
                if (objVal.empty()) return L"{}";
                std::wstring out = L"{\n";
                size_t i = 0;
                for (auto& kv : objVal) {
                    out += nextInd + L"\"" + kv.first + L"\": " + kv.second.serialize(indent + 1);
                    if (++i < objVal.size()) out += L",";
                    out += L"\n";
                }
                out += ind + L"}";
                return out;
            }
        }
        return L"null";
    }
};

class JsonParser {
    std::wstring src;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == L' ' || src[pos] == L'\t' || src[pos] == L'\n' || src[pos] == L'\r')) {
            pos++;
        }
    }

    wchar_t peek() { skipWhitespace(); return pos < src.size() ? src[pos] : 0; }
    wchar_t get() { skipWhitespace(); return pos < src.size() ? src[pos++] : 0; }

    std::wstring parseString() {
        if (get() != L'\"') return L"";
        std::wstring res;
        while (pos < src.size()) {
            wchar_t c = src[pos++];
            if (c == L'\"') break;
            if (c == L'\\' && pos < src.size()) {
                wchar_t esc = src[pos++];
                if (esc == L'\"') res += L'\"';
                else if (esc == L'\\') res += L'\\';
                else if (esc == L'/') res += L'/';
                else if (esc == L'n') res += L'\n';
                else if (esc == L'r') res += L'\r';
                else if (esc == L't') res += L'\t';
                else if (esc == L'u' && pos + 4 <= src.size()) {
                    std::wstring hex = src.substr(pos, 4);
                    pos += 4;
                    res += (wchar_t)wcstol(hex.c_str(), NULL, 16);
                } else res += esc;
            } else {
                res += c;
            }
        }
        return res;
    }

    JsonValue parseNumber() {
        skipWhitespace();
        size_t start = pos;
        if (pos < src.size() && (src[pos] == L'-' || src[pos] == L'+')) pos++;
        while (pos < src.size() && (iswdigit(src[pos]) || src[pos] == L'.')) pos++;
        std::wstring sub = src.substr(start, pos - start);
        return JsonValue::FromNumber(wcstod(sub.c_str(), NULL));
    }

public:
    JsonParser(const std::wstring& s) : src(s) {}

    JsonValue parseValue() {
        skipWhitespace();
        wchar_t c = peek();
        if (c == L'\"') {
            return JsonValue::FromString(parseString());
        } else if (c == L'{') {
            get(); // eat '{'
            JsonValue obj = JsonValue::Object();
            skipWhitespace();
            if (peek() == L'}') { get(); return obj; }
            while (pos < src.size()) {
                std::wstring key = parseString();
                skipWhitespace();
                if (get() != L':') break;
                JsonValue val = parseValue();
                obj.set(key, val);
                skipWhitespace();
                wchar_t sep = get();
                if (sep == L'}') break;
                if (sep != L',') break;
            }
            return obj;
        } else if (c == L'[') {
            get(); // eat '['
            JsonValue arr = JsonValue::Array();
            skipWhitespace();
            if (peek() == L']') { get(); return arr; }
            while (pos < src.size()) {
                arr.arrVal.push_back(parseValue());
                skipWhitespace();
                wchar_t sep = get();
                if (sep == L']') break;
                if (sep != L',') break;
            }
            return arr;
        } else if (c == L't' || c == L'f') {
            std::wstring token;
            while (pos < src.size() && iswalpha(src[pos])) token += src[pos++];
            return JsonValue::FromBool(token == L"true");
        } else if (c == L'n') {
            while (pos < src.size() && iswalpha(src[pos])) pos++;
            return JsonValue::Null();
        } else if (c == L'-' || iswdigit(c)) {
            return parseNumber();
        }
        return JsonValue::Null();
    }
};
