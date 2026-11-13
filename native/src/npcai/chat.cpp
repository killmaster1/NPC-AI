#include "chat.h"
#include "dialogue_timing.h"
#include "expressive.h"
#include <algorithm>

namespace npcai {
Chat::Chat(Transport t,size_t n):transport_(std::move(t)),historyLimit_(std::clamp<size_t>(n,2,100)),worker_(&Chat::Run,this){}
Chat::~Chat(){Shutdown();}
Chat::Conversation& Chat::ConversationFor(const std::string& id) {
    auto it=conversations_.find(id);
    if(it==conversations_.end()) {
        if(conversations_.size()>=32) {
            auto oldest=std::min_element(conversations_.begin(),conversations_.end(),[](const auto& a,const auto& b){return a.second.touched<b.second.touched;});
            conversations_.erase(oldest);
        }
        it=conversations_.emplace(id,Conversation{"c"+std::to_string(++serial_),{},0}).first;
    }
    it->second.touched=++clock_;
    return it->second;
}
void Chat::Trim(Conversation& c){if(c.messages.size()>historyLimit_) c.messages.erase(c.messages.begin(),c.messages.begin()+(c.messages.size()-historyLimit_));}
void Chat::Select(std::optional<Target> t) {
    std::lock_guard<std::mutex> guard(mutex_);
    if((!target_ && t) || (target_ && (!t || target_->id!=t->id))) {
        if(active_) active_->store(true);
        error_.clear();
    }
    target_=std::move(t);
    if(action_ && (!target_ || action_->target.id!=target_->id)) {
        auto old=conversations_.find(action_->target.id);
        if(old!=conversations_.end())old->second.actionWaiting=false;
        action_.reset();
    }
    if(target_) ConversationFor(target_->id);
}
void Chat::Retain(const std::vector<std::string>& live) {
    std::lock_guard<std::mutex> guard(mutex_);
    for(auto it=conversations_.begin();it!=conversations_.end();) {
        if(std::find(live.begin(),live.end(),it->first)==live.end()) it=conversations_.erase(it);else ++it;
    }
    if(target_ && std::find(live.begin(),live.end(),target_->id)==live.end()) {
        target_.reset();if(active_) active_->store(true);error_.clear();
    }
}
bool Chat::Send(const std::string& text){return Enqueue(text,false);}
bool Chat::Enqueue(const std::string& text,bool internalReaction) {
    std::lock_guard<std::mutex> guard(mutex_);
    if(stopping_ || busy_ || action_ || !target_) return false;
    if(text.empty() || text.size()>1200 || !ValidUtf8(text) || text.find_first_not_of(" \t\r\n")==std::string::npos) {
        error_="Digite de 1 a 1200 bytes UTF-8.";return false;
    }
    for(unsigned char ch:text) if(ch<32 && ch!='\n' && ch!='\t') {error_="Caractere de controle inválido";return false;}
    auto& c=ConversationFor(target_->id);
    if(c.actionWaiting)return false;
    if(!internalReaction)c.messages.push_back({true,text});
    Trim(c);
    active_=std::make_shared<std::atomic_bool>(false);
    activeId_=target_->id;activeToken_=c.token;busy_=true;error_.clear();
    pending_=Job{Request{target_->id,c.token,text,target_->context},active_};
    pending_->request.requestId=target_->id+"."+c.token+".r"+std::to_string(++requestSerial_);
    pending_->request.timeoutMs=settings_.legacyHttp?settings_.http.requestTimeoutMs:settings_.geminiTimeoutMs;
    pending_->request.feedback=c.feedback;pending_->request.spontaneous=internalReaction;
    cv_.notify_one();return true;
}
void Chat::Clear(){
    std::lock_guard<std::mutex> guard(mutex_);
    if(!target_) return;
    if(active_) active_->store(true);
    action_.reset();speech_.clear();
    auto& c=ConversationFor(target_->id);c.messages.clear();c.feedback.reset();c.actionWaiting=false;c.token="c"+std::to_string(++serial_);error_.clear();
}
Snapshot Chat::Get() const {
    std::lock_guard<std::mutex> guard(mutex_);
    Snapshot s;s.target=target_;s.error=error_;
    if(target_) {
        auto it=conversations_.find(target_->id);
        if(it!=conversations_.end()) {
            s.messages=it->second.messages;s.conversationId=it->second.token;s.bubbleDurationMs=it->second.bubbleDurationMs;s.replySerial=it->second.replySerial;
            s.waiting=it->second.actionWaiting || (busy_ && active_ && !active_->load() && activeId_==target_->id && activeToken_==it->second.token);
        }
    }
    return s;
}
void Chat::Run() {
    for(;;) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,[&]{return stopping_ || pending_.has_value();});
        if(stopping_) return;
        Job job=std::move(*pending_);pending_.reset();lock.unlock();
        Result r;
        try { r=transport_(job.request,*job.cancelled); }
        catch(...) { r={false,"Falha inesperada na comunicação"}; }
        lock.lock();
        auto it=conversations_.find(job.request.npcId);
        if(!stopping_ && !job.cancelled->load() && target_ && target_->id==job.request.npcId &&
           it!=conversations_.end() && it->second.token==job.request.conversationId) {
            if(r.text.size()>6000 || !ValidUtf8(r.text)) r={false,"Resposta inválida ou grande demais"};
            if(r.ok) {
                auto d=r.decision.value_or(ParseDecision(r.text,job.request.requestId));
                if(observer_)observer_(job.request,d);
                if(d.action!=Action::None && d.requestId==job.request.requestId){
                    const bool speech=IsSpeechAction(d.action);
                    if(speech){{auto spoken=*target_;spoken.context.emotion=d.emotion;spoken.context.delivery=d.delivery;spoken.context.relationship=d.relationship;spoken.context.relationshipStage=d.relationshipStage;spoken.context.emotionIntensity=d.emotionIntensity;Published(it->second,spoken,d.dialogue);}}
                    action_=ActionRequest{*target_,job.request.conversationId,std::move(d),speech};it->second.actionWaiting=!speech;}
                else {{auto spoken=*target_;spoken.context.emotion=d.emotion;spoken.context.delivery=d.delivery;spoken.context.relationship=d.relationship;spoken.context.relationshipStage=d.relationshipStage;spoken.context.emotionIntensity=d.emotionIntensity;Published(it->second,spoken,d.dialogue);}}
            }
            else error_=std::move(r.text);
        }
        busy_=false;active_.reset();activeId_.clear();activeToken_.clear();
    }
}
std::optional<ActionRequest> Chat::TakeAction(){std::lock_guard<std::mutex> lock(mutex_);auto value=std::move(action_);action_.reset();return value;}
void Chat::ReportAction(const ActionRequest& request,bool success,const std::string&,int amount){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it=conversations_.find(request.target.id);
    if(it==conversations_.end() || it->second.token!=request.conversationId)return;
    it->second.actionWaiting=false;
    if(!request.dialoguePublished){
        // The provider's natural speech survives all action failures. Execution remains a separate receipt.
        std::string shown=request.decision.dialogue;
        if(success && request.decision.action==Action::SurrenderMoney)
            shown+=" Deixei $"+std::to_string(amount)+" no chão.";
        auto spoken=request.target;spoken.context.emotion=request.decision.emotion;spoken.context.delivery=request.decision.delivery;spoken.context.relationship=request.decision.relationship;spoken.context.relationshipStage=request.decision.relationshipStage;spoken.context.emotionIntensity=request.decision.emotionIntensity;Published(it->second,spoken,shown);
    }
    it->second.feedback=ActionFeedback{request.decision.requestId,ActionName(request.decision.action),success?"executed":"failed",amount};
}
void Chat::SetDecisionObserver(std::function<void(const Request&,const Decision&)> observer){std::lock_guard<std::mutex> lock(mutex_);observer_=std::move(observer);}
bool Chat::LocalReply(const Target& t,const std::string& text){std::lock_guard<std::mutex> lock(mutex_);if(!target_ || target_->id!=t.id || stopping_ || text.empty() || text.size()>1200 || !ValidUtf8(text))return false;auto& c=ConversationFor(t.id);Published(c,t,text);return true;}
bool Chat::React(const std::string& text){return Enqueue(text,true);}
void Chat::ConfigureDialogue(const Settings& s){std::lock_guard<std::mutex> lock(mutex_);settings_=s;}
void Chat::Published(Conversation& c,const Target& target,const std::string& original){
    std::string text=original;if(settings_.ttsEngine=="elevenlabs"){auto shortText=ShortSpeech(original,settings_.elevenMax);if(!shortText.empty())text=shortText;}
    c.messages.push_back({false,text});Trim(c);c.replySerial=++replySerial_;
    c.bubbleDurationMs=settings_.dynamicBubble?DialogueDuration(text,settings_):15000;
    if(speech_.size()>=32)speech_.pop_front();
    speech_.push_back({target,c.token,text,c.replySerial});
}
std::optional<SpeechEvent> Chat::TakeSpeech(){std::lock_guard<std::mutex> lock(mutex_);if(speech_.empty())return {};auto v=std::move(speech_.front());speech_.pop_front();return v;}
void Chat::Shutdown() {
    std::lock_guard<std::mutex> shutdown(shutdownMutex_);
    {std::lock_guard<std::mutex> guard(mutex_);stopping_=true;if(active_) active_->store(true);pending_.reset();}
    cv_.notify_one();if(worker_.joinable()) worker_.join();
}
}
