////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include <JavaScriptCore/JavaScriptCore.h>
#include <string>
#include <stdexcept>

template<typename T>
inline void RJSFinalize(JSObjectRef object) {
    delete static_cast<T>(JSObjectGetPrivate(object));
    JSObjectSetPrivate(object, NULL);
}

template<typename T>
inline JSObjectRef RJSWrapObject(JSContextRef ctx, JSClassRef jsClass, T object, JSValueRef prototype = NULL) {
    JSObjectRef ref = JSObjectMake(ctx, jsClass, (void *)object);
    if (prototype) {
        JSObjectSetPrototype(ctx, ref, prototype);
    }
    return ref;
}

template<typename T>
inline T RJSGetInternal(JSObjectRef jsObject) {
    return static_cast<T>(JSObjectGetPrivate(jsObject));
}

template<typename T>
JSClassRef RJSCreateWrapperClass(const char * name, JSObjectGetPropertyCallback getter = NULL, JSObjectSetPropertyCallback setter = NULL, const JSStaticFunction *funcs = NULL, JSObjectFinalizeCallback finalize = RJSFinalize<T>, JSObjectGetPropertyNamesCallback propertyNames = NULL) {
    JSClassDefinition classDefinition = kJSClassDefinitionEmpty;
    classDefinition.className = name;
    classDefinition.finalize = finalize;
    classDefinition.getProperty = getter;
    classDefinition.setProperty = setter;
    classDefinition.staticFunctions = funcs;
    classDefinition.getPropertyNames = propertyNames;
    return JSClassCreate(&classDefinition);
}

void RJSRegisterGlobalClass(JSContextRef ctx, JSObjectRef globalObject, JSClassRef classRef, const char * name, JSValueRef *exception);

std::string RJSStringForJSString(JSStringRef jsString);
std::string RJSValidatedStringForValue(JSContextRef ctx, JSValueRef value, const char * name = nullptr);

JSStringRef RJSStringForString(const std::string &str);
JSValueRef RJSValueForString(JSContextRef ctx, const std::string &str);

inline void RJSValidateArgumentCount(size_t argumentCount, size_t expected) {
    if (argumentCount != expected) {
        throw std::invalid_argument("Invalid arguments");
    }
}

inline void RJSValidateArgumentRange(size_t argumentCount, size_t min, size_t max) {
    if (argumentCount < min || argumentCount > max) {
        throw std::invalid_argument("Invalid arguments");
    }
}

class RJSException : public std::runtime_error {
public:
    RJSException(JSContextRef ctx, JSValueRef &ex) : std::runtime_error(RJSValidatedStringForValue(ctx, ex, "exception")),
        m_jsException(ex) {}
    JSValueRef exception() { return m_jsException; }

private:
    JSValueRef m_jsException;
};

JSValueRef RJSMakeError(JSContextRef ctx, RJSException &exp);
JSValueRef RJSMakeError(JSContextRef ctx, std::exception &exp);
JSValueRef RJSMakeError(JSContextRef ctx, const std::string &message);

static inline JSObjectRef RJSValidatedValueToObject(JSContextRef ctx, JSValueRef value, const char *message = NULL) {
    JSObjectRef object = JSValueToObject(ctx, value, NULL);
    if (!object) {
        throw std::runtime_error(message ?: "Value is not an object.");
    }
    return object;
}

static inline double RJSValidatedValueToNumber(JSContextRef ctx, JSValueRef value) {
    JSValueRef exception = NULL;
    if (!JSValueIsNumber(ctx, value)) {
        throw std::runtime_error("Value is not a number");
    }
    double number = JSValueToNumber(ctx, value, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return number;
}

static inline JSValueRef RJSValidatedPropertyValue(JSContextRef ctx, JSObjectRef object, JSStringRef property) {
    JSValueRef exception = NULL;
    JSValueRef propertyValue = JSObjectGetProperty(ctx, object, property, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return propertyValue;
}

static inline JSObjectRef RJSValidatedObjectProperty(JSContextRef ctx, JSObjectRef object, JSStringRef property, const char *err = NULL) {
    JSValueRef propertyValue = RJSValidatedPropertyValue(ctx, object, property);
    if (JSValueIsUndefined(ctx, propertyValue)) {
        throw std::runtime_error(err ?: "Object property is undefined");
    }
    return RJSValidatedValueToObject(ctx, propertyValue, err);
}

static inline JSObjectRef RJSValidatedObjectAtIndex(JSContextRef ctx, JSObjectRef object, unsigned int index) {
    JSValueRef exception = NULL;
    JSValueRef objectValue = JSObjectGetPropertyAtIndex(ctx, object, index, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return RJSValidatedValueToObject(ctx, objectValue);
}

static inline std::string RJSValidatedStringProperty(JSContextRef ctx, JSObjectRef object, JSStringRef property) {
    JSValueRef exception = NULL;
    JSValueRef propertyValue = JSObjectGetProperty(ctx, object, property, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return RJSValidatedStringForValue(ctx, propertyValue);
}

static inline size_t RJSValidatedArrayLength(JSContextRef ctx, JSObjectRef object) {
    JSValueRef exception = NULL;
    static JSStringRef lengthString = JSStringCreateWithUTF8CString("length");
    JSValueRef lengthValue = JSObjectGetProperty(ctx, object, lengthString, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    if (!JSValueIsNumber(ctx, lengthValue)) {
        throw std::runtime_error("Missing property 'length'");
    }

    return RJSValidatedValueToNumber(ctx, lengthValue);
}

static inline bool RJSIsValueObjectOfType(JSContextRef ctx, JSValueRef value, JSStringRef type) {
    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);

    JSValueRef exception = NULL;
    JSValueRef constructorValue = JSObjectGetProperty(ctx, globalObject, type, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }

    bool ret = JSValueIsInstanceOfConstructor(ctx, value, RJSValidatedValueToObject(ctx, constructorValue), &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }

    return ret;
}

bool RJSIsValueArray(JSContextRef ctx, JSValueRef value);

