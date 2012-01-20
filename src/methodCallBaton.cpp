
#include "methodCallBaton.h"
#include "java.h"
#include "javaObject.h"

MethodCallBaton::MethodCallBaton(Java* java, jobject method, jarray args, v8::Handle<v8::Value>& callback) {
  JNIEnv *env = java->getJavaEnv();

  m_java = java;
  m_args = (jarray)env->NewGlobalRef(args);
  m_callback = v8::Persistent<v8::Value>::New(callback);
  m_method = env->NewGlobalRef(method);
}

MethodCallBaton::~MethodCallBaton() {
  JNIEnv *env = m_java->getJavaEnv();
  env->DeleteGlobalRef(m_args);
  env->DeleteGlobalRef(m_method);
  m_callback.Dispose();
}

void MethodCallBaton::run() {
  eio_custom(MethodCallBaton::EIO_MethodCall, EIO_PRI_DEFAULT, MethodCallBaton::EIO_AfterMethodCall, this);
  ev_ref(EV_DEFAULT_UC);
}

v8::Handle<v8::Value> MethodCallBaton::runSync() {
  JNIEnv *env = m_java->getJavaEnv();
  execute(env);
  return resultsToV8(env);
}

/*static*/ void MethodCallBaton::EIO_MethodCall(eio_req* req) {
  MethodCallBaton* self = static_cast<MethodCallBaton*>(req->data);
  JNIEnv *env = javaAttachCurrentThread(self->m_java->getJvm());
  self->execute(env);
  javaDetachCurrentThread(self->m_java->getJvm());
}

/*static*/ int MethodCallBaton::EIO_AfterMethodCall(eio_req* req) {
  MethodCallBaton* self = static_cast<MethodCallBaton*>(req->data);
  JNIEnv *env = self->m_java->getJavaEnv();
  self->after(env);
  ev_unref(EV_DEFAULT_UC);
  delete self;
  return 0;
}

void MethodCallBaton::after(JNIEnv *env) {
  if(m_callback->IsFunction()) {
    v8::Handle<v8::Value> argv[2];
    argv[0] = v8::Undefined();
    argv[1] = resultsToV8(env);
    v8::Function::Cast(*m_callback)->Call(v8::Context::GetCurrent()->Global(), 2, argv);
  }

  env->DeleteGlobalRef(m_result);
}

v8::Handle<v8::Value> MethodCallBaton::resultsToV8(JNIEnv *env) {
  v8::HandleScope scope;

  if(!m_error.IsEmpty() && !m_error->IsNull()) {
    v8::Handle<v8::Value> err = m_error;
    m_error.Dispose();
    return scope.Close(err);
  }

  switch(m_resultType) {
    case TYPE_VOID:
      return v8::Undefined();
    case TYPE_BOOLEAN:
      {
        jclass booleanClazz = env->FindClass("java/lang/Boolean");
        jmethodID boolean_booleanValue = env->GetMethodID(booleanClazz, "booleanValue", "()Z");
        bool result = env->CallBooleanMethod(m_result, boolean_booleanValue);
        return scope.Close(v8::Boolean::New(result));
      }
    case TYPE_BYTE:
      {
        jclass byteClazz = env->FindClass("java/lang/Byte");
        jmethodID byte_byteValue = env->GetMethodID(byteClazz, "byteValue", "()B");
        jbyte result = env->CallByteMethod(m_result, byte_byteValue);
        return scope.Close(v8::Number::New(result));
      }
    case TYPE_LONG:
      {
        jclass longClazz = env->FindClass("java/lang/Long");
        jmethodID long_longValue = env->GetMethodID(longClazz, "longValue", "()J");
        jlong result = env->CallLongMethod(m_result, long_longValue);
        return scope.Close(v8::Number::New(result));
      }
    case TYPE_INT:
      {
        jclass integerClazz = env->FindClass("java/lang/Integer");
        jmethodID integer_intValue = env->GetMethodID(integerClazz, "intValue", "()I");
        jint result = env->CallIntMethod(m_result, integer_intValue);
        return scope.Close(v8::Integer::New(result));
      }
    case TYPE_OBJECT:
      return scope.Close(JavaObject::New(m_java, m_result));
    case TYPE_STRING:
      return scope.Close(v8::String::New(javaObjectToString(env, m_result).c_str()));
  }
  return v8::Undefined();
}

void NewInstanceBaton::execute(JNIEnv *env) {
  jclass constructorClazz = env->FindClass("java/lang/reflect/Constructor");
  jmethodID constructor_newInstance = env->GetMethodID(constructorClazz, "newInstance", "([Ljava/lang/Object;)Ljava/lang/Object;");

  //printf("invoke: %s\n", javaObjectToString(env, m_method).c_str());

  jobject result = env->CallObjectMethod(m_method, constructor_newInstance, m_args);
  m_resultType = TYPE_OBJECT;
  m_result = env->NewGlobalRef(result);
  if(env->ExceptionCheck()) {
    m_error = v8::Persistent<v8::Value>::New(javaExceptionToV8(env, "Error running method"));
  }
}

void StaticMethodCallBaton::execute(JNIEnv *env) {
  jclass methodClazz = env->FindClass("java/lang/reflect/Method");
  jmethodID method_invoke = env->GetMethodID(methodClazz, "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
  jmethodID method_getReturnType = env->GetMethodID(methodClazz, "getReturnType", "()Ljava/lang/Class;");

  jclass returnType = (jclass)env->CallObjectMethod(m_method, method_getReturnType);

  /*
  printf("calling %s\n", javaObjectToString(env, m_method).c_str());
  printf("arguments\n");
  for(int i=0; i<env->GetArrayLength(m_args); i++) {
    printf("  %s\n", javaObjectToString(env, env->GetObjectArrayElement((jobjectArray)m_args, i)).c_str());
  }
  */

  m_resultType = javaGetType(env, returnType);
  jobject result = env->CallObjectMethod(m_method, method_invoke, NULL, m_args);
  m_result = env->NewGlobalRef(result);
  if(env->ExceptionCheck()) {
    m_error = v8::Persistent<v8::Value>::New(javaExceptionToV8(env, "Error running method"));
  }
}

void InstanceMethodCallBaton::execute(JNIEnv *env) {
  jclass methodClazz = env->FindClass("java/lang/reflect/Method");
  jmethodID method_invoke = env->GetMethodID(methodClazz, "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
  jmethodID method_getReturnType = env->GetMethodID(methodClazz, "getReturnType", "()Ljava/lang/Class;");

  jclass returnType = (jclass)env->CallObjectMethod(m_method, method_getReturnType);

  m_resultType = javaGetType(env, returnType);
  jobject result = env->CallObjectMethod(m_method, method_invoke, m_javaObject->getObject(), m_args);
  m_result = env->NewGlobalRef(result);
  if(env->ExceptionCheck()) {
    m_error = v8::Persistent<v8::Value>::New(javaExceptionToV8(env, "Error running method"));
  }
}

NewInstanceBaton::NewInstanceBaton(
  Java* java,
  jclass clazz,
  jobject method,
  jarray args,
  v8::Handle<v8::Value>& callback) : MethodCallBaton(java, method, args, callback) {
  JNIEnv *env = m_java->getJavaEnv();
  m_clazz = (jclass)env->NewGlobalRef(clazz);
}

NewInstanceBaton::~NewInstanceBaton() {
  JNIEnv *env = m_java->getJavaEnv();
  env->DeleteGlobalRef(m_clazz);
}

StaticMethodCallBaton::StaticMethodCallBaton(
  Java* java,
  jclass clazz,
  jobject method,
  jarray args,
  v8::Handle<v8::Value>& callback) : MethodCallBaton(java, method, args, callback) {
  JNIEnv *env = m_java->getJavaEnv();
  m_clazz = (jclass)env->NewGlobalRef(clazz);
}

StaticMethodCallBaton::~StaticMethodCallBaton() {
  JNIEnv *env = m_java->getJavaEnv();
  env->DeleteGlobalRef(m_clazz);
}

InstanceMethodCallBaton::InstanceMethodCallBaton(
  Java* java,
  JavaObject* obj,
  jobject method,
  jarray args,
  v8::Handle<v8::Value>& callback) : MethodCallBaton(java, method, args, callback) {
  m_javaObject = obj;
  m_javaObject->Ref();
}

InstanceMethodCallBaton::~InstanceMethodCallBaton() {
  m_javaObject->Unref();
}
