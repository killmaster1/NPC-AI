#include "native_keyboard.h"
#include "android_services.h"
#include "android_audio.h"
#include "android_eleven.h"
#include "types.h"
#include "editor_state.h"
#include <chrono>
#include "../../generated/editor_dex.h"
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mutex>
#include <optional>
namespace npcai::NativeKeyboard {
namespace {
jclass editor=nullptr;
EditorState state;
bool Check(JNIEnv* e) {if(!e->ExceptionCheck()) return true;e->ExceptionClear();logger->Error("Teclado JNI: operação recusada; use teclado touch");return false;}
void Finish(JNIEnv* e,jclass,jlong t,jbyteArray bytes) {
    std::optional<std::string> value;
    if(bytes) {
        jsize n=e->GetArrayLength(bytes);
        if(n>=0 && n<=1200) {
            value=std::string(static_cast<size_t>(n),'\0');
            e->GetByteArrayRegion(bytes,0,n,reinterpret_cast<jbyte*>(value->data()));
            if(e->ExceptionCheck()) {e->ExceptionClear();value.reset();}
        }
    }
    const bool accepted=state.Finish(t,std::move(value));
    logger->Info("IME callback: %s %s",bytes?"send":"cancel",accepted?"accepted":"stale_ignored");
}
}
bool Init(std::string& version) {
    JNIEnv* e=aml->GetJNIEnvironment();if(!e || e->PushLocalFrame(40)<0) return false;
    bool ok=false;
    do {
        jobject context=aml->GetAppContextObject();if(!context) break;
        jclass ctx=e->GetObjectClass(context);
        jmethodID getLoader=e->GetMethodID(ctx,"getClassLoader","()Ljava/lang/ClassLoader;");
        if(!Check(e) || !getLoader) break;
        jobject parent=e->CallObjectMethod(context,getLoader);if(!Check(e) || !parent) break;
        jclass loaderClass=e->FindClass("dalvik/system/InMemoryDexClassLoader");if(!Check(e) || !loaderClass) break;
        jmethodID ctor=e->GetMethodID(loaderClass,"<init>","(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        if(!Check(e) || !ctor) break;
        jobject buffer=e->NewDirectByteBuffer(const_cast<unsigned char*>(editorDex),sizeof editorDex);
        if(!Check(e) || !buffer) break;
        jobject loader=e->NewObject(loaderClass,ctor,buffer,parent);if(!Check(e) || !loader) break;
        jmethodID load=e->GetMethodID(loaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
        if(!Check(e) || !load) break;
        auto cls=static_cast<jclass>(e->CallObjectMethod(loader,load,e->NewStringUTF("br.com.npcai.NativeEditor")));
        if(!Check(e) || !cls) break;
        JNINativeMethod method{const_cast<char*>("finish"),const_cast<char*>("(J[B)V"),reinterpret_cast<void*>(&Finish)};
        if(e->RegisterNatives(cls,&method,1)!=0 || !Check(e)) break;
        jmethodID getVersion=e->GetStaticMethodID(cls,"version","(Landroid/content/Context;)Ljava/lang/String;");
        if(!Check(e) || !getVersion) break;
        auto name=static_cast<jstring>(e->CallStaticObjectMethod(cls,getVersion,context));
        if(!Check(e) || !name) break;
        const char* value=e->GetStringUTFChars(name,nullptr);if(!value) {Check(e);break;}
        version=value;e->ReleaseStringUTFChars(name,value);
        // Editor availability must never depend on the optional HTTPS service.
        editor=static_cast<jclass>(e->NewGlobalRef(cls));ok=editor!=nullptr && Check(e);
        auto networkClass=static_cast<jclass>(e->CallObjectMethod(loader,load,e->NewStringUTF("br.com.npcai.NativeServices")));
        if(!Check(e) || !networkClass || !AndroidServices::Init(e,networkClass,context)) {
            if(e->ExceptionCheck())e->ExceptionClear();
            logger->Error("Serviço HTTPS JNI indisponível; editor e interface preservados");
        }
        auto elevenClass=static_cast<jclass>(e->CallObjectMethod(loader,load,e->NewStringUTF("br.com.npcai.NativeEleven")));
        if(!Check(e) || !elevenClass || !AndroidEleven::Init(e,elevenClass,context)){if(e->ExceptionCheck())e->ExceptionClear();logger->Error("ElevenLabs JNI indisponível; interface preservada");}
        auto audioClass=static_cast<jclass>(e->CallObjectMethod(loader,load,e->NewStringUTF("br.com.npcai.NativeAudio")));
        if(!Check(e) || !audioClass || !AndroidAudio::Init(e,audioClass)){if(e->ExceptionCheck())e->ExceptionClear();logger->Error("Áudio JNI indisponível; chat textual preservado");}
    }while(false);
    e->PopLocalFrame(nullptr);return ok;
}
bool Show(const std::string& initial,const std::string& history) {
    if(!editor || initial.size()>1200) return false;
    auto* e=aml->GetJNIEnvironment();if(!e || e->PushLocalFrame(12)<0) return false;
    bool ok=false;
    do {
        jobject activity=aml->GetCurrentActivity();if(!activity) break;
        jclass activityClass=e->FindClass("android/app/Activity");
        if(!Check(e) || !activityClass || !e->IsInstanceOf(activity,activityClass)) break;
        jmethodID show=e->GetStaticMethodID(editor,"show","(Landroid/app/Activity;[B[BJ)V");
        if(!Check(e) || !show) break;
        jbyteArray bytes=e->NewByteArray(static_cast<jsize>(initial.size()));if(!Check(e) || !bytes) break;
        e->SetByteArrayRegion(bytes,0,static_cast<jsize>(initial.size()),reinterpret_cast<const jbyte*>(initial.data()));
        if(!Check(e)) break;
        size_t historyStart=history.size()>32768?history.size()-32768:0;
        while(historyStart<history.size() && (static_cast<unsigned char>(history[historyStart])&0xC0)==0x80) ++historyStart;
        const auto historySize=static_cast<jsize>(history.size()-historyStart);
        jbyteArray transcript=e->NewByteArray(historySize);if(!Check(e) || !transcript) break;
        e->SetByteArrayRegion(transcript,0,historySize,reinterpret_cast<const jbyte*>(history.data()+historyStart));
        if(!Check(e)) break;
        auto id=state.Begin();if(!id) break;
        logger->Info("IME show queued");
        e->CallStaticVoidMethod(editor,show,activity,bytes,transcript,static_cast<jlong>(*id));ok=Check(e);
        if(!ok) Invalidate();
    }while(false);
    e->PopLocalFrame(nullptr);return ok;
}
bool Take(std::string& out){
    if(!state.Take(out)) return false;
    logger->Info("IME completion consumed on game thread: %s",out.empty()?"cancel":"send");
    return true;
}
bool Active(){return state.Active();}
void Heartbeat(){
    if(!state.Active()) return;
    using Clock=std::chrono::steady_clock;
    static auto last=Clock::now();
    if(Clock::now()-last>std::chrono::seconds(2)){last=Clock::now();logger->Info("IME render heartbeat");}
}
void Invalidate(){
    auto old=state.Invalidate();if(!old || !editor || !aml) return;
    logger->Info("IME invalidated; close queued");
    auto* e=aml->GetJNIEnvironment();if(!e || e->PushLocalFrame(4)<0) return;
    auto hide=e->GetStaticMethodID(editor,"hide","(J)V");
    if(Check(e) && hide){e->CallStaticVoidMethod(editor,hide,static_cast<jlong>(*old));Check(e);}
    e->PopLocalFrame(nullptr);
}
}
