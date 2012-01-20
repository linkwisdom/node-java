
#include "utils.h"
#include <string.h>
#include <algorithm>
#include "javaObject.h"

std::list<jobject> javaReflectionGetMethods(JNIEnv *env, jclass clazz) {
  std::list<jobject> results;

  jclass clazzclazz = env->GetObjectClass(clazz);
  jmethodID methodId = env->GetMethodID(clazzclazz, "getMethods", "()[Ljava/lang/reflect/Method;");
  // TODO: filter out static and prive methods
  jobjectArray methodObjects = (jobjectArray)env->CallObjectMethod(clazz, methodId);
  jsize methodCount = env->GetArrayLength(methodObjects);
  for(jsize i=0; i<methodCount; i++) {
    jobject obj = env->GetObjectArrayElement(methodObjects, i);
    results.push_back(obj);
  }

  return results;
}

std::list<jobject> javaReflectionGetStaticMethods(JNIEnv *env, jclass clazz) {
  std::list<jobject> results;

  jclass clazzclazz = env->GetObjectClass(clazz);
  jmethodID methodId = env->GetMethodID(clazzclazz, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
  // TODO: filter out instance and prive methods
  jobjectArray methodObjects = (jobjectArray)env->CallObjectMethod(clazz, methodId);
  jsize methodCount = env->GetArrayLength(methodObjects);
  for(jsize i=0; i<methodCount; i++) {
    jobject obj = env->GetObjectArrayElement(methodObjects, i);
    results.push_back(obj);
  }

  return results;
}

std::list<jobject> javaReflectionGetConstructors(JNIEnv *env, jclass clazz) {
  std::list<jobject> results;

  jclass clazzclazz = env->GetObjectClass(clazz);
  jmethodID methodId = env->GetMethodID(clazzclazz, "getConstructors", "()[Ljava/lang/reflect/Constructor;");
  jobjectArray methodObjects = (jobjectArray)env->CallObjectMethod(clazz, methodId);
  jsize methodCount = env->GetArrayLength(methodObjects);
  for(jsize i=0; i<methodCount; i++) {
    jobject obj = env->GetObjectArrayElement(methodObjects, i);
    results.push_back(obj);
  }

  return results;
}

std::string javaToString(JNIEnv *env, jstring str) {
  const char* chars = env->GetStringUTFChars(str, NULL);
  std::string results = chars;
  env->ReleaseStringUTFChars(str, chars);
  return results;
}

std::string javaObjectToString(JNIEnv *env, jobject obj) {
  if(obj == NULL) {
    return "";
  }
  jclass objClazz = env->GetObjectClass(obj);
  jmethodID methodId = env->GetMethodID(objClazz, "toString", "()Ljava/lang/String;");
  jstring result = (jstring)env->CallObjectMethod(obj, methodId);
  return javaToString(env, result);
}

jobject javaFindBestMatchingMethod(
  JNIEnv *env,
  std::list<jobject>& methods,
  const char *methodName,
  std::list<int>& argTypes) {

  std::list<jobject> possibleMatches;
  jclass methodClazz = env->FindClass("java/lang/reflect/Method");
  jmethodID method_getNameMethod = env->GetMethodID(methodClazz, "getName", "()Ljava/lang/String;");
  jmethodID method_getParameterTypes = env->GetMethodID(methodClazz, "getParameterTypes", "()[Ljava/lang/Class;");

  for(std::list<jobject>::iterator it = methods.begin(); it != methods.end(); it++) {
    std::string itMethodName = javaToString(env, (jstring)env->CallObjectMethod(*it, method_getNameMethod));
    if(itMethodName == methodName) {
      jarray parameters = (jarray)env->CallObjectMethod(*it, method_getParameterTypes);
      if(env->GetArrayLength(parameters) == (jsize)argTypes.size()) {
        possibleMatches.push_back(*it);
      }
    }
  }

  if(possibleMatches.size() == 0) {
    return NULL;
  }
  if(possibleMatches.size() == 1) {
    return possibleMatches.front();
  } else {
    // TODO: argument match
    printf("javaFindBestMatchingMethod: multiple matches (choosing the first)\n");
    for(std::list<jobject>::iterator it = possibleMatches.begin(); it != possibleMatches.end(); it++) {
      printf("  %s\n", javaObjectToString(env, *it).c_str());
    }
    return possibleMatches.front();
  }

  return NULL;
}

jobject javaFindBestMatchingConstructor(
  JNIEnv *env,
  std::list<jobject>& constructors,
  std::list<int>& argTypes) {

  std::list<jobject> possibleMatches;
  jclass constructorClazz = env->FindClass("java/lang/reflect/Constructor");
  jmethodID constructor_getParameterTypes = env->GetMethodID(constructorClazz, "getParameterTypes", "()[Ljava/lang/Class;");

  for(std::list<jobject>::iterator it = constructors.begin(); it != constructors.end(); it++) {
    jarray parameters = (jarray)env->CallObjectMethod(*it, constructor_getParameterTypes);
    if(env->GetArrayLength(parameters) == (jsize)argTypes.size()) {
      possibleMatches.push_back(*it);
    }
  }

  if(possibleMatches.size() == 0) {
    return NULL;
  }
  if(possibleMatches.size() == 1) {
    return possibleMatches.front();
  } else {
    // TODO: argument match
    printf("javaFindBestMatchingConstructor: multiple matches (choosing the first)\n");
    for(std::list<jobject>::iterator it = possibleMatches.begin(); it != possibleMatches.end(); it++) {
      printf("  %s\n", javaObjectToString(env, *it).c_str());
    }
    return possibleMatches.front();
  }

  return NULL;
}

JNIEnv* javaAttachCurrentThread(JavaVM* jvm) {
  JNIEnv* env;
  JavaVMAttachArgs attachArgs;
  attachArgs.version = JNI_VERSION_1_4;
  attachArgs.name = NULL;
  attachArgs.group = NULL;
  jvm->AttachCurrentThread((void**)&env, &attachArgs);
  return env;
}

void javaDetachCurrentThread(JavaVM* jvm) {
  jvm->DetachCurrentThread();
}

jvalueType javaGetType(JNIEnv *env, jclass type) {
  // TODO: has to be a better way
  const char *typeStr = javaObjectToString(env, type).c_str();
  //printf("%s\n", typeStr);
  if(strcmp(typeStr, "int") == 0) {
    return TYPE_INT;
  } else if(strcmp(typeStr, "long") == 0) {
    return TYPE_LONG;
  } else if(strcmp(typeStr, "void") == 0) {
    return TYPE_VOID;
  } else if(strcmp(typeStr, "boolean") == 0) {
    return TYPE_BOOLEAN;
  } else if(strcmp(typeStr, "byte") == 0) {
    return TYPE_BYTE;
  } else if(strcmp(typeStr, "class java.lang.String") == 0) {
    return TYPE_STRING;
  }

  return TYPE_OBJECT;
}

jclass javaFindClass(JNIEnv* env, std::string className) {
  std::replace(className.begin(), className.end(), '.', '/');
  jclass clazz = env->FindClass(className.c_str());
  return clazz;
}

jobject javaFindField(JNIEnv* env, jclass clazz, std::string fieldName) {
  jclass clazzclazz = env->GetObjectClass(clazz);
  jclass fieldClazz = env->FindClass("java/lang/reflect/Field");
  jmethodID field_getName = env->GetMethodID(fieldClazz, "getName", "()Ljava/lang/String;");
  jmethodID class_getFields = env->GetMethodID(clazzclazz, "getFields", "()[Ljava/lang/reflect/Field;");
  jobjectArray fieldObjects = (jobjectArray)env->CallObjectMethod(clazz, class_getFields);

  jsize fieldCount = env->GetArrayLength(fieldObjects);
  for(jsize i=0; i<fieldCount; i++) {
    jobject field = env->GetObjectArrayElement(fieldObjects, i);
    std::string fieldName = javaToString(env, (jstring)env->CallObjectMethod(field, field_getName));
    if(strcmp(fieldName.c_str(), fieldName.c_str()) == 0) {
      return field;
    }
  }

  return NULL;
}

jobject v8ToJava(JNIEnv* env, v8::Local<v8::Value> arg, int *methodArgType) {
  if(arg->IsNull()) {
    return NULL;
  }

  if(arg->IsString()) {
    *methodArgType = TYPE_STRING;
    v8::String::AsciiValue val(arg->ToString());
    return env->NewStringUTF(*val);
  }

  if(arg->IsInt32()) {
    jint val = arg->ToInt32()->Value();
    *methodArgType = TYPE_INT;
    jclass clazz = env->FindClass("java/lang/Integer");
    jmethodID constructor = env->GetMethodID(clazz, "<init>", "(I)V");
    return env->NewObject(clazz, constructor, val);
  }

  if(arg->IsBoolean()) {
    jboolean val = arg->ToBoolean()->Value();
    *methodArgType = TYPE_BOOLEAN;
    jclass clazz = env->FindClass("java/lang/Boolean");
    jmethodID constructor = env->GetMethodID(clazz, "<init>", "(Z)V");
    return env->NewObject(clazz, constructor, val);
  }

  if(arg->IsObject()) {
    v8::Local<v8::Object> obj = v8::Object::Cast(*arg);
    v8::String::AsciiValue constructorName(obj->GetConstructorName());
    if(strcmp(*constructorName, "JavaObject") == 0) {
      JavaObject* javaObject = node::ObjectWrap::Unwrap<JavaObject>(obj);
      *methodArgType = TYPE_OBJECT;
      return javaObject->getObject();
    }
  }

  // TODO: handle other arg types
  v8::String::AsciiValue typeStr(arg);
  printf("Unhandled type: %s\n", *typeStr);
  *methodArgType = TYPE_OBJECT;
  return NULL;
}

jarray v8ToJava(JNIEnv* env, const v8::Arguments& args, int start, int end, std::list<int> *methodArgTypes) {
  jclass clazz = env->FindClass("java/lang/Object");
  jobjectArray results = env->NewObjectArray(end-start, clazz, NULL);

  for(int i=start; i<end; i++) {
    int methodArgType;
    jobject val = v8ToJava(env, args[i], &methodArgType);
    env->SetObjectArrayElement(results, i - start, val);
    if(methodArgTypes) {
      methodArgTypes->push_back(methodArgType);
    }
  }

  return results;
}

v8::Handle<v8::Value> javaExceptionToV8(JNIEnv* env, const std::string& alternateMessage) {
  jthrowable ex = env->ExceptionOccurred();
  if(ex) {
    printf("BEGIN Java Exception -------\n");
    env->ExceptionDescribe(); // TODO: handle error
    printf("END Java Exception ---------\n");
    // TODO: convert error to v8 error
  }
  return v8::Exception::TypeError(v8::String::New(alternateMessage.c_str()));
}
