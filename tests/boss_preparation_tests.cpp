#include "core/dialogue_catalogue.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void step(Explorer& e,uint64_t now,const DialogueCatalogue& c) {
    Explorer copy(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
    e.advanceGameplay(now,ActorCompletion{true,true,true},c);copy.advanceGameplay(now,ActorCompletion{true,true,true},c);
    check(encodeSnapshot(e.state())==encodeSnapshot(copy.state()),"boss preparation diverges after restore");
}
void crc(std::vector<uint8_t>& b) {
    uint32_t value=0xffffffffU;
    for(size_t i=0;i<b.size()-4;++i){value^=b[i];for(unsigned j=0;j<8;++j)value=(value>>1)^((value&1)?0xedb88320U:0);}
    value=~value;for(unsigned j=0;j<4;++j)b[b.size()-4+j]=uint8_t(value>>(8*j));
}
int main(int argc,char** argv){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto c=loadDialogueCatalogue(root/"assets/original/gameplay/dialogue/catalogue.mdlc");
    for(unsigned id:{1U,4U,8U,14U,17U,21U,35U,41U})for(bool independent:{false,true}) {
        const auto p=loadExplorationPackage(root/"assets/original/exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe");
        Explorer e(9400,41,p.assetVersion,p.floors,{0,0,0,0},{},p.expedition);e.enableGameplay(1000);
        if(e.state().expedition.entryPending)e.advanceGameplay(1000);
        for(unsigned count=0;e.state().floorIndex+2<p.floors.size() && count<256;++count) {
            for(unsigned n=0;e.state().gameplay.sceneBeat!=SceneBeat::None && n<40;++n)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
            if(e.state().floorIndex+2>=p.floors.size())break;
            auto s=e.state();s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;
            s.target=s.position;s.goalKind=GoalKind::Stairs;s.targetInstance=0;s.gameplay.movement={};
            e=Explorer(s);e.advanceGameplay(s.gameplay.deadlineMs+1,true);e.advanceGameplay(e.state().gameplay.deadlineMs,true);
        }
        for(unsigned n=0;e.state().gameplay.sceneBeat!=SceneBeat::None && n<40;++n)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
        check(e.state().floorIndex+2==p.floors.size() && upcomingBoss(e.state()),"did not reach real pre-boss floor");
        e.enableCompanion();auto s=e.state();auto& g=s.gameplay;g.independentParty=independent;
        s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;
        g.companion.tile=-1;
        for(int t=0;t<kTiles;++t)if(t!=s.position && canAttack(s.world,t,s.position)){g.companion.tile=t;break;}
        check(g.companion.tile>=0,"no place for warning partner");
        g.phaseStartedMs=g.deadlineMs=1000;g.sceneBeat=SceneBeat::None;g.life=LifePhase::Explore;g.partyPhase=PartyPhase::Idle;
        g.partyActions.clear();g.partyPlaybacks.clear();g.movement=g.companion.movement={};g.walkStartedMs=g.companion.walkStartedMs=0;
        s.target=-1;s.goalKind=GoalKind::None;s.targetInstance=0;e=Explorer(s);
        step(e,1000,c);check(bossPreparation(e.state()) && dialogueContext(e.state())=="boss.prepare" && e.state().floorIndex==s.floorIndex,"warning did not precede boss entry");
        const auto rng=e.state().variety.dialogueRng;const auto bundle=e.state().variety.dialogue.bundle;
        step(e,1000,c);check(e.state().variety.dialogueRng==rng && e.state().variety.dialogue.bundle==bundle,"warning rerolled each frame");
        check(cataloguePortraits(e.state(),c).speaker==PortraitRole::Partner,"warning lacks partner portrait");
        if(argc==3 && std::string(argv[1])=="--fixtures" && id==14 && independent) {
            std::filesystem::create_directories(argv[2]);auto saved=e.state();
            const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            e.shiftPausedClocks(now-1000,1000);saveSnapshot(std::filesystem::path(argv[2])/"boss-prepare.mdes",e.state());e=Explorer(saved);
        }
        for(unsigned n=0;bossPreparation(e.state()) && n<8;++n)step(e,e.state().gameplay.deadlineMs,c);
        check(e.state().gameplay.onceKeys.count(bossPreparationKey(s)+":done"),"warning was not committed once");
        for(unsigned n=0;(e.state().floorIndex+1<p.floors.size() || e.state().gameplay.sceneBeat!=SceneBeat::None) && n<64;++n)
            step(e,std::max(e.state().gameplay.deadlineMs,e.state().gameplay.phaseStartedMs+1),c);
        check(e.state().floorIndex+1==p.floors.size() && !bossPreparation(e.state()),"warning blocked or repeated at entry");
        unsigned bosses=0;
        for(const auto& actor:e.state().population.entities)if(isBossEntity(e.state(),actor)) {
            ++bosses;check(actor.remainingHits==4,"new boss does not have four hearts");
        }
        check(bosses>0,"terminal fixture lacks bosses");
        auto full=e.state();
        for(unsigned oldHp:{0U,1U,2U}) {
            auto old=full;unsigned selected=0;
            for(auto& actor:old.population.entities)if(isBossEntity(old,actor)) {
                actor.remainingHits=2;
                if(!selected){selected=actor.instanceId;actor.remainingHits=oldHp;
                    if(!oldHp){actor.done=true;old.onceKeys.insert("greet:"+std::to_string(old.runId)+":"+std::to_string(old.floorIndex)+":"+std::to_string(actor.instanceId));}}
            }
            auto bytes=encodeSnapshot(old);check(bytes[4]==20,"writer did not version boss policy");
            bytes.erase(bytes.end()-12,bytes.end()-4);bytes[4]=19;crc(bytes);
            const auto migrated=decodeSnapshot(bytes,p.assetVersion);
            const auto& boss=*std::find_if(migrated.population.entities.begin(),migrated.population.entities.end(),[&](const auto& a){return a.instanceId==selected;});
            check(boss.remainingHits==(oldHp?oldHp+2:0),"old boss damage lost or defeated boss revived");
        }
        if(id==14 && independent) {
            auto hit=full;auto& g=hit.gameplay;
            const auto boss=std::find_if(hit.population.entities.begin(),hit.population.entities.end(),[&](const auto& a){return isBossEntity(hit,a);});
            const auto actor=boss->instanceId;
            for(int t=0;t<kTiles;++t)if(t!=g.companion.tile && canAttack(hit.world,t,boss->tile) &&
                std::none_of(hit.population.entities.begin(),hit.population.entities.end(),[&](const auto& a){return a.kind==EntityKind::Friend && a.tile==t;})){hit.position=t;break;}
            hit.visited[hit.position]=1;g.enemyInstance=actor;g.enemyHits=4;g.partyPhase=PartyPhase::Player;
            g.playerActionRound=g.companion.actionRound=g.actionRound;g.phaseStartedMs=g.deadlineMs=1000;
            const auto action=g.nextPartyId++;
            g.partyActions={{action,g.actionRound,1000,6000,0,actor,1,PartyActionKind::Attack,true,false,false}};
            g.partyPlaybacks={{action,g.actionRound,1000,6000,0,1,4}};
            g.onceKeys.insert("party-attack:"+std::to_string(hit.runId)+":"+std::to_string(hit.floorIndex)+":"+std::to_string(g.actionRound)+":0");
            Explorer damaged(hit);step(damaged,1500,c);
            check(damaged.state().population.entities[0].remainingHits==3 && damaged.state().gameplay.enemyHits==3,"one damage did not leave boss at three hearts");
            Explorer resumed(decodeSnapshot(encodeSnapshot(damaged.state()),p.assetVersion));resumed.advanceGameplay(1500,ActorCompletion{},c);
            check(resumed.state().population.entities[0].remainingHits==3,"restored boss replayed damage");
        }
        std::cout<<"boss HP4 + pre-entry warning/once/restore "<<id<<" independent="<<independent<<" passed\n";
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
