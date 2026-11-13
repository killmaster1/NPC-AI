#include "eleven_backend.h"
#include <chrono>
namespace npcai {
class ElevenLabsTtsEngine final:public VoiceBackend {
 Settings s_;std::string root_,error_,credential_,npc_;ElevenTransport https_;std::function<std::string(const std::string&,const std::string&)> control_;NpcVoiceManager voices_;CreditBudget budget_;PcmCache cache_;ElevenVoice voice_;Context context_;int http_=0;
public:
 ElevenLabsTtsEngine(Settings s,std::string root,ElevenTransport t,std::function<std::string(const std::string&,const std::string&)> ctl):s_(std::move(s)),root_(root),https_(std::move(t)),control_(std::move(ctl)),voices_(s_),budget_(root),cache_(root,static_cast<size_t>(s_.elevenCacheMb)*1024*1024,s_.elevenCachePersistent){s_.geminiApiKey.clear();s_.http.token.clear();s_.elevenKey.clear();}
 bool Online()const override{return true;}uint64_t ModelBytes()const override{return 0;}int ResidentModels()const override{return 0;}
 bool SelectModel(const std::string& model,std::string& error)override{if(model!="eleven_v3" && model!="eleven_flash_v2_5"){error=error_="Modelo ElevenLabs não permitido";return false;}s_.elevenModel=model;return Load(error);}
 bool Load(std::string& error)override{if(!https_){error=error_="Ponte ElevenLabs HTTPS indisponível";return false;}if(root_.empty() && (s_.elevenDay>0 || s_.elevenHour>0)){error=error_="Armazenamento privado indisponível para o orçamento";return false;}return true;}
 bool Prepare(const std::string& npc,const Context& c,std::string& id,std::string& error)override{context_=c;npc_=npc;try{voice_=voices_.Assign(npc,c);id=voice_.id;return true;}catch(const std::exception& e){error=error_=e.what();return false;}}
 void Retain(const std::vector<std::string>& ids)override{voices_.Retain(ids);}void Cancel()override{}
 std::string Error()const override{return error_;}
 std::string Details()const override{return JsonValue{{"npc_id",npc_},{"voice_id",voice_.id},{"voice_name",voice_.name},{"characters",budget_.characters},{"requests",budget_.requests},{"avoided",budget_.avoided},{"cache_hits",cache_.hits},{"hour",budget_.Hour()},{"day",budget_.Day()},{"http",http_},{"budget",budget_.warning?"Aviso: >=80% do orçamento local":"Estimativa local; não é auditoria da cobrança"},{"credential",credential_},{"error",error_}}.dump();}
 void Control(const std::string& op,const std::string& value)override{error_.clear();try{
  if(op=="model" && (value=="eleven_v3" || value=="eleven_flash_v2_5")){s_.elevenModel=value;return;}
  if(op=="clear_cache"){cache_.Clear();return;}
  if(op=="import_key" || op=="remove_key" || op=="status"){if(!control_)throw std::runtime_error("JNI indisponível");auto j=JsonValue::parse(control_(op,value));credential_=j.value("credential_present",false)?"presente":"ausente";if(!j.value("error",std::string{}).empty())error_="Credencial ElevenLabs: "+j.value("error",std::string{});return;}
  if(op=="refresh_voices"){std::atomic_bool cancel{false};auto r=https_("","",s_.elevenTimeout,cancel);http_=r.status;if(r.status==200 && !r.voices.empty())voices_.Catalog(std::move(r.voices));else error_="Consulta de vozes falhou (HTTP "+std::to_string(r.status)+"): "+r.error;return;}
 }catch(...){error_="Operação de voz falhou; detalhes privados omitidos";}}
 bool Generate(const std::string& raw,float speed,const std::atomic_bool& cancel,const std::function<bool(const float*,size_t,int)>& sink)override{
  error_.clear();if(cancel)return false;auto input=TrimSpeech(raw);
  if(input.empty() || input.front()=='{' || input.rfind("HTTP ",0)==0 || input.rfind("Erro TTS:",0)==0 || input.find("[Ação confirmada:")!=std::string::npos){++budget_.avoided;error_="Conteúdo técnico ou vazio não é fala";return false;}
  auto text=ShortSpeech(raw,s_.elevenMax);
  if(text.empty() || !ValidVoice(voice_.id)){++budget_.avoided;error_="Fala vazia, longa sem frase completa ou Voice ID ausente; áudio não solicitado";return false;}
  JsonValue settings;if(s_.elevenModel=="eleven_v3")settings={{"stability",context_.emotionIntensity>.6f?.0f:.5f}};
  else{float stability=context_.emotion=="neutral"?.65f:context_.emotion=="afraid"?.4f:.5f;settings={{"stability",stability},{"similarity_boost",.75},{"speed",std::clamp(speed,.7f,1.2f)}};}
  auto spoken=(s_.elevenModel=="eleven_v3" && s_.emotionsEnabled?(context_.delivery=="normal"?(context_.emotionIntensity>.05f?VoiceTag(context_.emotion):std::string{}):DeliveryTag(context_.delivery)):std::string{})+text;
  JsonValue payload={{"text",spoken},{"model_id",s_.elevenModel},{"language_code","pt"},{"voice_settings",settings}};
  auto key=Digest(JsonValue{{"normalization",1},{"voice",voice_.id},{"model",s_.elevenModel},{"language","pt"},{"settings",settings},{"text",spoken}}.dump());
  std::vector<int16_t> pcm;if(!(s_.elevenCache && cache_.Get(key,pcm))){
   // Check credentials before reserving. Budget is charged conservatively once a request may be sent.
   if(control_){try{auto status=JsonValue::parse(control_("status",""));credential_=status.value("credential_present",false)?"presente":"ausente";if(!status.value("credential_present",false)){error_="Credencial ElevenLabs ausente; importe pelo menu";++budget_.avoided;return false;}}catch(...){error_="Serviço ElevenLabs indisponível";return false;}}
   int64_t now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();if(!budget_.Reserve(Characters(spoken),s_.elevenRate,s_.elevenHour,s_.elevenDay,now,error_))return false;
   if(cancel){++budget_.avoided;return false;}auto response=https_(voice_.id,payload.dump(),s_.elevenTimeout,cancel);http_=response.status;
   if(cancel)return false;
   if(http_!=200 || response.pcm.empty()){error_="ElevenLabs HTTP "+std::to_string(http_)+": "+(http_==401?"credencial recusada":http_==402?"créditos/plano insuficientes":http_==403?"voz ou permissão indisponível":http_==429?"limite do provedor; sem retry automático":response.error.empty()?"áudio indisponível":response.error);return false;}
   pcm=std::move(response.pcm);bool audible=false;for(auto sample:pcm)if(sample>5 || sample<-5){audible=true;break;}if(!audible){error_="PCM silencioso; sem retry automático";return false;}if(pcm.size()>960000){error_="Áudio excede 40 segundos";return false;}if(s_.elevenCache)cache_.Put(key,pcm);
  }else ++budget_.avoided;
  if(cancel)return false;
  std::vector<float> samples; samples.reserve(pcm.size());for(auto x:pcm)samples.push_back(x/32768.f);return sink(samples.data(),samples.size(),24000);
 }
};
class UnavailableEleven final:public VoiceBackend {public:bool Online()const override{return true;}bool Load(std::string& e)override{e="Configuração de vozes ou armazenamento ElevenLabs inválidos; interface preservada";return false;}bool Generate(const std::string&,float,const std::atomic_bool&,const std::function<bool(const float*,size_t,int)>&)override{return false;}void Cancel()override{}uint64_t ModelBytes()const override{return 0;}};
std::unique_ptr<VoiceBackend> MakeElevenBackend(const Settings& s,const std::string& root,ElevenTransport t,std::function<std::string(const std::string&,const std::string&)> ctl){try{return std::make_unique<ElevenLabsTtsEngine>(s,root,std::move(t),std::move(ctl));}catch(...){return std::make_unique<UnavailableEleven>();}}
}
