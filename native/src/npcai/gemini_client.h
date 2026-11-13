#pragma once
#include "settings.h"
#include "memory_store.h"
#include <functional>
#include <atomic>
namespace npcai {
struct ClientMetrics {std::atomic<uint64_t> requests{0},retries{0},errors{0},tokens{0},latencyMs{0};};
class GeminiClient {
public:
    using HttpsTransport=std::function<std::string(const std::string&,const std::string&,int,const std::atomic_bool&)>;
    GeminiClient(Settings,HttpsTransport);
    Result Chat(const Request&,const JsonValue& compact,const JsonValue& history,const std::atomic_bool&);
    Result Probe(const std::atomic_bool&);
    ClientMetrics metrics;
    static Decision Mock(const Request&,const JsonValue& compact);
    static JsonValue Schema();static std::string Prompt(const Context&);
private:
    Result Send(const Request&,const std::string& payload,const std::atomic_bool&);
    Settings settings_;HttpsTransport https_;
};
}
