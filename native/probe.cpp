// Diagnostic only. No SDK, ImGui, game symbols, game memory, hooks, or network.
#include <mod/amlmod.h>
#include <android/log.h>
MYMOD(br.com.npcai.probe, NPC_AI_Probe, 1.0.1, NPC_AI_Contributors)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0)
END_DEPLIST()
ON_MOD_LOAD() {
    __android_log_write(ANDROID_LOG_INFO,"NPC_AI_PROBE","Probe 1.0.1 carregado; sem hooks ou acesso ao GTA");
    if(!aml) return;
    bool compatible=aml->HasModOfVersion("net.rusjj.aml","1.4.1");
    __android_log_write(ANDROID_LOG_INFO,"NPC_AI_PROBE",compatible?
        "AML declara compatibilidade >=1.4.1":"AML abaixo de 1.4.1; plugin completo não deve ser usado");
}
ON_MOD_UNLOAD() {}
