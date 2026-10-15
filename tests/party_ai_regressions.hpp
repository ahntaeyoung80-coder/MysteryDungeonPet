#pragma once
#include "core/exploration.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// Desired behavior for the October 15 follow-up. Failures are intentional on
// the pre-fix baseline, and must remain visible until the implementation passes.
namespace party_ai_regression {
using namespace pet;
inline void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
inline ExplorationState party(ExplorationState s) {
    s.population.entities.clear();
    s.gameplay.hostile.clear();
    s.gameplay.companion.enabled=true;
    s.gameplay.companion.tile=124;
    s.gameplay.companion.actionRound=0;
    s.gameplay.playerActionRound=0;
    return s;
}
inline ExplorationState corridor(ExplorationState s) {
    s=party(std::move(s));
    // Reuse the valid two-room terrain. A blocked mask isolates the four
    // occupied corridor cells without inventing invalid room metadata.
    for(int tile=0;tile<kTiles;++tile)
        if(s.world.walkable(tile) && (tile/kWidth!=5 || tile%kWidth<3 || tile%kWidth>6))
            s.blocked[tile]=1;
    for(int tile=123;tile<=126;++tile) s.world.room[tile]=-1;
    s.population.entities={{1,4,EntityKind::Friend,123,false},
                           {2,4,EntityKind::Friend,126,false}};
    s.gameplay.hostile={1,2};
    return s;
}
inline GameplayUpdate restoredStep(Explorer& e,uint64_t now,ActorCompletion done={true,true,true}) {
    const auto before=e.state();
    Explorer copy(decodeSnapshot(encodeSnapshot(before),before.assetVersion));
    const auto expected=e.advanceGameplay(now,done),actual=copy.advanceGameplay(now,done);
    require(expected.event==actual.event && expected.actorId==actual.actorId &&
            expected.damage==actual.damage && expected.onceKey==actual.onceKey &&
            encodeSnapshot(e.state())==encodeSnapshot(copy.state()),
            "interrupted execution changed the event, RNG or durable state");
    return expected;
}
inline uint64_t nextTime(const Explorer& e,uint64_t now) {
    return std::max(now+1,e.state().gameplay.deadlineMs);
}
inline void forceHit(ExplorationState& s) {
    for(uint64_t seed=0;seed<10000;++seed) {
        Rng rng{seed};const auto attack=rollBasicAttack(rng,true);
        if(attack.hit && !attack.critical){s.rng.state=seed;return;}
    }
    throw std::runtime_error("unable to select a deterministic noncritical hit");
}
inline std::vector<uint64_t> walking(ExplorationState s,unsigned hp) {
    s=party(std::move(s));s.gameplay.hearts=hp;
    s.population.entities={{1,10,EntityKind::Item,129,false}};
    Explorer e(s);std::vector<uint64_t> starts;uint64_t now=1000;
    for(unsigned tick=0;tick<32 && starts.size()<4;++tick) {
        const auto event=restoredStep(e,now);
        if(event.event==GameplayEvent::Move) {
            require(e.state().gameplay.movement.durationMs==400,"walking duration differs from 400ms");
            starts.push_back(e.state().gameplay.movement.startedMs);
        }
        now=nextTime(e,now);
    }
    require(starts.size()==4,"walking fixture did not produce four moves");
    return starts;
}
inline unsigned run(const ExplorationState& base) {
    unsigned failures=0;
    auto test=[&](const char* name,const auto& body) {
        try {body();std::cout<<"PARTY_AI PASS "<<name<<std::endl;}
        catch(const std::exception& error) {
            ++failures;std::cout<<"PARTY_AI FAIL "<<name<<": "<<error.what()<<std::endl;
        }
    };
    test("R01_unobstructed_straight_path",[&] {
        auto s=party(base);s.population.entities={{1,10,EntityKind::Item,129,false}};
        Explorer e(s);std::vector<int> route;uint64_t now=1000;
        for(unsigned tick=0;tick<32 && route.size()<4;++tick) {
            const auto event=restoredStep(e,now);
            if(event.event==GameplayEvent::Move) route.push_back(e.state().position);
            now=nextTime(e,now);
        }
        std::cout<<"PARTY_AI R01 observed=125";
        for(int tile:route) std::cout<<"->"<<tile;
        std::cout<<std::endl;
        require(route==std::vector<int>({126,127,128,129}),
                "unobstructed same-row goal used an unnecessary diagonal detour");
    });
    test("R02_hp_alone_does_not_delay_walking",[&] {
        const auto healthy=walking(base,5),low=walking(base,1);
        std::cout<<"PARTY_AI R02 HP5";
        for(auto start:healthy)std::cout<<' '<<start;
        std::cout<<" HP1";
        for(auto start:low)std::cout<<' '<<start;
        std::cout<<std::endl;
        require(healthy==low,"low HP alone changed movement start times");
        for(size_t i=1;i<low.size();++i)
            require(low[i]-low[i-1]==400,"unobstructed walking contains an extra wait");
    });
    test("R03_hp1_surrounded_fights_without_returning",[&] {
        auto s=corridor(base);s.gameplay.hearts=1;Explorer e(s);
        const auto event=restoredStep(e,1000);
        std::cout<<"PARTY_AI R03 life="<<lifeName(e.state().gameplay.life)
                 <<" phase="<<unsigned(e.state().phase)<<std::endl;
        require(event.event!=GameplayEvent::ReturnHome && e.state().phase==Phase::Explore &&
                !e.state().gameplay.onceKeys.count("return:"+std::to_string(s.runId)),
                "being surrounded triggered an automatic return");
        require(e.state().gameplay.life==LifePhase::Combat,
                "surrounded low-HP actor did not engage an adjacent attackable enemy");
    });
    test("R03_no_legal_action_waits_without_returning",[&] {
        auto s=party(base);s.gameplay.hearts=1;
        s.gameplay.companion.actionRound=s.gameplay.actionRound;
        for(int tile=0;tile<kTiles;++tile)
            if(s.world.walkable(tile) && tile!=s.position && tile!=s.gameplay.companion.tile)
                s.blocked[tile]=1;
        Explorer e(s);uint64_t now=1000;
        for(unsigned tick=0;tick<8;++tick) {
            const auto event=restoredStep(e,now);
            require(event.event!=GameplayEvent::ReturnHome && e.state().phase==Phase::Explore,
                    "no available goal ended exploration instead of waiting");
            now=nextTime(e,now);
        }
    });
    test("R04_both_allies_attack_their_own_enemy_in_one_round",[&] {
        auto s=corridor(base);Explorer e(s);uint64_t now=1000;
        const auto round=s.gameplay.actionRound;
        bool leader=false,companion=false;
        for(unsigned tick=0;tick<64 && e.state().gameplay.actionRound==round;++tick) {
            const auto event=restoredStep(e,now);const auto& g=e.state().gameplay;
            if(event.event==GameplayEvent::PlayerAttack) {
                leader=leader || g.enemyInstance==2;
                std::cout<<"PARTY_AI R04 leader target="<<g.enemyInstance<<" round="<<g.actionRound<<std::endl;
            }
            if(event.event==GameplayEvent::CompanionAttack) {
                companion=companion || g.enemyInstance==1;
                std::cout<<"PARTY_AI R04 companion target="<<g.enemyInstance<<" round="<<g.actionRound<<std::endl;
            }
            require(e.state().phase==Phase::Explore,"two-sided fight left the dungeon");
            now=nextTime(e,now);
        }
        require(leader && companion,"one ally lost its independent attack against the opposite enemy");
    });
    test("R05_one_enemy_faints_other_fight_continues",[&] {
        auto s=corridor(base);auto& g=s.gameplay;
        s.population.entities[0].remainingHits=1;
        g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;
        g.enemyInstance=1;g.enemyHits=1;forceHit(s);
        Explorer e(s);uint64_t now=1000;
        bool leftFainted=false,rightActed=false;
        for(unsigned tick=0;tick<64 && !rightActed;++tick) {
            const auto event=restoredStep(e,now);const auto& after=e.state();
            leftFainted=leftFainted || after.population.entities[0].done;
            rightActed=leftFainted && event.event==GameplayEvent::PlayerAttack &&
                       after.gameplay.enemyInstance==2;
            require(after.phase==Phase::Explore,"one victory incorrectly ended the whole exploration");
            now=nextTime(e,now);
        }
        require(leftFainted && rightActed,"surviving right-side encounter stopped after left-side victory");
    });
    test("R06_leader_retreat_does_not_cancel_companion_combat",[&] {
        auto s=corridor(base);auto& g=s.gameplay;
        g.hearts=1;g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;
        g.enemyInstance=1;g.enemyHits=2;forceHit(s);
        Explorer e(s);const auto event=restoredStep(e,1000);
        require(event.event==GameplayEvent::CompanionAttack && e.state().gameplay.enemyHits==1,
                "leader low HP canceled the companion's own adjacent combat");
    });
    test("R06_separated_combat_state_roundtrips",[&] {
        auto s=party(base);auto& g=s.gameplay;
        g.companion.tile=122;
        s.population.entities={{1,4,EntityKind::Friend,121,false}};
        g.hostile={1};g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;
        g.enemyInstance=1;
        const auto bytes=encodeSnapshot(s);
        const auto restored=decodeSnapshot(bytes,s.assetVersion);
        require(restored.gameplay.companion.tile==122 && restored.position==125,
                "combat save teleported a separated companion");
    });
    test("R07_R08_owner_and_damage_survive_restore",[&] {
        auto s=party(base);auto& g=s.gameplay;
        s.population.entities={{1,4,EntityKind::Friend,123,false}};g.hostile={1};
        g.life=LifePhase::Combat;g.combat=CombatPhase::EnemyAttack;g.enemyInstance=1;
        g.playerActionRound=g.companion.actionRound=g.actionRound;
        g.hit=true;g.damage=1;g.enemyTargetsCompanion=true;
        s.population.entities[0].actionRound=g.actionRound;
        g.phaseStartedMs=1000;g.deadlineMs=6000;
        g.onceKeys.insert("pending-hit:"+std::to_string(s.runId)+":0:1:1");
        Explorer e(s);const auto bytes=encodeSnapshot(e.state());
        restoredStep(e,1001,{true,false,true});
        require(encodeSnapshot(e.state())==bytes,"wrong animation owner applied the pending hit");
        restoredStep(e,1002,{false,true,false});
        require(e.state().gameplay.companion.hearts==4 && e.state().gameplay.hearts==5,
                "restored pending hit damaged the wrong actor");
        restoredStep(e,1003,{false,true,false});
        require(e.state().gameplay.companion.hearts==4,
                "duplicate enemy completion repeated damage");
    });
    std::cout<<"PARTY_AI SUMMARY failures="<<failures<<" cases=9"<<std::endl;
    return failures;
}
} // namespace party_ai_regression
