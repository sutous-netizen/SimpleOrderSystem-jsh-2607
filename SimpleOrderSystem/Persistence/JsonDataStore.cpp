#include "JsonDataStore.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Persistence {

namespace {

// ---------------------------------------------------------------------
// 최소 JSON 값 트리 (외부 라이브러리 미사용).
// 이 프로젝트의 스키마(배열 안에 평평한 객체들)만 지원하면 충분하므로
// Null/Bool/Number/String/Array/Object 6종만 다룬다.
// ---------------------------------------------------------------------

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>; // 순서 보존

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    JsonArray arrayValue;
    JsonObject objectValue;

    static JsonValue MakeString(std::string s) {
        JsonValue v;
        v.type = JsonType::String;
        v.stringValue = std::move(s);
        return v;
    }
    static JsonValue MakeNumber(double d) {
        JsonValue v;
        v.type = JsonType::Number;
        v.numberValue = d;
        return v;
    }
    static JsonValue MakeArray() {
        JsonValue v;
        v.type = JsonType::Array;
        return v;
    }
    static JsonValue MakeObject() {
        JsonValue v;
        v.type = JsonType::Object;
        return v;
    }

    const JsonValue* Find(const std::string& key) const {
        if (type != JsonType::Object) return nullptr;
        for (const auto& kv : objectValue) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    std::string AsString(const std::string& defaultValue = "") const {
        if (type == JsonType::String) return stringValue;
        return defaultValue;
    }

    int64_t AsInt64(int64_t defaultValue = 0) const {
        if (type == JsonType::Number) return static_cast<int64_t>(numberValue);
        return defaultValue;
    }

    double AsDouble(double defaultValue = 0.0) const {
        if (type == JsonType::Number) return numberValue;
        return defaultValue;
    }
};

// ---- 파서 ----

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text), pos_(0) {}

    JsonValue Parse() {
        SkipWhitespace();
        JsonValue v = ParseValue();
        SkipWhitespace();
        return v;
    }

private:
    const std::string& text_;
    size_t pos_;

    char Peek() const {
        if (pos_ >= text_.size()) throw std::runtime_error("JSON parse error: unexpected end of input");
        return text_[pos_];
    }

    char Next() {
        if (pos_ >= text_.size()) throw std::runtime_error("JSON parse error: unexpected end of input");
        return text_[pos_++];
    }

    void SkipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    void Expect(char c) {
        if (Peek() != c) {
            std::ostringstream oss;
            oss << "JSON parse error: expected '" << c << "' at position " << pos_;
            throw std::runtime_error(oss.str());
        }
        ++pos_;
    }

    JsonValue ParseValue() {
        SkipWhitespace();
        char c = Peek();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return JsonValue::MakeString(ParseRawString());
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        return ParseNumber();
    }

    JsonValue ParseObject() {
        JsonValue obj = JsonValue::MakeObject();
        Expect('{');
        SkipWhitespace();
        if (Peek() == '}') {
            ++pos_;
            return obj;
        }
        while (true) {
            SkipWhitespace();
            std::string key = ParseRawString();
            SkipWhitespace();
            Expect(':');
            SkipWhitespace();
            JsonValue value = ParseValue();
            obj.objectValue.emplace_back(std::move(key), std::move(value));
            SkipWhitespace();
            char c = Next();
            if (c == ',') continue;
            if (c == '}') break;
            throw std::runtime_error("JSON parse error: expected ',' or '}' in object");
        }
        return obj;
    }

    JsonValue ParseArray() {
        JsonValue arr = JsonValue::MakeArray();
        Expect('[');
        SkipWhitespace();
        if (Peek() == ']') {
            ++pos_;
            return arr;
        }
        while (true) {
            SkipWhitespace();
            arr.arrayValue.push_back(ParseValue());
            SkipWhitespace();
            char c = Next();
            if (c == ',') continue;
            if (c == ']') break;
            throw std::runtime_error("JSON parse error: expected ',' or ']' in array");
        }
        return arr;
    }

    std::string ParseRawString() {
        Expect('"');
        std::string result;
        while (true) {
            char c = Next();
            if (c == '"') break;
            if (c == '\\') {
                char esc = Next();
                switch (esc) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'n': result.push_back('\n'); break;
                    case 't': result.push_back('\t'); break;
                    case 'r': result.push_back('\r'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    default: result.push_back(esc); break;
                }
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    JsonValue ParseBool() {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            JsonValue v;
            v.type = JsonType::Bool;
            v.boolValue = true;
            return v;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            JsonValue v;
            v.type = JsonType::Bool;
            v.boolValue = false;
            return v;
        }
        throw std::runtime_error("JSON parse error: invalid literal");
    }

    JsonValue ParseNull() {
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return JsonValue{};
        }
        throw std::runtime_error("JSON parse error: invalid literal");
    }

    JsonValue ParseNumber() {
        size_t start = pos_;
        if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
        while (pos_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[pos_])) ||
                                        text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E' ||
                                        text_[pos_] == '-' || text_[pos_] == '+')) {
            ++pos_;
        }
        std::string numText = text_.substr(start, pos_ - start);
        if (numText.empty()) throw std::runtime_error("JSON parse error: invalid number");
        return JsonValue::MakeNumber(std::stod(numText));
    }
};

JsonValue ParseJson(const std::string& text) {
    JsonParser parser(text);
    return parser.Parse();
}

// ---- 라이터 ----

std::string EscapeJsonString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            default: result.push_back(c); break;
        }
    }
    return result;
}

// ---- 파일 I/O ----

std::string ReadFileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

void WriteFileTextAtomic(const std::string& path, const std::string& content) {
    // 원자성을 최대한 보장하기 위해 임시 파일에 먼저 쓰고 rename 한다.
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("파일을 열 수 없습니다: " + tmpPath);
        }
        out << content;
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        // rename 실패 시(예: 이미 존재하는 파일에 대한 플랫폼 제약) 복사 후 삭제로 대체한다.
        std::filesystem::copy_file(tmpPath, path, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath);
        if (ec) {
            throw std::runtime_error("파일 저장에 실패했습니다: " + path);
        }
    }
}

// ---- Sample <-> JSON ----

JsonValue SampleToJson(const Model::Sample& s) {
    JsonValue obj = JsonValue::MakeObject();
    obj.objectValue.emplace_back("id", JsonValue::MakeString(s.id));
    obj.objectValue.emplace_back("name", JsonValue::MakeString(s.name));
    obj.objectValue.emplace_back("avgProductionTimeMin", JsonValue::MakeNumber(s.avgProductionTimeMin));
    obj.objectValue.emplace_back("yieldRate", JsonValue::MakeNumber(s.yieldRate));
    obj.objectValue.emplace_back("stock", JsonValue::MakeNumber(static_cast<double>(s.stock)));
    return obj;
}

Model::Sample SampleFromJson(const JsonValue& obj) {
    Model::Sample s;
    if (auto* v = obj.Find("id")) s.id = v->AsString();
    if (auto* v = obj.Find("name")) s.name = v->AsString();
    if (auto* v = obj.Find("avgProductionTimeMin")) s.avgProductionTimeMin = v->AsDouble();
    if (auto* v = obj.Find("yieldRate")) s.yieldRate = v->AsDouble(1.0);
    if (auto* v = obj.Find("stock")) s.stock = v->AsInt64();
    return s;
}

// ---- Order <-> JSON ----

JsonValue OrderToJson(const Model::Order& o) {
    JsonValue obj = JsonValue::MakeObject();
    obj.objectValue.emplace_back("orderNo", JsonValue::MakeString(o.orderNo));
    obj.objectValue.emplace_back("sampleId", JsonValue::MakeString(o.sampleId));
    obj.objectValue.emplace_back("customerName", JsonValue::MakeString(o.customerName));
    obj.objectValue.emplace_back("quantity", JsonValue::MakeNumber(static_cast<double>(o.quantity)));
    obj.objectValue.emplace_back("status", JsonValue::MakeString(Model::ToString(o.status)));
    obj.objectValue.emplace_back("createdAt", JsonValue::MakeString(o.createdAt));
    obj.objectValue.emplace_back("updatedAt", JsonValue::MakeString(o.updatedAt));
    obj.objectValue.emplace_back("shortage", JsonValue::MakeNumber(static_cast<double>(o.shortage)));
    obj.objectValue.emplace_back("actualProductionQty", JsonValue::MakeNumber(static_cast<double>(o.actualProductionQty)));
    obj.objectValue.emplace_back("totalProductionTimeMin", JsonValue::MakeNumber(o.totalProductionTimeMin));
    return obj;
}

Model::Order OrderFromJson(const JsonValue& obj) {
    Model::Order o;
    if (auto* v = obj.Find("orderNo")) o.orderNo = v->AsString();
    if (auto* v = obj.Find("sampleId")) o.sampleId = v->AsString();
    if (auto* v = obj.Find("customerName")) o.customerName = v->AsString();
    if (auto* v = obj.Find("quantity")) o.quantity = v->AsInt64();
    if (auto* v = obj.Find("status")) o.status = Model::OrderStatusFromString(v->AsString("RESERVED"));
    if (auto* v = obj.Find("createdAt")) o.createdAt = v->AsString();
    if (auto* v = obj.Find("updatedAt")) o.updatedAt = v->AsString();
    if (auto* v = obj.Find("shortage")) o.shortage = v->AsInt64();
    if (auto* v = obj.Find("actualProductionQty")) o.actualProductionQty = v->AsInt64();
    if (auto* v = obj.Find("totalProductionTimeMin")) o.totalProductionTimeMin = v->AsDouble();
    return o;
}

// JsonValue(Object) -> 한 줄 문자열. 배열 안에서 들여쓰기해 사용.
std::string WriteObjectLine(const JsonValue& obj, const std::string& indent) {
    std::ostringstream oss;
    oss << indent << "{";
    for (size_t i = 0; i < obj.objectValue.size(); ++i) {
        const auto& kv = obj.objectValue[i];
        oss << "\"" << EscapeJsonString(kv.first) << "\": ";
        const JsonValue& v = kv.second;
        switch (v.type) {
            case JsonType::String:
                oss << "\"" << EscapeJsonString(v.stringValue) << "\"";
                break;
            case JsonType::Number: {
                // 정수 값은 정수로, 소수는 왕복 손실이 없도록 충분한 정밀도로 출력한다.
                double d = v.numberValue;
                if (d == static_cast<int64_t>(d)) {
                    oss << static_cast<int64_t>(d);
                } else {
                    std::ostringstream numOss;
                    numOss.precision(17);
                    numOss << d;
                    oss << numOss.str();
                }
                break;
            }
            case JsonType::Bool:
                oss << (v.boolValue ? "true" : "false");
                break;
            default:
                oss << "null";
                break;
        }
        if (i + 1 < obj.objectValue.size()) oss << ", ";
    }
    oss << "}";
    return oss.str();
}

std::string WriteArrayOfObjects(const std::vector<JsonValue>& objects) {
    std::ostringstream oss;
    oss << "[\n";
    for (size_t i = 0; i < objects.size(); ++i) {
        oss << WriteObjectLine(objects[i], "  ");
        if (i + 1 < objects.size()) oss << ",";
        oss << "\n";
    }
    oss << "]\n";
    return oss.str();
}

// 파일에서 최상위 배열을 읽어들인다. 파일이 없거나 비어있으면 빈 배열로 취급한다.
JsonArray LoadJsonArrayFromFile(const std::string& path) {
    std::string text = ReadFileText(path);
    // 앞뒤 공백만 있는 경우도 빈 것으로 취급
    bool blank = std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
    if (text.empty() || blank) {
        return JsonArray{};
    }
    JsonValue root = ParseJson(text);
    if (root.type != JsonType::Array) {
        throw std::runtime_error("JSON 파일의 최상위 값이 배열이 아닙니다: " + path);
    }
    return root.arrayValue;
}

std::string ExtractDateFromOrderNo(const std::string& orderNo) {
    // 형식: ORD-YYYYMMDD-NNNN
    // "ORD-" (4자) 다음 8자가 날짜
    if (orderNo.size() < 4 + 8) return "";
    return orderNo.substr(4, 8);
}

int ExtractSeqFromOrderNo(const std::string& orderNo) {
    size_t lastDash = orderNo.find_last_of('-');
    if (lastDash == std::string::npos) return 0;
    std::string seqText = orderNo.substr(lastDash + 1);
    try {
        return std::stoi(seqText);
    } catch (...) {
        return 0;
    }
}

} // namespace

JsonDataStore::JsonDataStore(std::string samplesPath, std::string ordersPath)
    : samplesPath_(std::move(samplesPath)), ordersPath_(std::move(ordersPath)) {}

std::vector<Model::Sample> JsonDataStore::LoadSamples() const {
    JsonArray arr = LoadJsonArrayFromFile(samplesPath_);
    std::vector<Model::Sample> samples;
    samples.reserve(arr.size());
    for (const auto& item : arr) {
        samples.push_back(SampleFromJson(item));
    }
    return samples;
}

void JsonDataStore::SaveSamples(const std::vector<Model::Sample>& samples) {
    std::vector<JsonValue> arr;
    arr.reserve(samples.size());
    for (const auto& s : samples) {
        arr.push_back(SampleToJson(s));
    }
    WriteFileTextAtomic(samplesPath_, WriteArrayOfObjects(arr));
}

std::optional<Model::Sample> JsonDataStore::FindSampleById(const std::string& sampleId) const {
    std::vector<Model::Sample> samples = LoadSamples();
    for (const auto& s : samples) {
        if (s.id == sampleId) return s;
    }
    return std::nullopt;
}

std::vector<Model::Order> JsonDataStore::LoadOrders() const {
    JsonArray arr = LoadJsonArrayFromFile(ordersPath_);
    std::vector<Model::Order> orders;
    orders.reserve(arr.size());
    for (const auto& item : arr) {
        orders.push_back(OrderFromJson(item));
    }
    return orders;
}

void JsonDataStore::SaveOrders(const std::vector<Model::Order>& orders) {
    std::vector<JsonValue> arr;
    arr.reserve(orders.size());
    for (const auto& o : orders) {
        arr.push_back(OrderToJson(o));
    }
    WriteFileTextAtomic(ordersPath_, WriteArrayOfObjects(arr));
}

std::optional<Model::Order> JsonDataStore::FindOrderByNo(const std::string& orderNo) const {
    std::vector<Model::Order> orders = LoadOrders();
    for (const auto& o : orders) {
        if (o.orderNo == orderNo) return o;
    }
    return std::nullopt;
}

std::string JsonDataStore::NextOrderNo(const std::string& yyyymmdd) {
    std::vector<Model::Order> orders = LoadOrders();
    int maxSeq = 0;
    for (const auto& o : orders) {
        if (ExtractDateFromOrderNo(o.orderNo) == yyyymmdd) {
            int seq = ExtractSeqFromOrderNo(o.orderNo);
            maxSeq = std::max(maxSeq, seq);
        }
    }
    int nextSeq = maxSeq + 1;
    std::ostringstream oss;
    oss << "ORD-" << yyyymmdd << "-";
    oss.width(4);
    oss.fill('0');
    oss << nextSeq;
    return oss.str();
}

} // namespace Persistence
