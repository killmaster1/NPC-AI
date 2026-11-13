#include "android_audio.h"
namespace npcai::AndroidAudio {
namespace {JavaVM* vm=nullptr;jclass cls=nullptr;jmethodID open=nullptr,write=nullptr,head=nullptr,close=nullptr,hash=nullptr,gains=nullptr;
struct Env{JNIEnv* e=nullptr;bool attached=false;Env(){if(vm && vm->GetEnv(reinterpret_cast<void**>(&e),JNI_VERSION_1_6)!=JNI_OK){attached=vm->AttachCurrentThread(&e,nullptr)==JNI_OK;if(!attached)e=nullptr;}}~Env(){if(e && e->ExceptionCheck())e->ExceptionClear();if(attached)vm->DetachCurrentThread();}};
bool Good(JNIEnv* e){if(!e->ExceptionCheck())return true;e->ExceptionClear();return false;}}
bool Init(JNIEnv* e,jclass c){if(e->GetJavaVM(&vm)!=JNI_OK)return false;cls=static_cast<jclass>(e->NewGlobalRef(c));if(!Good(e) || !cls)return false;
gains=e->GetStaticMethodID(cls,"gains","(FF)V");open=e->GetStaticMethodID(c,"open","(IF)Z");if(!Good(e))return false;write=e->GetStaticMethodID(c,"write","([S)I");if(!Good(e))return false;head=e->GetStaticMethodID(c,"head","()J");if(!Good(e))return false;close=e->GetStaticMethodID(c,"close","()V");if(!Good(e))return false;hash=e->GetStaticMethodID(c,"hash","(Ljava/lang/String;)Ljava/lang/String;");return Good(e) && open && write && head && close && hash && gains;}
bool Open(int rate,float volume){Env env;if(!env.e || !open)return false;auto value=env.e->CallStaticBooleanMethod(cls,open,rate,volume);return Good(env.e) && value;}
void Gains(float left,float right){Env env;if(env.e && cls && gains){env.e->CallStaticVoidMethod(cls,gains,left,right);Good(env.e);}}
int Write(const int16_t* data,size_t count){Env env;if(!env.e || !cls || !data || count>1024)return -1;
 int16_t stereo[2048];for(size_t i=0;i<count;++i)stereo[2*i]=stereo[2*i+1]=data[i];
 auto samples=env.e->NewShortArray(static_cast<jsize>(count*2));if(!samples || !Good(env.e))return -1;
 env.e->SetShortArrayRegion(samples,0,static_cast<jsize>(count*2),stereo);
 int result=-1;if(Good(env.e))result=env.e->CallStaticIntMethod(cls,write,samples);bool ok=Good(env.e);env.e->DeleteLocalRef(samples);return ok && result>=0 && result%2==0?result/2:-1;
}
int64_t Head(){Env env;if(!env.e || !head)return -1;auto result=env.e->CallStaticLongMethod(cls,head);return Good(env.e)?result:-1;}
std::string Hash(const std::string& path){Env env;auto* e=env.e;if(!e || !hash || e->PushLocalFrame(4)<0)return {};auto p=e->NewStringUTF(path.c_str());std::string text;if(Good(e) && p){auto s=static_cast<jstring>(e->CallStaticObjectMethod(cls,hash,p));if(Good(e) && s){const char* raw=e->GetStringUTFChars(s,nullptr);if(raw){text=raw;e->ReleaseStringUTFChars(s,raw);}}}e->PopLocalFrame(nullptr);return text;}
void Close(){Env env;if(env.e && close){env.e->CallStaticVoidMethod(cls,close);Good(env.e);}}
}
