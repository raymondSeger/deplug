#include "private/variant.hpp"
#include "private/stream_buffer.hpp"
#include "plugkit_module.hpp"
#include "extended_slot.hpp"
#include "slice.hpp"
#include <iomanip>
#include <mutex>
#include <nan.h>
#include <sstream>
#include <unordered_map>
#include <uv.h>

namespace plugkit {

namespace {

const Variant::Array nullArray;
const Variant::Map nullMap;

bool hasBuffer(const Variant &var) {
  switch (var.type()) {
  case Variant::TYPE_STRING:
  case Variant::TYPE_BUFFER:
    return true;
  default:
    return false;
  }
}

struct SharedBufferStore {
  uv_rwlock_t rwlock;
  std::unordered_map<const char *, std::weak_ptr<std::string>> map;

  SharedBufferStore() { uv_rwlock_init(&rwlock); }

  ~SharedBufferStore() { uv_rwlock_destroy(&rwlock); }

  std::shared_ptr<std::string> find(const char *addr) {
    uv_rwlock_rdlock(&rwlock);
    std::shared_ptr<std::string> buf;
    auto it = map.find(addr);
    if (it != map.end()) {
      buf = it->second.lock();
    }
    uv_rwlock_rdunlock(&rwlock);
    return buf;
  }

  void add(const std::shared_ptr<std::string> &buf) {
    uv_rwlock_wrlock(&rwlock);
    map[buf->data()] = buf;
    uv_rwlock_wrunlock(&rwlock);
  }

  void cleanup() {
    uv_rwlock_wrlock(&rwlock);
    for (auto it = map.begin(); it != map.end(); ++it) {
      if (!it->second.lock()) {
        it = map.erase(it);
      }
    }
    uv_rwlock_wrunlock(&rwlock);
  }
};

SharedBufferStore bufferStore;
}

void Variant::Private::init(v8::Isolate *isolate) {
  PlugkitModule *module = ExtendedSlot::get<PlugkitModule>(
      isolate, ExtendedSlot::SLOT_PLUGKIT_MODULE);

  auto script =
      Nan::CompileScript(Nan::New("(function (buf) { return "
                                  "require('buffer').Buffer.from(buf) })")
                             .ToLocalChecked());
  auto func = Nan::RunScript(script.ToLocalChecked())
                  .ToLocalChecked()
                  .As<v8::Function>();
  module->arrayToBuffer.Reset(isolate, func);
}

Variant::Variant() : type_(TYPE_NIL) {}

Variant::Variant(bool value) : type_(TYPE_BOOL) { d.bool_ = value; }

Variant::Variant(int8_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(uint8_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(int16_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(uint16_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(int32_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(uint32_t value) : Variant(static_cast<double>(value)) {}

Variant::Variant(int64_t value) : type_(TYPE_INT64) { d.int_ = value; }

Variant::Variant(uint64_t value) : type_(TYPE_UINT64) { d.uint_ = value; }

Variant::Variant(double value) : type_(TYPE_DOUBLE) { d.double_ = value; }

Variant::Variant(const std::string &str) : type_(TYPE_STRING) {
  d.slice = new Slice(std::make_shared<std::string>(str));
}

Variant::Variant(std::string &&str) : type_(TYPE_STRING) {
  d.slice = new Slice(std::make_shared<std::string>(str));
}

Variant::Variant(const Array &array) : type_(TYPE_ARRAY) {
  d.array = new Array(array);
}

Variant::Variant(const Map &map) : type_(TYPE_MAP) { d.map = new Map(map); }

Variant::Variant(Array &&array) : type_(TYPE_ARRAY) {
  d.array = new Array(array);
}

Variant::Variant(Map &&map) : type_(TYPE_MAP) { d.map = new Map(map); }

Variant::Variant(const Slice &slice) : type_(TYPE_BUFFER) {
  d.slice = new Slice(slice);
}

Variant::Variant(const StreamBuffer &stream) : type_(TYPE_STREAM) {
  d.uint_ = stream.id();
}

Variant::Variant(const Timestamp &ts) : type_(TYPE_TIMESTAMP) {
  d.ts = new Timestamp(ts);
}

Variant::~Variant() {
  switch (type()) {
  case Variant::TYPE_TIMESTAMP:
    delete d.ts;
    break;
  case Variant::TYPE_STRING:
  case Variant::TYPE_BUFFER:
    delete d.slice;
    break;
  case Variant::TYPE_ARRAY:
    delete d.array;
    break;
  case Variant::TYPE_MAP:
    delete d.map;
    break;
  default:;
  }
}

Variant::Variant(const Variant &value) { *this = value; }

Variant &Variant::operator=(const Variant &value) {
  this->type_ = value.type_;
  this->d = value.d;
  switch (this->type_) {
  case Variant::TYPE_TIMESTAMP:
    this->d.ts = new Timestamp(*value.d.ts);
    break;
  case Variant::TYPE_STRING:
  case Variant::TYPE_BUFFER:
    this->d.slice = new Slice(*value.d.slice);
    break;
  case Variant::TYPE_ARRAY:
    this->d.array = new Array(*value.d.array);
    break;
  case Variant::TYPE_MAP:
    this->d.map = new Map(*value.d.map);
    break;
  default:;
  }
  return *this;
}

void Variant::Private::cleanupSharedBuffers() { bufferStore.cleanup(); }

Variant::Type Variant::type() const { return static_cast<Type>(type_); }

bool Variant::isNil() const { return type() == TYPE_NIL; }

bool Variant::isBool() const { return type() == TYPE_BOOL; }

bool Variant::boolValue(bool defaultValue) const {
  switch (type_) {
  case TYPE_BOOL:
    return d.bool_;
  case TYPE_INT64:
    return d.int_;
  case TYPE_UINT64:
    return d.uint_;
  case TYPE_DOUBLE:
    return d.double_;
  default:
    return defaultValue;
  }
}

bool Variant::isInt64() const { return type() == TYPE_INT64; }

int64_t Variant::int64Value(int64_t defaultValue) const {
  switch (type_) {
  case TYPE_BOOL:
    return d.bool_;
  case TYPE_INT64:
    return d.int_;
  case TYPE_UINT64:
    return d.uint_;
  case TYPE_DOUBLE:
    return d.double_;
  default:
    return defaultValue;
  }
}

bool Variant::isUint64() const { return type() == TYPE_UINT64; }

uint64_t Variant::uint64Value(uint64_t defaultValue) const {
  switch (type_) {
  case TYPE_BOOL:
    return d.bool_;
  case TYPE_INT64:
    return d.int_;
  case TYPE_UINT64:
  case TYPE_STREAM:
    return d.uint_;
  case TYPE_DOUBLE:
    return d.double_;
  default:
    return defaultValue;
  }
}

bool Variant::isDouble() const { return type() == TYPE_DOUBLE; }

double Variant::doubleValue(double defaultValue) const {
  switch (type_) {
  case TYPE_BOOL:
    return d.bool_;
  case TYPE_INT64:
    return d.int_;
  case TYPE_UINT64:
  case TYPE_STREAM:
    return d.uint_;
  case TYPE_DOUBLE:
    return d.double_;
  default:
    return defaultValue;
  }
}

std::string Variant::string(const std::string &defaultValue) const {
  if (hasBuffer(*this)) {
    return *d.slice->buffer();
  } else {
    switch (type()) {
    case TYPE_BOOL:
      return boolValue() ? "true" : "false";
    case TYPE_INT64:
      return std::to_string(int64Value());
    case TYPE_UINT64:
    case TYPE_STREAM:
      return std::to_string(uint64Value());
    case TYPE_DOUBLE:
      return std::to_string(doubleValue());
    case TYPE_TIMESTAMP: {
      auto tp = std::chrono::time_point_cast<std::chrono::seconds>(timestamp());
      std::time_t ts = std::chrono::system_clock::to_time_t(tp);
      std::stringstream stream;
      stream << std::put_time(std::localtime(&ts), "%FT%T%z");
      return stream.str();
    }
    default:
      return defaultValue;
    }
  }
}

Variant::Timestamp Variant::timestamp(const Timestamp &defaultValue) const {
  if (isTimestamp()) {
    return *d.ts;
  } else {
    return defaultValue;
  }
}

Slice Variant::slice() const {
  if (hasBuffer(*this)) {
    return *d.slice;
  } else {
    return Slice();
  }
}

StreamBuffer Variant::stream() const {
  if (isStream()) {
    return StreamBuffer::Private::create(uint64Value());
  } else {
    return StreamBuffer();
  }
}

const Variant::Array &Variant::array() const {
  if (type() == TYPE_ARRAY) {
    return *d.array;
  } else {
    return nullArray;
  }
}

const Variant::Map &Variant::map() const {
  if (type() == TYPE_MAP) {
    return *d.map;
  } else {
    return nullMap;
  }
}

Variant Variant::operator[](size_t index) const {
  if (type() == TYPE_ARRAY && index < d.array->size()) {
    return (*d.array)[index];
  } else {
    return Variant();
  }
}

Variant Variant::operator[](const std::string &key) const {
  if (type() == TYPE_MAP) {
    auto it = d.map->find(key);
    if (it != d.map->end()) {
      return it->second;
    }
  }
  return Variant();
}

size_t Variant::length() const {
  if (type() == TYPE_ARRAY) {
    return d.array->size();
  } else if (type() == TYPE_MAP) {
    return d.map->size();
  }
  return 0;
}

uint8_t Variant::tag() const { return tag_; }

bool Variant::isString() const { return type() == TYPE_STRING; }

bool Variant::isTimestamp() const { return type() == TYPE_TIMESTAMP; }

bool Variant::isBuffer() const { return type() == TYPE_BUFFER; }

bool Variant::isStream() const { return type() == TYPE_STREAM; }

v8::Local<v8::Object> Variant::Private::getNodeBuffer(const Slice &slice) {
  using namespace v8;

  Isolate *isolate = Isolate::GetCurrent();
  Slice::Buffer buffer = slice.buffer();
  if (!buffer) {
    buffer = std::make_shared<std::string>();
  }

  auto abuf = ArrayBuffer::New(isolate, const_cast<char *>(buffer->data()),
                               buffer->size());
  auto array = Uint8Array::New(abuf, slice.offset(), slice.size());
  if (!isolate->GetData(1)) { // Node.js is not installed
    return array;
  }

  PlugkitModule *module = ExtendedSlot::get<PlugkitModule>(
      isolate, ExtendedSlot::SLOT_PLUGKIT_MODULE);

  auto func = v8::Local<v8::Function>::New(isolate, module->arrayToBuffer);

  v8::Local<v8::Value> args[1] = {array};
  auto nodeBufObj = func->Call(func, 1, args);
  auto nodeBuf = nodeBufObj.As<v8::Object>();
  nodeBuf->Set(Nan::New("dataOffset").ToLocalChecked(),
               Nan::New(static_cast<uint32_t>(slice.offset())));
  return nodeBuf;
}

Slice Variant::Private::getSlice(v8::Local<v8::Object> obj) {
  using namespace v8;

  if (!node::Buffer::HasInstance(obj)) {
    return Slice();
  }

  v8::Local<v8::Uint8Array> ui = obj.As<v8::Uint8Array>();
  v8::ArrayBuffer::Contents contents = ui->Buffer()->GetContents();

  if (auto shared =
          bufferStore.find(static_cast<const char *>(contents.Data()))) {
    return Slice(shared, ui->ByteOffset(), ui->ByteLength());
  } else {
    return Slice(node::Buffer::Data(obj), node::Buffer::Length(obj));
  }

  return Slice();
}

v8::Local<v8::Value> Variant::Private::getValue(const Variant &var) {
  switch (var.type()) {
  case TYPE_BOOL:
    return Nan::New(var.boolValue());
  case TYPE_INT64:
    return Nan::New(std::to_string(var.int64Value())).ToLocalChecked();
  case TYPE_UINT64:
    return Nan::New(std::to_string(var.uint64Value())).ToLocalChecked();
  case TYPE_DOUBLE:
    return Nan::New(var.doubleValue());
  case TYPE_STRING:
    return Nan::New(var.string()).ToLocalChecked();
  case TYPE_TIMESTAMP: {
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    var.timestamp().time_since_epoch())
                    .count();
    auto date = v8::Date::New(isolate, nano / 1000000.0).As<v8::Object>();
    date->Set(Nan::New("nsec").ToLocalChecked(),
              Nan::New(static_cast<double>(nano % 1000000)));
    return date;
  }
  case TYPE_ARRAY: {
    const auto &array = var.array();
    auto obj = Nan::New<v8::Array>(array.size());
    for (size_t i = 0; i < array.size(); ++i) {
      obj->Set(i, getValue(array[i]));
    }
    return obj;
  }
  case TYPE_MAP: {
    const auto &map = var.map();
    auto obj = Nan::New<v8::Object>();
    for (const auto &pair : map) {
      obj->Set(Nan::New(pair.first).ToLocalChecked(), getValue(pair.second));
    }
    return obj;
  }
  case TYPE_BUFFER:
    return getNodeBuffer(var.slice());
  default:
    return Nan::Null();
  }
}

json11::Json Variant::Private::getJson(const Variant &var) {
  using namespace json11;
  Json::object json;
  switch (var.type()) {
  case Variant::TYPE_NIL:
    json["type"] = "nil";
    json["value"] = Json();
    break;
  case Variant::TYPE_BOOL:
    json["type"] = "bool";
    json["value"] = var.boolValue();
    break;
  case Variant::TYPE_INT64:
    json["type"] = "int64";
    json["value"] = var.string();
    break;
  case Variant::TYPE_UINT64:
    json["type"] = "uint64";
    json["value"] = var.string();
    break;
  case Variant::TYPE_DOUBLE:
    json["type"] = "double";
    json["value"] = var.doubleValue();
    break;
  case Variant::TYPE_STRING:
    json["type"] = "string";
    json["value"] = var.string();
    break;
  case Variant::TYPE_TIMESTAMP:
    json["type"] = "timestamp";
    json["value"] = var.string();
    break;
  case Variant::TYPE_BUFFER:
    json["type"] = "buffer";
    {
      std::stringstream stream;
      stream << std::hex << std::setfill('0') << std::setw(2);
      const auto &slice = var.slice();
      const uint8_t *data = reinterpret_cast<const uint8_t *>(slice.data());
      for (size_t i = 0; i < slice.size(); ++i) {
        stream << static_cast<int>(data[i]);
      }
      json["value"] = stream.str();
    }
    break;
  case Variant::TYPE_STREAM:
    json["type"] = "stream";
    json["value"] = var.string();
    break;
  case Variant::TYPE_ARRAY:
    json["type"] = "array";
    {
      Json::array array;
      for (const auto &item : var.array()) {
        array.push_back(getJson(item));
      }
      json["value"] = array;
    }
    break;
  case Variant::TYPE_MAP:
    json["type"] = "map";
    {
      Json::object object;
      for (const auto &pair : var.map()) {
        object[pair.first] = getJson(pair.second);
      }
      json["value"] = object;
    }
    break;
  }
  json["tag"] = var.tag();
  return json;
}

Variant Variant::Private::getVariant(v8::Local<v8::Value> var) {
  if (var->IsBoolean()) {
    return var->BooleanValue();
  } else if (var->IsNumber()) {
    return var->NumberValue();
  } else if (var->IsString()) {
    return std::string(*Nan::Utf8String(var));
  } else if (var->IsDate()) {
    auto nsec = var.As<v8::Object>()->Get(Nan::New("nsec").ToLocalChecked());
    uint64_t ts =
        static_cast<uint64_t>(var.As<v8::Date>()->ValueOf()) * 1000000;
    if (nsec->IsNumber()) {
      ts += var->NumberValue();
    }
    return Variant::Timestamp(std::chrono::nanoseconds(ts));
  } else if (node::Buffer::HasInstance(var)) {
    return getSlice(var.As<v8::Object>());
  } else if (var->IsArray()) {
    auto obj = var.As<v8::Array>();
    Variant::Array array;
    array.resize(obj->Length());
    for (size_t i = 0; i < obj->Length(); ++i) {
      array[i] = getVariant(obj->Get(i));
    }
    return array;
  } else if (var->IsObject()) {
    auto obj = var.As<v8::Object>();
    auto keys = obj->GetOwnPropertyNames();
    Variant::Map map;
    for (size_t i = 0; i < keys->Length(); ++i) {
      auto key = keys->Get(i);
      map[*Nan::Utf8String(key)] = getVariant(obj->Get(key));
    }
    return map;
  }
  return Variant();
}
}
