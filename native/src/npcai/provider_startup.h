#pragma once
#include "diagnostics.h"
#include <algorithm>
namespace npcai {
struct NativeServiceState {std::string privateDirectory,error;int internet=-1;bool available=false,credential=false;};
// Optional network initialization is a contained stage, never a gate for the local interface.
inline NativeServiceState PrepareNativeProvider(const Settings& settings,
    const std::function<std::string(const Settings&,std::string&)>& configure,
    const std::function<std::string()>& status){
    NativeServiceState result;
    try{
        Settings serviceSettings=settings;
        if(!settings.providerConfigError.empty()){
            std::fill(serviceSettings.geminiApiKey.begin(),serviceSettings.geminiApiKey.end(),'\0');serviceSettings.geminiApiKey.clear();
            serviceSettings.credentialAction="none"; // Do not import/remove credentials from an invalid provider config.
        }
        std::string diagnostic;auto configured=configure(serviceSettings,diagnostic);
        auto response=JsonValue::parse(configured.empty()?"{}":configured);
        result.privateDirectory=response.value("body",std::string{});
        result.error=SafeServiceError(response.value("error",diagnostic));
        auto state=JsonValue::parse(status());
        result.available=state.value("https_available",false);result.internet=state.value("internet",-1);
        if(result.internet<0 || result.internet>1)result.internet=-1;
        result.credential=state.value("credential_present",false);
        if(!result.available && result.error.empty())result.error="jni_unavailable";
    }catch(...){result.error="jni_configuration";}
    return result;
}
}
