#include <algorithm>
#include "tts.h"
#include <openssl/evp.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
using namespace npcai;
std::string Hash(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)return {};auto* ctx=EVP_MD_CTX_new();EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr);char block[32768];while(f){f.read(block,sizeof block);EVP_DigestUpdate(ctx,block,f.gcount());}unsigned char digest[32];unsigned n=0;EVP_DigestFinal_ex(ctx,digest,&n);EVP_MD_CTX_free(ctx);std::ostringstream out;for(unsigned i=0;i<n;++i)out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(digest[i]);return out.str();}
int main(int argc,char** argv){if(argc!=3)return 2;Settings s;std::string invalid;
 auto missing=MakeKokoroBackend(s,"/npcai/no-model",argv[2],Hash);if(missing->Load(invalid) || invalid.empty())return 1;
 auto fixture=std::filesystem::temp_directory_path()/"npcai-kokoro-corruption-test";std::filesystem::remove_all(fixture);auto dest=fixture/"models/kokoro-pt-br";auto original=std::filesystem::canonical(std::filesystem::path(argv[1])/"models/kokoro-pt-br");std::filesystem::create_directories(dest);
 for(const auto& entry:std::filesystem::recursive_directory_iterator(original))if(entry.is_regular_file()){auto target=dest/std::filesystem::relative(entry.path(),original);std::filesystem::create_directories(target.parent_path());std::filesystem::create_hard_link(entry.path(),target);}
 std::filesystem::remove(dest/"tokens.txt");{std::ofstream out(dest/"tokens.txt");out<<"corrupted";}
 auto corrupt=MakeKokoroBackend(s,fixture.string(),argv[2],Hash);if(corrupt->Load(invalid) || invalid.empty() || corrupt->SelectModel("unregistered-package",invalid))return 1;
 std::filesystem::remove_all(fixture);std::cout<<"Missing/corrupt files and unregistered model safely rejected\n";
 auto b=MakeKokoroBackend(s,argv[1],argv[2],Hash);std::string error;auto start=std::chrono::steady_clock::now();if(!b->Load(error)){std::cerr<<error<<'\n';return 1;}std::cout<<"Load ms "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<" resident="<<b->ResidentModels()<<" files="<<b->ModelBytes()<<'\n';
 for(int id:{42,43,44}){b->Speaker(id);std::atomic_bool cancel{false};uint64_t count=0;double square=0;std::vector<int16_t> pcm;start=std::chrono::steady_clock::now();bool ok=b->Generate("Qual foi, parceiro! Tá ligado, mano? Ação, coração e café.",1,cancel,[&](const float* data,size_t n,int rate){if(rate!=24000)return false;for(size_t i=0;i<n;++i){if(!std::isfinite(data[i]))return false;square+=data[i]*data[i];pcm.push_back(static_cast<int16_t>(std::clamp(data[i],-1.f,1.f)*32767));}count+=n;return true;});if(!ok || count<24000 || square/count<1e-7){std::cerr<<"Invalid PCM "<<id<<'\n';return 1;}
 std::ofstream out(std::string("build/kokoro-")+std::to_string(id)+".pcm",std::ios::binary);out.write(reinterpret_cast<char*>(pcm.data()),pcm.size()*2);
 std::cout<<"Speaker "<<id<<" samples="<<count<<" rms="<<std::sqrt(square/count)<<" synth_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<std::endl;}
 b->Speaker(43);std::atomic_bool cancel{true};if(b->Generate("Cancelado",1,cancel,[](auto,auto,auto){return true;}))return 1;
 cancel=false;bool called=false;bool ok=b->Generate("Primeira frase. Segunda frase.",1,cancel,[&](auto,auto,auto){called=true;cancel=true;return false;});if(ok || !called)return 1;
 b->Unload();if(b->ResidentModels()!=0 || !b->Load(error))return 1;
 std::cout<<"Real FP32 synthesis + cooperative cancellation + unload/reload passed (Linux host only).\n";
}
