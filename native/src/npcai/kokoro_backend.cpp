#include "tts.h"
#include "memory_store.h"
#include "tts_staging.h"
#include "../../../third_party/sherpa/c-api.h"
#include "../../generated/kokoro_model_hash.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <set>
#include <cstdio>
#ifdef __ANDROID__
#include "../../generated/tts_runtime_hashes.h"
#endif
namespace npcai {
namespace {
// Only this worker owns the Sherpa session. Cancel never frees an inference in progress.
class KokoroTtsEngine final:public VoiceBackend {
 Settings settings_;std::string root_,libs_,private_,alternate_,modelName_;std::function<std::string(const std::string&)> hash_;
 void* ort_=nullptr;void* library_=nullptr;const SherpaOnnxOfflineTts* engine_=nullptr;uint64_t bytes_=0;int speaker_=43,rate_=0;
 decltype(&SherpaOnnxCreateOfflineTts) create_=nullptr;
 decltype(&SherpaOnnxDestroyOfflineTts) destroy_=nullptr;
 decltype(&SherpaOnnxOfflineTtsGenerateWithCallbackWithArg) generate_=nullptr;
 decltype(&SherpaOnnxDestroyOfflineTtsGeneratedAudio) freeAudio_=nullptr;
 decltype(&SherpaOnnxOfflineTtsNumSpeakers) speakers_=nullptr;
 decltype(&SherpaOnnxOfflineTtsSampleRate) sampleRate_=nullptr;
 template<class T>bool Symbol(T& fn,const char* name){fn=reinterpret_cast<T>(dlsym(library_,name));return fn!=nullptr;}
public:
 KokoroTtsEngine(Settings s,std::string root,std::string libs,std::function<std::string(const std::string&)> hash,std::string priv,std::string alt):settings_(std::move(s)),root_(std::move(root)),libs_(std::move(libs)),private_(std::move(priv)),alternate_(std::move(alt)),hash_(std::move(hash)){settings_.geminiApiKey.clear();settings_.http.token.clear();}
 ~KokoroTtsEngine(){Unload();/* ORT process statics: keep code mapped until process exit. */}
 void Unload()override{if(engine_ && destroy_)destroy_(engine_);engine_=nullptr;bytes_=0;modelName_.clear();}
 uint64_t ModelBytes()const override{return bytes_;}
 std::string ActiveModel()const override{return modelName_;}
 int ResidentModels()const override{return engine_?1:0;}
 void Speaker(int id)override{speaker_=id;}
 void Cancel()override{} // Cooperative callback cancellation; the public API cannot interrupt an ONNX Run mid-sentence.
 bool Load(std::string& error)override{return SelectModel(settings_.ttsModel,error);}
 bool SelectModel(const std::string& model,std::string& error)override{
  if(engine_ && model==modelName_)return true;
  if(model!="kokoro-pt-br"){error="Pacote Kokoro não registrado; este build valida kokoro-pt-br FP32";return false;}
  try{
   namespace fs=std::filesystem;const auto directory=fs::path(root_)/"models"/model;
   const auto manifest=directory/"manifest.json";
   if(!hash_ || !fs::is_regular_file(manifest)){error="Modelo Kokoro ausente; instale o pacote FP32 em files/npcai-tts/models/kokoro-pt-br";return false;}
   if(fs::file_size(manifest)>262144 || hash_(manifest.string())!=npcaiKokoroManifestHash){error="Manifesto Kokoro incompatível ou corrompido";return false;}
   std::ifstream in(manifest);const auto m=JsonValue::parse(in);
   if(m.value("schema",0)!=1 || m.value("language","")!="pt" || m.value("precision","")!="fp32" || !m.contains("files") || !m["files"].is_object() || m["files"].size()>2048){error="Metadados Kokoro inválidos";return false;}
   std::set<std::string> required={"model.onnx","voices.bin","tokens.txt","espeak-ng-data/phondata","espeak-ng-data/phonindex","espeak-ng-data/phontab","espeak-ng-data/pt_dict"};
   uint64_t bytes=0;
   for(auto it=m["files"].begin();it!=m["files"].end();++it){const auto name=it.key();
    if(name.empty() || name.size()>180 || name.front()=='/' || name.find("..")!=std::string::npos || name.find('\\')!=std::string::npos || !it.value().is_string()){error="Manifesto Kokoro contém caminho inválido";return false;}
    const auto file=directory/name;const auto relative=fs::relative(fs::canonical(file),fs::canonical(directory)).generic_string();
    if(relative.find("..")!=std::string::npos || !fs::is_regular_file(file) || fs::file_size(file)>512ull*1024*1024 || hash_(file.string())!=it.value().get<std::string>()){error="Integridade SHA-256 dos arquivos Kokoro falhou; reinstale o pacote";return false;}
    bytes+=fs::file_size(file);if(bytes>600ull*1024*1024){error="Pacote Kokoro excede limite de arquivos";return false;}required.erase(name);
   }
   if(!required.empty()){error="Pacote Kokoro incompleto";return false;}
#ifdef __ANDROID__
   if(private_.empty() || !StageVoiceLibraries({libs_,alternate_},private_,npcaiOrtHash,npcaiKokoroHash,hash_,error))return false;
   libs_=private_;
#endif
   if(!ort_)ort_=dlopen((fs::path(libs_)/"libnpcai_kokoro_ort.so").c_str(),RTLD_NOW|RTLD_LOCAL);
   if(!ort_){error="ONNX Runtime não carregou; confira bibliotecas Android e pasta privada AML";return false;}
   if(!library_)library_=dlopen((fs::path(libs_)/"libnpcai_kokoro.so").c_str(),RTLD_NOW|RTLD_LOCAL);
   if(!library_ || !Symbol(create_,"SherpaOnnxCreateOfflineTts") || !Symbol(destroy_,"SherpaOnnxDestroyOfflineTts") || !Symbol(generate_,"SherpaOnnxOfflineTtsGenerateWithCallbackWithArg") || !Symbol(freeAudio_,"SherpaOnnxDestroyOfflineTtsGeneratedAudio") || !Symbol(speakers_,"SherpaOnnxOfflineTtsNumSpeakers") || !Symbol(sampleRate_,"SherpaOnnxOfflineTtsSampleRate")){error="Sherpa-ONNX C API indisponível ou incompatível";return false;}
#ifdef __ANDROID__
   if(settings_.ttsMinMemoryMb>0){std::ifstream memory("/proc/meminfo");std::string line;unsigned long long available=0;while(std::getline(memory,line))if(std::sscanf(line.c_str(),"MemAvailable: %llu kB",&available)==1)break;
    if(available && available<static_cast<unsigned long long>(settings_.ttsMinMemoryMb)*1024){error="RAM disponível insuficiente para Kokoro FP32; texto preservado";return false;}}
#endif
   auto onnx=(directory/"model.onnx").string(),voices=(directory/"voices.bin").string(),tokens=(directory/"tokens.txt").string(),data=(directory/"espeak-ng-data").string();
   SherpaOnnxOfflineTtsConfig config{};config.model.kokoro.model=onnx.c_str();config.model.kokoro.voices=voices.c_str();config.model.kokoro.tokens=tokens.c_str();config.model.kokoro.data_dir=data.c_str();config.model.kokoro.lang="pt";config.model.kokoro.length_scale=1.f;
   config.model.num_threads=settings_.ttsThreads;config.model.provider="cpu";config.max_num_sentences=1;config.silence_scale=1.f;
   Unload();engine_=create_(&config);
   if(!engine_){error="Não foi possível inicializar Kokoro/eSpeak/ONNX; texto preservado";return false;}
   rate_=sampleRate_(engine_);
   if(speakers_(engine_)!=53 || rate_!=24000){Unload();error="Speakers/sample rate incompatíveis com o pacote brasileiro verificado";return false;}
   bytes_=bytes;modelName_=model;return true;
  }catch(const std::bad_alloc&){error="Memória insuficiente para Kokoro FP32; conversa textual preservada";return false;}
   catch(...){error="Arquivos Kokoro inválidos ou inacessíveis; texto preservado";return false;}
 }
 bool Generate(const std::string& text,float speed,const std::atomic_bool& cancel,const std::function<bool(const float*,size_t,int)>& sink)override{
  if(!engine_ || cancel || speaker_<42 || speaker_>44)return false;
  struct Callback {const std::atomic_bool& cancel;const std::function<bool(const float*,size_t,int)>& sink;int rate;bool delivered=false,failed=false;};
  Callback context{cancel,sink,rate_};
  auto callback=[](const float* samples,int32_t n,void* opaque)->int32_t{auto& c=*static_cast<Callback*>(opaque);if(c.cancel || !samples || n<=0 || n>c.rate*20){c.failed=true;return 0;}
   for(int32_t i=0;i<n;++i)if(!std::isfinite(samples[i])){c.failed=true;return 0;}
   try{if(!c.sink(samples,static_cast<size_t>(n),c.rate)){c.failed=true;return 0;}c.delivered=true;return c.cancel?0:1;}catch(...){c.failed=true;return 0;}
  };
  const auto* audio=generate_(engine_,text.c_str(),speaker_,speed,callback,&context);
  const bool ok=audio && audio->n>0 && !cancel && context.delivered && !context.failed;
  if(audio)freeAudio_(audio);
  return ok;
 }
};
}
std::unique_ptr<VoiceBackend> MakeKokoroBackend(const Settings& s,const std::string& root,const std::string& libs,std::function<std::string(const std::string&)> hash,std::string priv,std::string alt){return std::make_unique<KokoroTtsEngine>(s,root,libs,std::move(hash),std::move(priv),std::move(alt));}
}
