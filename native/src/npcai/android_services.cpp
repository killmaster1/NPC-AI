#include "android_services.h"
#include <chrono>
#include <thread>
#include <mutex>
namespace npcai::AndroidServices {
namespace {std::mutex callbackMutex;std::function<void()> pauseObserver;
void Paused(JNIEnv*,jclass){std::lock_guard<std::mutex> lock(callbackMutex);if(pauseObserver)pauseObserver();}
JavaVM* vm=nullptr;jclass services=nullptr;jobject context=nullptr;
struct Env {JNIEnv* e=nullptr;bool attached=false;Env(){if(!vm)return;if(vm->GetEnv(reinterpret_cast<void**>(&e),JNI_VERSION_1_6)!=JNI_OK){if(vm->AttachCurrentThread(&e,nullptr)==JNI_OK)attached=true;else e=nullptr;}}~Env(){if(e && e->ExceptionCheck())e->ExceptionClear();if(attached)vm->DetachCurrentThread();}};
bool Good(JNIEnv* e){if(!e->ExceptionCheck())return true;e->ExceptionClear();return false;}
jbyteArray Bytes(JNIEnv* e,const std::string& s){auto b=e->NewByteArray(s.size());if(b)e->SetByteArrayRegion(b,0,s.size(),reinterpret_cast<const jbyte*>(s.data()));return b;}
std::string Text(JNIEnv* e,jbyteArray bytes){if(!bytes)return {};jsize n=e->GetArrayLength(bytes);if(n<0 || n>600000)return {};std::string s(static_cast<size_t>(n),'\0');if(!Good(e))return {};e->GetByteArrayRegion(bytes,0,n,reinterpret_cast<jbyte*>(s.data()));return Good(e)?s:std::string{};}
}
void SetPauseObserver(std::function<void()> callback){std::lock_guard<std::mutex> lock(callbackMutex);pauseObserver=std::move(callback);}
bool Init(JNIEnv* e,jclass cls,jobject app){JNINativeMethod m{const_cast<char*>("pauseMemory"),const_cast<char*>("()V"),reinterpret_cast<void*>(&Paused)};if(e->RegisterNatives(cls,&m,1)!=JNI_OK)return false;if(e->GetJavaVM(&vm)!=JNI_OK)return false;services=static_cast<jclass>(e->NewGlobalRef(cls));context=e->NewGlobalRef(app);return services && context && Good(e);}
std::string Status(){Env env;auto* e=env.e;if(!e || !services || e->PushLocalFrame(4)<0)return "{}";std::string result;auto method=e->GetStaticMethodID(services,"status","()[B");if(Good(e) && method){auto bytes=static_cast<jbyteArray>(e->CallStaticObjectMethod(services,method));if(Good(e))result=Text(e,bytes);}e->PopLocalFrame(nullptr);return result.empty()?"{}":result;}
std::string Configure(const Settings& s,std::string& diagnostic){Env env;auto* e=env.e;if(!e || !services || e->PushLocalFrame(12)<0){diagnostic="jni_unavailable";return {};}
    auto method=e->GetStaticMethodID(services,"configure","(Landroid/content/Context;[BLjava/lang/String;Ljava/lang/String;)[B");std::string result;
    if(Good(e) && method){auto key=Bytes(e,s.geminiApiKey);if(!Good(e) || !key){e->PopLocalFrame(nullptr);diagnostic="jni_configuration_failed";return {};}auto storage=e->NewStringUTF(s.credentialStorage.c_str());if(!Good(e) || !storage){e->PopLocalFrame(nullptr);diagnostic="jni_configuration_failed";return {};}auto action=e->NewStringUTF(s.credentialAction.c_str());if(!Good(e) || !action){e->PopLocalFrame(nullptr);diagnostic="jni_configuration_failed";return {};}auto bytes=static_cast<jbyteArray>(e->CallStaticObjectMethod(services,method,context,key,storage,action));if(Good(e))result=Text(e,bytes);}
    e->PopLocalFrame(nullptr);diagnostic=result.empty()?"jni_configuration_failed":"";return result;
}
std::string Https(const std::string& model,const std::string& body,int timeout,const std::atomic_bool& cancel){Env env;auto* e=env.e;if(!e || !services || e->PushLocalFrame(8)<0)return "{\"status\":0,\"error\":\"jni\"}";
    std::string answer;
    auto start=e->GetStaticMethodID(services,"start","(Ljava/lang/String;[BI)I");if(!Good(e) || !start){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"jni\"}";}auto poll=e->GetStaticMethodID(services,"poll","(I)[B");if(!Good(e) || !poll){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"jni\"}";}auto stop=e->GetStaticMethodID(services,"cancel","(I)V");
    if(!Good(e) || !start || !poll || !stop){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"jni\"}";}
    auto name=e->NewStringUTF(model.c_str());if(!Good(e) || !name){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"jni\"}";}auto bytes=Bytes(e,body);if(!Good(e) || !bytes){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"jni\"}";}int id=e->CallStaticIntMethod(services,start,name,bytes,timeout);
    if(!Good(e) || id<0){e->PopLocalFrame(nullptr);return "{\"status\":0,\"error\":\"permission_or_busy\"}";}
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    while(!cancel && std::chrono::steady_clock::now()<deadline){auto result=static_cast<jbyteArray>(e->CallStaticObjectMethod(services,poll,id));if(!Good(e))break;if(result){answer=Text(e,result);e->DeleteLocalRef(result);break;}std::this_thread::sleep_for(std::chrono::milliseconds(25));}
    if(answer.empty()){e->CallStaticVoidMethod(services,stop,id);Good(e);answer=cancel?"{\"status\":0,\"error\":\"cancelled\"}":"{\"status\":0,\"error\":\"timeout\"}";}
    e->PopLocalFrame(nullptr);return answer;
}
}
