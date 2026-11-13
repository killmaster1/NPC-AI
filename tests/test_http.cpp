#include "http_client.h"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    if(argc<4) return 3;
    npcai::HttpConfig config;config.port=static_cast<uint16_t>(std::stoi(argv[1]));
    config.requestTimeoutMs=std::stoi(argv[2]);config.token=argv[3];
    std::atomic_bool cancel{false};std::thread stopper;
    bool structured=argc>4 && std::string(argv[4])=="structured";
    if(argc>4 && !structured) stopper=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(80));cancel=true;});
    npcai::Request request{"session-123.ped-567","c1",structured?"Passa a grana!":"Olá, você vive em uma simulação? ç Ã ê 😀",{}};
    if(structured){request.requestId="session-123.ped-567.c1.r1";request.context.hasPersonality=true;request.context.personality={20,20,60,50,60,40};}
    auto result=npcai::HttpChat(request,config,cancel);
    if(structured){if(!result.decision || result.decision->action!=npcai::Action::SurrenderMoney || result.decision->amount!=100)return 2;std::cout<<npcai::ActionName(result.decision->action)<<"\n";}
    if(stopper.joinable()) stopper.join();
    std::cout<<result.text<<"\n";return result.ok?0:1;
}
