#pragma once
#include "gemini_client.h"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace npcai {
struct DiagnosticSnapshot {
    std::string version="2.4.1",iniPath,transport,model,lastError,testStatus="Não executado",exportStatus;
    bool iniFound=false,configValid=true,geminiEnabled=false,httpsAvailable=false,credentialPresent=false,gameBridgeReady=false;
    int internet=-1,lastHttp=0;bool testing=false;
    std::string selection="Aguardando GameBridge",bubbles="Aguardando projeção";
    std::vector<std::string> errors,warnings;
};
class Diagnostics {
public:
    Diagnostics(const Settings&,GeminiClient::HttpsTransport,std::string exportDirectory);
    ~Diagnostics();
    DiagnosticSnapshot Get() const;
    void Services(bool available,int internet,bool credential,const std::string& safeError);
    void Bridge(bool ready);
    void Live(const std::string& fixedSelection,bool target,bool projected);
    void ObserveHttps(const std::string& envelope);
    bool TestGemini();bool Export();
    std::string Report() const;
    void Shutdown();
private:
    void Run();
    Settings settings_;GeminiClient::HttpsTransport https_;std::string exportDirectory_;
    mutable std::mutex mutex_;std::mutex shutdownMutex_;std::condition_variable cv_;
    DiagnosticSnapshot state_;bool stop_=false,test_=false,export_=false;std::atomic_bool cancel_{false};std::thread worker_;
};
std::string SafeServiceError(const std::string& code);
}
