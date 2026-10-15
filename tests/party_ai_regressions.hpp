#pragma once
#include "core/exploration.hpp"
#include "legacy_snapshot.hpp"
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
    s.gameplay.companion.enabled=true;s.gameplay.independentParty=true;
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
    const bool equal=expected.event==actual.event && expected.actorId==actual.actorId &&
            expected.damage==actual.damage && expected.onceKey==actual.onceKey &&
            encodeSnapshot(e.state())==encodeSnapshot(copy.state());
    if(!equal) {
        const auto& a=e.state();const auto& b=copy.state();
        std::cout<<"PARTY_AI REPLAY now="<<now<<" before="<<before.position
                 <<" continuous="<<a.position<<" restored="<<b.position
                 <<" target="<<a.target<<'/'<<b.target
                 <<" companion="<<a.gameplay.companion.tile<<'/'<<b.gameplay.companion.tile
                 <<" round="<<a.gameplay.actionRound<<'/'<<b.gameplay.actionRound
                 <<" event="<<unsigned(expected.event)<<'/'<<unsigned(actual.event)<<std::endl;
    }
    require(equal,"interrupted execution changed the event, RNG or durable state");
    return expected;
}
inline uint64_t nextTime(const Explorer& e,uint64_t now) {
    return std::max(now+1,e.state().gameplay.deadlineMs);
}
inline void forceHit(ExplorationState& s) {
    for(uint64_t seed=0;seed<10000;++seed) {
        Rng rng{seed};const auto attack=rollBasicAttack(rng,true);
        const auto second=rollBasicAttack(rng,true);
        if(attack.hit && !attack.critical && second.hit && !second.critical){s.rng.state=seed;return;}
    }
    throw std::runtime_error("unable to select a deterministic noncritical hit");
}
inline std::vector<uint64_t> walking(ExplorationState s,unsigned hp) {
    s=party(std::move(s));s.gameplay.hearts=hp;
    s.population.entities={{1,10,EntityKind::Item,129,false}};
    Explorer e(s);std::vector<uint64_t> starts;uint64_t now=1000;
    for(unsigned tick=0;tick<32 && starts.size()<4;++tick) {
        const auto event=e.advanceGameplay(now,ActorCompletion{true,true,true});
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
    unsigned failures=0,cases=0;
    auto test=[&](const char* name,const auto& body) {
        ++cases;
        try {body();std::cout<<"PARTY_AI PASS "<<name<<std::endl;}
        catch(const std::exception& error) {
            ++failures;std::cout<<"PARTY_AI FAIL "<<name<<": "<<error.what()<<std::endl;
        }
    };
    test("R01_unobstructed_straight_path",[&] {
        auto s=party(base);s.population.entities={{1,10,EntityKind::Item,129,false}};
        Explorer e(s);std::vector<int> route;uint64_t now=1000;
        for(unsigned tick=0;tick<32 && route.size()<4;++tick) {
            const auto event=e.advanceGameplay(now,ActorCompletion{true,true,true});
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
        uint64_t now=1000;bool attacked=false;
        for(unsigned tick=0;tick<64 && !attacked;++tick) {
            // Keep enemy attacks as misses so this tests the trapped player's
            // available action rather than ending because of unrelated damage.
            if(e.state().gameplay.life==LifePhase::Combat &&
               e.state().gameplay.combat==CombatPhase::EnemyTurn) {
                auto safe=e.state();bool selected=false;
                for(uint64_t seed=0;seed<10000;++seed) {
                    Rng rng{seed};
                    if(!rollBasicAttack(rng,false).hit){safe.rng.state=seed;selected=true;break;}
                }
                require(selected,"unable to select a deterministic enemy miss");
                e=Explorer(safe);
            }
            const auto event=restoredStep(e,now);
            require(event.event!=GameplayEvent::ReturnHome &&
                    !e.state().gameplay.onceKeys.count("return:"+std::to_string(s.runId)),
                    "being surrounded triggered an automatic return");
            require(e.state().phase==Phase::Explore,"surrounded player was defeated before using an available attack");
            attacked=event.event==GameplayEvent::PlayerAttack;
            now=nextTime(e,now);
        }
        require(attacked,"surrounded low-HP player never used its available adjacent attack");
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
            const auto actions=event.actors.empty()?std::vector<ActorEvent>{{event.event,event.actorId,g.enemyInstance,event.damage,0,g.actionRound,event.critical}}:event.actors;
            for(const auto& action:actions) {
            if(action.event==GameplayEvent::PlayerAttack) {
                leader=leader || (action.targetId==2 && action.round==round);
                std::cout<<"PARTY_AI R04 leader target="<<action.targetId<<" round="<<action.round<<std::endl;
            }
            if(action.event==GameplayEvent::CompanionAttack) {
                companion=companion || (action.targetId==1 && action.round==round);
                std::cout<<"PARTY_AI R04 companion target="<<g.enemyInstance<<" round="<<g.actionRound<<std::endl;
            }
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
            rightActed=leftFainted && std::any_of(event.actors.begin(),event.actors.end(),[](const auto& a){
                return a.event==GameplayEvent::PlayerAttack && a.targetId==2;
            });
            require(after.phase==Phase::Explore,"one victory incorrectly ended the whole exploration");
            now=nextTime(e,now);
        }
        require(leftFainted && rightActed,"surviving right-side encounter stopped after left-side victory");
    });
    test("R06_leader_retreat_does_not_cancel_companion_combat",[&] {
        auto s=corridor(base);auto& g=s.gameplay;
        g.hearts=1;g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;
        g.enemyInstance=1;g.enemyHits=2;
        g.companionIntent=CompanionIntent::Engage;g.companionTarget=1;forceHit(s);
        Explorer e(s);const auto event=restoredStep(e,1000);
        const auto action=std::find_if(event.actors.begin(),event.actors.end(),[](const auto& a){
            return a.event==GameplayEvent::CompanionAttack && a.targetId==1;
        });
        require(action!=event.actors.end(),"leader low HP canceled the companion's own adjacent combat");
        ActorCompletion done;done.keyed=true;
        done.actions.push_back({s.runId,action->round,action->actionId,s.floorIndex,kCompanionActorId});
        restoredStep(e,1001,done);
        require(e.state().population.entities[0].remainingHits==1 &&
                e.state().population.entities[1].remainingHits==2,
                "companion completion applied a different actor's impact");
    });
    test("R06_separated_combat_state_roundtrips",[&] {
        auto s=party(base);auto& g=s.gameplay;
        g.companion.tile=122;
        s.population.entities={{1,4,EntityKind::Friend,123,false}};
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
        g.independentParty=false;
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
    test("R08_navigation_matches_uninterrupted_execution",[&] {
        auto s=party(base);s.population.entities={{1,10,EntityKind::Item,129,false}};
        Explorer e(s);uint64_t now=1000;
        for(unsigned tick=0;tick<8;++tick) {
            restoredStep(e,now);
            now=nextTime(e,now);
        }
    });
    test("R10_keyed_completion_is_actor_action_and_run_specific",[&] {
        auto s=corridor(base);forceHit(s);Explorer e(s);
        const auto event=restoredStep(e,1000);
        const auto c=std::find_if(event.actors.begin(),event.actors.end(),[](const auto& a){
            return a.event==GameplayEvent::CompanionAttack;
        });
        require(c!=event.actors.end(),"companion attack record absent");
        const auto token=*c;const auto before=encodeSnapshot(e.state());
        ActorCompletion wrong;wrong.keyed=true;
        wrong.actions.push_back({s.runId+1,token.round,token.actionId,s.floorIndex,kCompanionActorId});
        restoredStep(e,1001,wrong);
        require(encodeSnapshot(e.state())==before,"completion from another run advanced combat");
        wrong.actions[0].runId=s.runId;wrong.actions[0].actorId=0;
        restoredStep(e,1002,wrong);
        require(encodeSnapshot(e.state())==before,"another actor completed the companion attack");
        wrong.actions[0].actorId=kCompanionActorId;wrong.actions[0].actionId=token.actionId+100;
        restoredStep(e,1003,wrong);
        require(encodeSnapshot(e.state())==before,"stale action ID advanced the active clip");
        wrong.actions[0].actionId=token.actionId;restoredStep(e,1004,wrong);
        require(e.state().population.entities[0].remainingHits==1 &&
                e.state().population.entities[1].remainingHits==2,"keyed completion damaged the wrong enemy");
        const auto applied=encodeSnapshot(e.state());restoredStep(e,1005,wrong);
        require(encodeSnapshot(e.state())==applied,"repeated keyed completion replayed damage");
    });
    test("R11_mdes7_pending_hit_finishes_before_party_upgrade",[&] {
        auto s=party(base);auto& g=s.gameplay;g.independentParty=false;
        s.population.entities={{1,4,EntityKind::Friend,123,false}};s.population.entities[0].remainingHits=1;
        g.hostile={1};g.life=LifePhase::Combat;g.combat=CombatPhase::EnemyAttack;
        g.enemyInstance=1;g.enemyHits=1;g.hit=true;g.damage=1;g.enemyTargetsCompanion=true;
        g.playerActionRound=g.companion.actionRound=g.actionRound;
        s.population.entities[0].actionRound=g.actionRound;
        g.phaseStartedMs=1000;g.deadlineMs=6000;
        g.onceKeys.insert("pending-hit:"+std::to_string(s.runId)+":0:1:1");forceHit(s);
        auto bytes=encodeSnapshot(s);removePartyBlock(bytes);
        uint32_t crc=0xffffffff;
        for(size_t i=0;i<bytes.size()-4;++i) {
            crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);
        }
        crc=~crc;for(unsigned b=0;b<4;++b)bytes[bytes.size()-4+b]=uint8_t(crc>>(8*b));
        require(bytes[4]==7,"legacy test is not an MDES7 payload");
        Explorer e(decodeSnapshot(bytes,s.assetVersion));
        e.enableCompanion();require(!e.state().gameplay.independentParty,"upgrade canceled a pending legacy encounter");
        restoredStep(e,1001,{false,true,false});
        require(e.state().gameplay.companion.hearts==4 && e.state().gameplay.hearts==5,
                "MDES7 pending damage was lost or applied to the wrong actor");
        restoredStep(e,1002,{false,true,false});
        require(e.state().gameplay.companion.hearts==4,"MDES7 hit replayed after restore");
        uint64_t now=1003;
        for(unsigned tick=0;tick<64 && e.state().gameplay.life!=LifePhase::Explore;++tick) {
            restoredStep(e,now);now=nextTime(e,now);
        }
        require(e.state().gameplay.life==LifePhase::Explore,"legacy encounter did not finish");
        e.enableCompanion();require(e.state().gameplay.independentParty,"finished legacy encounter did not upgrade");
    });
    test("R12_party_greets_and_passes_a_friendly_corridor",[&] {
        auto s=party(base);
        for(int tile=0;tile<kTiles;++tile)
            if(s.world.walkable(tile) && (tile/kWidth!=5 || tile%kWidth<3 || tile%kWidth>9))
                s.blocked[tile]=1;
        s.population.entities={{1,4,EntityKind::Friend,126,false},{2,10,EntityKind::Item,128,false}};
        Explorer e(s);uint64_t now=1000;bool greeted=false;
        for(unsigned tick=0;tick<64 && e.state().inventory.empty();++tick) {
            const auto event=restoredStep(e,now);
            greeted=greeted || event.event==GameplayEvent::DialoguePlayer;
            require(e.state().phase==Phase::Explore && e.state().position!=e.state().gameplay.companion.tile,
                    "friendly corridor returned home or overlapped the party");
            now=nextTime(e,now);
        }
        require(greeted && e.state().inventory.size()==1 &&
                e.state().onceKeys.count("greet:"+std::to_string(s.runId)+":0:1"),
                "party could not greet and pass the corridor NPC");
    });
    test("R13_recovery_decision_refreshes_room_visibility",[&] {
        auto s=party(base);s.discovered.fill(0);s.discovered[s.position]=1;s.knownStairs=-1;
        s.target=126;s.goalKind=GoalKind::Auxiliary;
        s.gameplay.navigationHistory={navigationFingerprint(s)};
        Explorer e(s);restoredStep(e,1000);
        require(e.state().discovered[129],"detour planning skipped visibility discovery");
    });
    test("R14_exhausted_old_probe_budget_keeps_exploring",[&] {
        auto s=party(base);s.knownStairs=-1;
        s.floorSteps=Explorer::kFloorStepLimit+kTiles;s.stalledActions=s.floorSteps;
        s.population.entities={{1,10,EntityKind::Item,129,false}};
        Explorer e(s);uint64_t now=1000;
        for(unsigned tick=0;tick<16 && e.state().inventory.empty();++tick) {
            const auto event=restoredStep(e,now);
            require(event.event!=GameplayEvent::ReturnHome && e.state().phase==Phase::Explore,
                    "old probe budget ended the current exploration");
            now=nextTime(e,now);
        }
        require(e.state().inventory.size()==1 &&
                e.state().floorSteps<=Explorer::kFloorStepLimit+kTiles,
                "old probe budget permanently disabled reachable goals");
    });
    test("R15_companion_approaches_shared_known_threat",[&] {
        auto s=party(base);s.gameplay.hearts=1;
        s.population.entities={{1,4,EntityKind::Friend,156,false}};s.gameplay.hostile={1};
        require(s.world.room[156]!=s.world.room[s.gameplay.companion.tile] &&
                bfs(s.world,s.gameplay.companion.tile).distance[156]>2,"shared threat fixture is already local");
        Explorer e(s);restoredStep(e,1000);
        require(e.state().gameplay.companionIntent==CompanionIntent::Engage &&
                e.state().gameplay.companionTarget==1 &&
                e.state().gameplay.companion.actionRound==e.state().gameplay.actionRound,
                "companion ignored the threat blocking the leader's safe exploration");
    });
    test("R16_shortest_paths_prefer_axes_and_preserve_corners",[&] {
        const auto horizontal=bfs(base.world,125).pathTo(129);
        require(horizontal==std::vector<int>({126,127,128,129}),"same-row BFS is not straight");
        const auto vertical=bfs(base.world,125).pathTo(221);
        require(vertical==std::vector<int>({149,173,197,221}),"same-column BFS is not straight");
        Mask blocked{};blocked[126]=1;
        const auto detour=bfs(base.world,125,blocked).pathTo(129);
        require(detour.size()==4,"axis preference sacrificed the shortest legal distance");
        int from=125;for(int to:detour) {
            require(canMove(base.world,from,to,blocked),"detour violated occupancy/corner rules");from=to;
        }
        auto corner=base.world;corner.terrain.fill(0);
        corner.terrain[125]=corner.terrain[150]=1;
        require(bfs(corner,125).distance[150]<0,"blocked diagonal corner became passable");
        corner.terrain[126]=1;
        require(bfs(corner,125).distance[150]==2,"one blocked corner side allowed a diagonal shortcut");
    });
    test("R17_companion_return_is_straight_or_a_legal_shortest_detour",[&] {
        for(const bool blockedLine:{false,true}) {
            auto s=party(base);auto& g=s.gameplay;
            g.companion.tile=129;g.companionIntent=CompanionIntent::Return;
            g.playerActionRound=g.actionRound;
            if(blockedLine)s.blocked[127]=1;
            Explorer e(s);const auto event=restoredStep(e,1000);
            const auto& after=e.state();
            require(after.position==125 && after.gameplay.companion.tile!=129,
                    "return fixture moved the consumed leader or waited unnecessarily");
            require(canMove(s.world,129,after.gameplay.companion.tile,s.blocked),
                    "return cut terrain/occupancy corners");
            if(!blockedLine)require(after.gameplay.companion.tile==128,
                    "same-row return chose an unnecessary diagonal");
            require(std::any_of(event.actors.begin(),event.actors.end(),[](const auto& a) {
                return a.actorId==kCompanionActorId && a.event==GameplayEvent::CompanionMove;
            }),"return did not consume the companion's own movement action");
        }
    });
    test("R18_chasing_enemy_shares_hp1_hp5_walking_interval",[&] {
        std::vector<uint64_t> reference;
        for(const unsigned hp:{5U,1U}) {
            auto s=party(base);s.gameplay.hearts=hp;
            s.population.entities={{1,10,EntityKind::Item,129,false},
                                   {2,4,EntityKind::Friend,260,false}};
            s.gameplay.hostile={2};auto& enemy=s.population.entities[1];
            // Keep the remembered endpoint free for four steps in both HP cases.
            // The old player tile may legitimately become companion-occupied.
            enemy.tracking=Tracking::Chase;enemy.lastSeen=enemy.chaseTarget=156;
            Explorer e(s);uint64_t now=1000;std::vector<uint64_t> starts;
            for(unsigned tick=0;tick<16 && starts.size()<4;++tick) {
                const auto event=restoredStep(e,now);
                const auto player=std::find_if(event.actors.begin(),event.actors.end(),[](const auto& a) {
                    return a.actorId==0 && a.event==GameplayEvent::Move;
                });
                if(player!=event.actors.end()) {
                    const auto& state=e.state();const auto& move=state.gameplay.movement;
                    const auto& chase=state.population.entities[1].movement;
                    if(move.durationMs!=400 || chase.durationMs!=400 ||
                       chase.startedMs!=move.startedMs || chase.actionId!=move.actionId) {
                        std::ostringstream details;
                        details<<"enemy and leader movement used separate intervals hp="<<hp<<" now="<<now
                            <<" P="<<state.position<<" C="<<state.gameplay.companion.tile
                            <<" E="<<state.population.entities[1].tile
                            <<" Pstart="<<move.startedMs<<" Pround="<<move.actionId
                            <<" Estart="<<chase.startedMs<<" Eround="<<chase.actionId
                            <<" tracking="<<unsigned(state.population.entities[1].tracking);
                        for(const auto& a:state.gameplay.partyActions)if(a.actorId==2)
                            details<<" Eaction="<<unsigned(a.kind)<<" finished="<<a.finished;
                        throw std::runtime_error(details.str());
                    }
                    starts.push_back(move.startedMs);
                }
                now=nextTime(e,now);
            }
            require(starts==std::vector<uint64_t>({1000,1400,1800,2200}),
                    "chasing enemy introduced a wait or low-HP per-tile delay");
            if(reference.empty())reference=starts;else require(reference==starts,"HP changed chase walking cadence");
            std::cout<<"PARTY_AI R18 HP"<<hp;for(auto start:starts)std::cout<<' '<<start;std::cout<<std::endl;
        }
    });
    test("R19_companion_finishes_separated_fight_then_returns",[&] {
        auto s=party(base);auto& g=s.gameplay;
        s.position=129;s.visited[129]=1;g.hearts=1;g.playerActionRound=g.actionRound;
        g.companionIntent=CompanionIntent::Engage;g.companionTarget=1;
        s.population.entities={{1,4,EntityKind::Friend,123,false},{2,10,EntityKind::Item,130,false}};
        s.population.entities[0].remainingHits=1;g.hostile={1};forceHit(s);
        Explorer e(s);uint64_t now=1000;bool killed=false,returnMoved=false,followed=false;
        for(unsigned tick=0;tick<80 && !followed;++tick) {
            const auto before=e.state();const auto event=restoredStep(e,now);
            const auto& after=e.state();const auto& partner=after.gameplay.companion;
            killed=killed || after.population.entities[0].done;
            const bool moved=std::any_of(event.actors.begin(),event.actors.end(),[](const auto& a) {
                return a.actorId==kCompanionActorId && a.event==GameplayEvent::CompanionMove;
            });
            if(moved && killed) {
                returnMoved=true;
                require(canMove(after.world,before.gameplay.companion.tile,partner.tile),
                        "post-combat return teleported or cut a wall corner");
            }
            followed=returnMoved && after.gameplay.companionIntent==CompanionIntent::Follow &&
                canAttack(after.world,after.position,partner.tile);
            require(event.event!=GameplayEvent::ReturnHome && !after.gameplay.defeated,
                    "return fixture ended the run before rejoining");
            now=nextTime(e,now);
        }
        require(killed && returnMoved && followed,"companion did not finish, return and rejoin after its separated fight");
    });
    test("R20_cached_path_suffix_matches_a_fresh_search",[&] {
        World world;world.terrain.fill(0);
        for(int y=1;y<=4;++y)for(int x=1;x<=3;++x)world.terrain[y*kWidth+x]=1;
        world.terrain[3*kWidth+3]=0;
        const int start=kWidth+3,goal=4*kWidth+1;
        const auto cached=bfs(world,start).pathTo(goal);
        require(cached.size()==3,"suffix fixture no longer has its three-step shortest route");
        for(size_t cut=1;cut<cached.size();++cut) {
            const auto fresh=bfs(world,cached[cut-1]).pathTo(goal);
            const std::vector<int> tail(cached.begin()+cut,cached.end());
            require(fresh==tail,"restoring a static cached path changes its next tile");
        }
    });
    test("R21_legacy_exhausted_budget_resumes_reachable_goals",[&] {
        auto s=party(base);s.gameplay.independentParty=false;s.gameplay.companion={};
        s.floorSteps=Explorer::kFloorStepLimit+kTiles;s.stalledActions=s.floorSteps;
        s.population.entities={{1,10,EntityKind::Item,129,false}};
        Explorer e(s);uint64_t now=1000;
        for(unsigned tick=0;tick<16 && e.state().inventory.empty();++tick) {
            const auto event=restoredStep(e,now);
            require(event.event!=GameplayEvent::ReturnHome && e.state().phase==Phase::Explore,
                    "legacy exhausted budget returned home");
            now=nextTime(e,now);
        }
        require(e.state().inventory.size()==1 && e.state().floorSteps<=Explorer::kFloorStepLimit+kTiles,
                "legacy exhausted budget permanently disabled a reachable goal");
    });
    std::cout<<"PARTY_AI SUMMARY failures="<<failures<<" cases="<<cases<<std::endl;
    return failures;
}
} // namespace party_ai_regression
