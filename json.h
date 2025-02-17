#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <fstream>

// 前向声明
class JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

// JsonValue 类，用于表示 JSON 值
class JsonValue {
public:
    using Value = std::variant<std::nullptr_t, bool, int, double, std::string, JsonArray, JsonObject>;

    JsonValue() : value(nullptr) {}
    JsonValue(std::nullptr_t) : value(nullptr) {}
    JsonValue(bool b) : value(b) {}
    JsonValue(int i) : value(i) {}
    JsonValue(double d) : value(d) {}
    JsonValue(const std::string& s) : value(s) {}
    JsonValue(const JsonArray& arr) : value(arr) {}
    JsonValue(const JsonObject& obj) : value(obj) {}

    // 获取值的类型
    int type() const { return value.index(); }

    // 访问不同类型的值
    template <typename T>
    const T& get() const { return std::get<T>(value); }

    // 序列化方法
    std::string serialize() const;

private:
    Value value;
};

std::string JsonValue::serialize() const {
    // 把 JSON 值转换为字符串
    struct Visitor {
        std::string operator()(std::nullptr_t) const { return "null"; }
        std::string operator()(bool b) const { return b ? "true" : "false"; }
        std::string operator()(int i) const { return std::to_string(i); }
        std::string operator()(double d) const { return std::to_string(d); }

        std::string operator()(const std::string& s) const {
            return "\"" + s + "\"";
        }
        std::string operator()(const JsonArray& arr) const {
            std::string result = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) result += ", ";
                result += arr[i].serialize();
            }
            result += "]";
            return result;
        }
        std::string operator()(const JsonObject& obj) const {
            std::string result = "{";
            bool first = true;
            for (const auto& pair : obj) {
                if (!first) result += ", ";
                result += "\"" + pair.first + "\": " + pair.second.serialize();
                first = false;
            }
            result += "}";
            return result;
        }
    };

    return std::visit(Visitor{}, value);
}

// 跳过空白字符
inline void skipWhitespace(std::string_view json, size_t& pos) {
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
		pos++;
	}
}

inline JsonValue parse(std::string_view json, size_t& pos) {
    if (json.empty() || pos >= json.size()) {
        return JsonValue(std::nullptr_t{});
    }
    if (json[pos] == '{') {
        JsonObject obj;
        pos++;
        while (pos < json.size() && json[pos] != '}') {
            skipWhitespace(json, pos);
            if (json[pos] == '}') {
				pos++;
				return JsonValue(obj);
            }
            else if (json[pos] != '"') {
                throw std::invalid_argument("Invalid JSON string");
            }
            auto keyStart = pos + 1;
            while (json[pos] != ':') { 
                pos++;
                if (pos >= json.size() || json[pos] == '}') {
                    pos++;
                    return JsonValue(obj);
                }
            }
            std::string_view key = json.substr(keyStart, pos - keyStart - 1);
            pos++;
            skipWhitespace(json, pos);
            if(pos >= json.size()) return JsonValue(obj);
            JsonValue value = parse(json, pos);
            obj[std::string(key)] = value;
            if (json[pos] != ',') {
                skipWhitespace(json, pos);
                if (json[pos] == '}') {
                    pos++;
                    break;
                }
                else if (json[pos] == ',') {
                    pos++;
                    continue;
                }
                else {
                    throw std::invalid_argument("Invalid JSON string");
                }
            }
            else {
				pos++;
			}
        }
        return JsonValue(obj);
    }
    else if (json[pos] == '[') {
        JsonArray arr;
        pos++;
        while (pos < json.size() && json[pos] != ']') {
            skipWhitespace(json, pos);
            if (json[pos] == ']') {
                pos++;
                return JsonValue(arr);
            }
            arr.push_back(parse(json, pos));
            if (json[pos] != ',') {
                skipWhitespace(json, pos);
                if (json[pos] == ']') {
                    pos++;
                    break;
                }
                else if (json[pos] == ',') {
                    pos++;
                    continue;
                }
            }
            else {
                pos++;
            }
        }
        return JsonValue(arr);
    }
    else if (json[pos] == '"') {
        pos++;
        size_t start = pos;
        while (json[pos] != '"') {
            if (pos >= json.size()) throw std::invalid_argument("Invalid JSON string");
            pos++;
        }
        pos++;
        return JsonValue(std::string(json.substr(start, pos - start - 1)));
    }
    else if (json[pos] == 't') {
        pos++;
        return JsonValue(true);
    }
    else if (json[pos] == 'f') {
        pos++;
        return JsonValue(false);
    }
    else if (json[pos] == 'n') {
        pos++;
        return JsonValue(nullptr);
    }
    else if (json[pos] >= '0' && json[pos] <= '9') {
        auto begin = pos;
        bool isDouble = false;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '\n' && json[pos] != '\r' && json[pos] != '\t') {
            pos++;
            if (json[pos] == '.' || json[pos] == 'e') {
                isDouble = true;
            }
        }
        if(isDouble) {
            if (begin == pos - 1) {
                return JsonValue(std::stod(std::string(1, json[begin])));
            }
			return JsonValue(std::stod(std::string(json.substr(begin, pos- begin -1))));
		}
        if (begin == pos - 1) {
            return JsonValue(std::stoi(std::string(1, json[begin])));
        }
        return JsonValue(std::stoi(std::string(json.substr(begin, pos- begin - 1))));
    }
    else {
        if(pos >= json.size()) return JsonValue(nullptr);
        return parse(json, ++pos);
    }
    throw std::invalid_argument("Invalid JSON string");
}

inline JsonValue parse(std::string_view json) {
    if (json.empty()) return JsonValue(std::nullptr_t{});
    size_t pos = 1;
    if (json[0] == '{') {
        JsonObject obj;
        while (pos < json.size() && json[pos] != '}') {
            skipWhitespace(json, pos);
            if (json[pos] == '}') {
				pos++;
				return JsonValue(obj);
			}
			else if (json[pos] != '"') {
				throw std::invalid_argument("Invalid JSON string");
			}
            auto keyStart = pos + 1;
            while (json[pos] != ':') {
                pos++;
                if (pos >= json.size() || json[pos] == '}') {
                    pos++;
                    return JsonValue(obj);
                }
            }
            std::string_view key = json.substr(keyStart, pos - keyStart - 1);
            pos++;
            skipWhitespace(json, pos);
            if (pos >= json.size()) return JsonValue(obj);
            JsonValue value = parse(json, pos);
            obj[std::string(key)] = value;
            if (json[pos] != ',') {
                skipWhitespace(json, pos);
                if (json[pos] == '}') {
                    pos++;
                    break;
                }
                else if (json[pos] == ',') {
                    pos++;
                    continue;
                }
                else {
                    throw std::invalid_argument("Invalid JSON string");
                }
            }
            else {
                pos++;
            }
        }
        return JsonValue(obj);
    }
    else if (json[0] == '[') {
        JsonArray arr;
        while (pos < json.size() && json[pos] != ']') {
            skipWhitespace(json, pos);
            if(json[pos] == ']') {
				pos++;
				return JsonValue(arr);
			}
            arr.push_back(parse(json, pos));
            if (json[pos] != ',') {
                skipWhitespace(json, pos);
                if (json[pos] == ']') {
                    pos++;
                    break;
                }
                else if (json[pos] == ',') {
                    pos++;
                    continue;
                }
            }
            else {
                pos++;
            }
        }
        return JsonValue(arr);
    }
    else if (json[0] == '"') {
        pos++;
        while (json[pos] != '"') {
            pos++;
            if (pos >= json.size()) throw std::invalid_argument("Invalid JSON string");
        }
        pos++;
        return JsonValue(std::string(json.substr(1, pos - 1)));
    }
    else if (json[0] == 't') {
        return JsonValue(true);
    }
    else if (json[0] == 'f') {
        return JsonValue(false);
    }
    else if (json[0] == 'n') {
        return JsonValue(nullptr);
    }
    else if (json[0] >= '0' && json[0] <= '9') {
        bool isDouble = false;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '\n' && json[pos] != '\r' && json[pos] != '\t') {
            pos++;
            if (json[pos] == '.' || json[pos] == 'e') {
                isDouble = true;
            }
        }
        if (isDouble) {
            if(pos == 1) {
                return JsonValue(std::stod(std::string(1,json[0])));
            }
            return JsonValue(std::stod(std::string(json.substr(0, pos - 1))));
        }
        if(pos == 1) {
            return JsonValue(std::stoi(std::string(1,json[0])));
        }
        return JsonValue(std::stoi(std::string(json.substr(0, pos - 1))));
    }
    else {
        if (pos >= json.size()) {
            return JsonValue(nullptr);
        }
        return parse(json, ++pos);
    }
    throw std::invalid_argument("Invalid JSON string");
}