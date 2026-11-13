#include "android_eleven.h"
#include <chrono>
#include <thread>
#include <mod/amlmod.h>
namespace npcai::AndroidEleven {
namespace {JavaVM* vm=nullptr;jclass cls=nullptr;jobject app=nullptr;
struct Env{JNIEnv* e=nullptr;bool attached=false;Env(){if(vm && vm->GetEnv(reinterpret_cast<void**>(&e),JNI_VERSION_1_6)!=JNI_OK){if(vm->AttachCurrentThread(&e,nullptr)==JNI_OK)attached=true;else e=nullptr;}}~Env(){if(e && e->ExceptionCheck())e->ExceptionClear();if(attached)vm->DetachCurrentThread();}};
bool Good(JNIEnv* e){if(!e->ExceptionCheck())return true;e->ExceptionClear();return false;}
jmethodID Method(JNIEnv* e,const char* name,const char* sig){auto m=e->GetStaticMethodID(cls,name,sig);return Good(e)?m:nullptr;}
jbyteArray Bytes(JNIEnv* e,const std::string& s){auto a=e->NewByteArray(s.size());if(a)e->SetByteArrayRegion(a,0,s.size(),reinterpret_cast<const jbyte*>(s.data()));return a;}
std::string Text(JNIEnv* e,jbyteArray a){if(!a)return {};auto n=e->GetArrayLength(a);if(n<0 || n>600000)return {};std::string s(n,'\0');e->GetByteArrayRegion(a,0,n,reinterpret_cast<jbyte*>(s.data()));return Good(e)?s:std::string{};}
}
bool Init(JNIEnv* e,jclass c,jobject context){if(e->GetJavaVM(&vm)!=JNI_OK)return false;cls=static_cast<jclass>(e->NewGlobalRef(c));app=e->NewGlobalRef(context);return cls && app && Good(e);}
std::string Configure(const Settings& s){Env env;auto e=env.e;if(!e || !cls || e->PushLocalFrame(8)<0)return "{\"error\":\"jni\"}";std::string out;auto m=e->GetStaticMethodID(cls,"configure","(Landroid/content/Context;[BLjava/lang/String;Ljava/lang/String;)[B");if(Good(e) && m){auto key=Bytes(e,s.elevenKey);auto storage=e->NewStringUTF(s.elevenStorage.c_str()),action=e->NewStringUTF(s.elevenKeyAction.c_str());if(Good(e) && key && storage && action){auto b=static_cast<jbyteArray>(e->CallStaticObjectMethod(cls,m,app,key,storage,action));if(Good(e))out=Text(e,b);}}e->PopLocalFrame(nullptr);return out.empty()?"{\"error\":\"jni\"}":out;}
std::string Control(const std::string& op,const std::string& value){Env env;auto e=env.e;if(!e || !cls || e->PushLocalFrame(6)<0)return "{\"error\":\"jni\"}";std::string out;auto m=e->GetStaticMethodID(cls,"control","(Ljava/lang/String;[B)[B");if(Good(e) && m){auto name=e->NewStringUTF(op.c_str());auto bytes=Bytes(e,value);if(Good(e) && name && bytes){auto b=static_cast<jbyteArray>(e->CallStaticObjectMethod(cls,m,name,bytes));if(Good(e))out=Text(e,b);}}e->PopLocalFrame(nullptr);return out.empty()?"{\"error\":\"jni\"}":out;}
ElevenReply Https(const std::string& voice,const std::string& body,int timeout,const std::atomic_bool& cancel){ElevenReply out;out.error="jni";Env env;auto e=env.e;if(!e || !cls || e->PushLocalFrame(10)<0)return out;
 auto start=Method(e,"start","(Ljava/lang/String;[BI)I");auto poll=Method(e,"poll","(I)[B");auto audio=Method(e,"audio","(I)[B");auto stop=Method(e,"cancel","(I)V");
 if(!Good(e) || !start || !poll || !audio || !stop){e->PopLocalFrame(nullptr);return out;}
 auto name=e->NewStringUTF(voice.c_str());auto bytes=Bytes(e,body);if(!Good(e) || !name || !bytes){e->PopLocalFrame(nullptr);return out;}int id=e->CallStaticIntMethod(cls,start,name,bytes,timeout);if(!Good(e) || id<0){out.error="permission_or_busy";e->PopLocalFrame(nullptr);return out;}
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);bool done=false;
 while(!cancel && std::chrono::steady_clock::now()<deadline){auto a=static_cast<jbyteArray>(e->CallStaticObjectMethod(cls,poll,id));if(!Good(e))break;if(a){try{auto j=JsonValue::parse(Text(e,a));out.status=j.value("status",0);out.error=j.value("error","");if(voice.empty() && out.status==200){auto catalog=JsonValue::parse(j.value("body","{}"));if(catalog.contains("voices") && catalog["voices"].is_array())for(const auto& v:catalog["voices"]){auto vId=v.value("voice_id","");if(ValidVoice(vId) && out.voices.size()<128)out.voices.push_back({vId,v.value("name",vId)});}}done=true;}catch(...){out.error="invalid_response";}e->DeleteLocalRef(a);break;}std::this_thread::sleep_for(std::chrono::milliseconds(25));}
 if(done && !cancel){auto pcm=static_cast<jbyteArray>(e->CallStaticObjectMethod(cls,audio,id));if(Good(e) && pcm && !voice.empty()){auto n=e->GetArrayLength(pcm);if(n>0 && n<=1920000 && n%2==0){std::string data(n,'\0');e->GetByteArrayRegion(pcm,0,n,reinterpret_cast<jbyte*>(data.data()));if(Good(e) && data.compare(0,3,"ID3")!=0 && data.compare(0,4,"RIFF")!=0){out.pcm.resize(n/2);for(int i=0;i<n/2;++i)out.pcm[i]=static_cast<int16_t>(static_cast<unsigned char>(data[2*i]) | (static_cast<unsigned char>(data[2*i+1])<<8));}}}}
 else{e->CallStaticVoidMethod(cls,stop,id);Good(e);out.status=0;out.error=cancel?"cancelled":"timeout";}
 e->PopLocalFrame(nullptr);return out;
}
void ImportEditor(){Env env;auto e=env.e;if(!e || !cls)return;auto m=e->GetStaticMethodID(cls,"showImport","(Landroid/app/Activity;)V");if(Good(e) && m){auto activity=aml->GetCurrentActivity();if(activity)e->CallStaticVoidMethod(cls,m,activity);}Good(e);}
}
