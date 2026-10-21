#pragma once
#include "core/exploration.hpp"
#include "core/motion.hpp"
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
inline std::string canonicalTerrain(const World& world) {
    int minX=kWidth,minY=kHeight,maxX=-1,maxY=-1;
    for(int p=0;p<kTiles;++p)if(world.walkable(p)) {
        minX=std::min(minX,p%kWidth);maxX=std::max(maxX,p%kWidth);
        minY=std::min(minY,p/kWidth);maxY=std::max(maxY,p/kWidth);
    }
    std::string shape=std::to_string(maxX-minX+1)+"x"+std::to_string(maxY-minY+1)+":";
    for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x)shape+=world.walkable(y*kWidth+x)?'1':'0';
    return shape;
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
        g.playerActionRound=g.actionRound; // leader already spent this turn
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
        auto s=corridor(base);s.gameplay.playerActionRound=s.gameplay.actionRound;forceHit(s);Explorer e(s);
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
    test("R15_companion_ignores_distant_threat",[&] {
        auto s=party(base);s.gameplay.hearts=1;
        s.population.entities={{1,4,EntityKind::Friend,156,false}};s.gameplay.hostile={1};
        require(s.world.room[156]!=s.world.room[s.gameplay.companion.tile] &&
                bfs(s.world,s.gameplay.companion.tile).distance[156]>2,"shared threat fixture is already local");
        Explorer e(s);restoredStep(e,1000);
        require(e.state().gameplay.companionIntent!=CompanionIntent::Engage &&
                e.state().gameplay.companionTarget==0 &&
                e.state().gameplay.companion.actionRound==e.state().gameplay.actionRound,
                "companion left the leader to hunt a distant threat");
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
    test("R19_separated_companion_returns_instead_of_hunting",[&] {
        auto s=party(base);auto& g=s.gameplay;
        s.position=129;s.visited[129]=1;g.playerActionRound=g.actionRound;
        g.companionIntent=CompanionIntent::Engage;g.companionTarget=1;
        s.population.entities={{1,4,EntityKind::Friend,123,false}};g.hostile={1};
        Explorer e(s);const auto event=restoredStep(e,1000);
        require(e.state().gameplay.companionIntent==CompanionIntent::Return &&
                !e.state().gameplay.companionTarget,"old distant engagement was retained");
        require(std::none_of(event.actors.begin(),event.actors.end(),[](const auto& a) {
            return a.event==GameplayEvent::CompanionAttack;
        }),"separated companion attacked instead of returning");
        require(bfs(s.world,s.position).distance[e.state().gameplay.companion.tile]<
                bfs(s.world,s.position).distance[g.companion.tile],"companion failed to move toward leader");
    });
    test("R19_local_player_combat_gets_support",[&] {
        auto s=party(base);s.gameplay.playerActionRound=s.gameplay.actionRound;
        s.population.entities={{1,4,EntityKind::Friend,126,false}};s.gameplay.hostile={1};
        Explorer e(s);const auto event=restoredStep(e,1000);
        require(e.state().gameplay.companionIntent==CompanionIntent::Engage &&
                e.state().gameplay.companionTarget==1,"companion did not assist leader's nearby combat");
        require(std::any_of(event.actors.begin(),event.actors.end(),[](const auto& a) {
            return a.event==GameplayEvent::CompanionMove || a.event==GameplayEvent::CompanionAttack;
        }),"local support used no action");
    });
    test("R19_idle_room_enemy_does_not_trigger_hunt",[&] {
        auto s=party(base);s.gameplay.playerActionRound=s.gameplay.actionRound;
        s.population.entities={{1,4,EntityKind::Friend,128,false}};s.gameplay.hostile={1};
        Explorer e(s);restoredStep(e,1000);
        require(e.state().gameplay.companionIntent!=CompanionIntent::Engage &&
                !e.state().gameplay.companionTarget,"same-room enemy outside support range triggered a hunt");
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
    test("R22_legacy_cornered_combat_does_not_cancel_before_attack",[&] {
        auto s=party(base);s.gameplay.independentParty=false;s.gameplay.companion={};
        s.gameplay.hearts=1;
        s.population.entities={{1,4,EntityKind::Friend,126,false}};
        s.population.entities[0].remainingHits=1;s.gameplay.hostile={1};
        for(int tile=0;tile<kTiles;++tile)
            if(s.world.walkable(tile) && tile!=125 && tile!=126)s.blocked[tile]=1;
        forceHit(s);Explorer e(s);uint64_t now=1000;bool attacked=false,finished=false;
        for(unsigned tick=0;tick<16 && !finished;++tick) {
            const auto event=restoredStep(e,now);
            attacked=attacked || event.event==GameplayEvent::PlayerAttack;
            finished=e.state().population.entities[0].done;
            require(event.event!=GameplayEvent::ReturnHome && e.state().gameplay.hearts==1,
                    "cornered legacy encounter returned home or lost its first attack");
            now=nextTime(e,now);
        }
        require(attacked && finished,"cornered legacy encounter oscillated between retreat and PlayerTurn");
    });
    test("R23_translated_floor_shape_is_not_reused",[&] {
        auto canonical=[](const World& w) {
            int minX=kWidth,minY=kHeight,maxX=-1,maxY=-1;
            for(int p=0;p<kTiles;++p)if(w.walkable(p)) {
                minX=std::min(minX,p%kWidth);maxX=std::max(maxX,p%kWidth);
                minY=std::min(minY,p/kWidth);maxY=std::max(maxY,p/kWidth);
            }
            std::string shape=std::to_string(maxX-minX+1)+"x"+std::to_string(maxY-minY+1)+":";
            for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x)
                shape+=w.walkable(y*kWidth+x)?'1':'0';
            return shape;
        };
        Rng firstRng{340},secondRng{1041};
        const auto first=generate(firstRng,1),second=generate(secondRng,2);
        require(first.terrain!=second.terrain && canonical(first)==canonical(second),
                "controlled original generator seeds are not translated copies");
        require(first.stairs!=second.stairs,"controlled floor candidate hits the prior feet exclusion");
        Explorer initial(940,340,"layout-collision",fixtureProfiles(2),{0,0,0,0});
        auto state=initial.state();
        require(state.world.terrain==first.terrain,"fixture did not reuse actual production generation");
        state.position=state.world.stairs;state.visited[state.position]=state.discovered[state.position]=1;
        state.knownStairs=state.position;state.target=state.position;state.goalKind=GoalKind::Stairs;
        state.rng.state=1041;
        Explorer e(state);const auto event=e.advance();
        require(event.kind==EventKind::FloorChanged,"controlled duplicate fixture did not change floors");
        std::cout<<"PARTY_AI R23 seedA=340 seedB=1041 rawEqual="<<(first.terrain==e.state().world.terrain)
            <<" translatedEqual="<<(canonical(first)==canonical(e.state().world))<<std::endl;
        require(canonical(first)!=canonical(e.state().world),"whole-floor shape repeated after a translation despite the raw-layout guard");
    });
    test("R24_next_day_remembers_the_previous_floor_shape",[&] {
        Explorer e(940,42,"layout-history",fixtureProfiles(1),{0,0,0,0});e.enableGameplay(1000);
        const auto shape=canonicalTerrain(e.state().world);
        e.resetExploration(2000);uint64_t now=2000;
        for(unsigned tick=0;tick<64 && e.state().gameplay.sceneBeat!=SceneBeat::MorningFadeOut;++tick) {
            restoredStep(e,now);now=nextTime(e,now);
        }
        require(e.state().gameplay.sceneBeat==SceneBeat::MorningFadeOut,"new-day layout fixture missed the normal morning boundary");
        auto ready=e.state();ready.rng.state=42;e=Explorer(ready);
        restoredStep(e,ready.gameplay.deadlineMs);
        require(e.state().runId==941 && e.state().gameplay.day==2,"new-day layout fixture did not start its next run");
        require(canonicalTerrain(e.state().world)!=shape,"new run reused the preceding floor shape");
    });
    test("R25_layout_memory_is_bounded_and_restored",[&] {
        std::vector<uint64_t> previous;for(uint64_t i=1;i<=64;++i)previous.push_back(i);
        Explorer e(940,42,"layout-memory",fixtureProfiles(1),{0,0,0,0},previous);
        auto count=[](const ExplorationState& s) {
            return std::count_if(s.gameplay.onceKeys.begin(),s.gameplay.onceKeys.end(),[](const auto& key) {
                return key.find("recent-layout:")==0;
            });
        };
        require(count(e.state())==64,"layout memory grew beyond its fixed history");
        const auto bytes=encodeSnapshot(e.state());
        require(encodeSnapshot(decodeSnapshot(bytes,"layout-memory"))==bytes,"layout memory changed on restore");
        auto invalid=e.state();invalid.gameplay.onceKeys.insert("recent-layout:999:0");
        bool rejected=false;try{encodeSnapshot(invalid);}catch(const std::exception&){rejected=true;}
        require(rejected,"oversized layout history was accepted");
        invalid=e.state();
        const auto first=std::find_if(invalid.gameplay.onceKeys.begin(),invalid.gameplay.onceKeys.end(),[](const auto& key){return key.find("recent-layout:")==0;});
        invalid.gameplay.onceKeys.erase(first);invalid.gameplay.onceKeys.insert("recent-layout:bad:1");
        rejected=false;try{encodeSnapshot(invalid);}catch(const std::exception&){rejected=true;}
        require(rejected,"malformed layout history was accepted");
    });
    test("R26_generated_room_rectangles_do_not_overlap",[&] {
        unsigned checked=0;
        for(uint64_t seed=1;seed<=512;++seed) {
            Rng rng{seed};const auto world=generate(rng,1);
            struct Box {int left=kWidth,top=kHeight,right=-1,bottom=-1;};
            std::vector<Box> rooms(world.centers.size());
            for(int tile=0;tile<kTiles;++tile)if(world.room[tile]>=0) {
                const auto id=size_t(world.room[tile]);require(id<rooms.size(),"room id exceeds generated room count");
                auto& box=rooms[id];box.left=std::min(box.left,tile%kWidth);box.right=std::max(box.right,tile%kWidth);
                box.top=std::min(box.top,tile/kWidth);box.bottom=std::max(box.bottom,tile/kWidth);
            }
            for(size_t id=0;id<rooms.size();++id) {
                const auto& box=rooms[id];require(box.left<=box.right && box.top<=box.bottom,"generated room vanished");
                for(int y=box.top;y<=box.bottom;++y)for(int x=box.left;x<=box.right;++x)
                    require(world.room[y*kWidth+x]==int(id) && world.walkable(y*kWidth+x),"room rectangles overlap or have holes");
                for(size_t other=0;other<id;++other)
                    require(box.right<rooms[other].left || rooms[other].right<box.left ||
                            box.bottom<rooms[other].top || rooms[other].bottom<box.top,"two room bounds overlap");
                ++checked;
            }
        }
        std::cout<<"PARTY_AI R26 room rectangles checked="<<checked<<" seeds=512"<<std::endl;
    });
    test("R27_combat_turn_order_player_partner_three_enemies",[&] {
        auto s=party(base);s.population.entities={{1,4,EntityKind::Friend,126,false},
            {2,4,EntityKind::Friend,100,false},{3,4,EntityKind::Friend,150,false}};
        s.gameplay.hostile={1,2,3};
        bool found=false;
        for(uint64_t seed=0;seed<100000 && !found;++seed) {
            Rng r{seed};bool misses=true;
            for(unsigned i=0;i<5;++i)misses=(!rollBasicAttack(r,i<2).hit)&&misses;
            if(misses){s.rng.state=seed;found=true;}
        }
        require(found,"five miss fixture missing");
        Explorer e(s);ActorCompletion done;done.keyed=true;
        std::vector<uint32_t> order;std::vector<uint64_t> rounds;
        uint64_t now=1000;
        for(unsigned tick=0;tick<24 && order.size()<6;++tick) {
            const auto u=restoredStep(e,now,done);done.actions.clear();
            unsigned started=0;
            for(const auto& a:u.actors)if(a.event==GameplayEvent::PlayerAttack ||
                a.event==GameplayEvent::CompanionAttack || a.event==GameplayEvent::EnemyAttack) {
                ++started;order.push_back(a.actorId);rounds.push_back(a.round);
                done.actions.push_back({s.runId,a.round,a.actionId,s.floorIndex,a.actorId});
            }
            require(started<=1,"two actors started combat in the same update");
            require(std::count_if(e.state().gameplay.partyActions.begin(),e.state().gameplay.partyActions.end(),
                [](const auto& a){return !a.finished;})<=1,"combat actions overlapped");
            if(!done.actions.empty()) {
                const auto frozen=encodeSnapshot(e.state());ActorCompletion none;none.keyed=true;
                restoredStep(e,now+1,none);
                require(frozen==encodeSnapshot(e.state()),"next actor acted before owner completion");
            }
            now+=2;
        }
        require(order==std::vector<uint32_t>({0,kCompanionActorId,1,2,3,0}),"combat actor order differs from leader/partner/enemies/leader");
        require(rounds==std::vector<uint64_t>({1,1,1,1,1,2}),"round advanced before all actors finished");
    });
    test("R28_low_hp_leader_supports_fighting_companion",[&] {
        for(unsigned hp:{1U,2U})for(int enemyTile:{123,100}) {
            auto s=party(base);s.gameplay.hearts=hp;s.gameplay.oran=0;
            s.population.entities={{1,4,EntityKind::Friend,enemyTile,false}};s.gameplay.hostile={1};
            s.gameplay.companionIntent=CompanionIntent::Engage;s.gameplay.companionTarget=1;
            Explorer e(s);const auto u=restoredStep(e,1000);
            const auto action=std::find_if(e.state().gameplay.partyActions.begin(),e.state().gameplay.partyActions.end(),[](const auto& a){return a.actorId==0;});
            require(action!=e.state().gameplay.partyActions.end(),"support did not consume player turn");
            if(canAttack(s.world,s.position,enemyTile))require(action->kind==PartyActionKind::Attack && action->targetId==1,"low HP leader fled adjacent companion battle");
            else require(action->kind==PartyActionKind::Move && canAttack(s.world,e.state().position,enemyTile),"low HP leader did not approach companion battle");
            (void)u;
        }
    });
    test("R29_room_completion_requires_actual_screen_coverage",[&] {
        for(unsigned size:{200U,100U,67U}) {
            auto s=party(base);s.population.entities.clear();s.gameplay.hostile.clear();
            Explorer e(s);e.enableScreenExploration(size);
            const int room=s.world.room[s.position];
            require(!e.roomFullyShown(room),"logical room discovery completed screen coverage");
            const auto rng=e.state().rng.state;
            auto camera=cameraFor(tilePoint(e.state().position),kWidth*24,kHeight*24,size);
            e.observeViewport(camera.x,camera.y);
            require(e.state().rng.state==rng && !e.roomFullyShown(room),"first viewport marked unseen room edges or consumed RNG");
            bool complete=false;
            for(unsigned tick=0;tick<1500;++tick) {
                camera=cameraFor(tilePoint(e.state().position),kWidth*24,kHeight*24,size);
                e.observeViewport(camera.x,camera.y);
                complete=e.roomFullyShown(room);
                e=Explorer(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
                if(complete)break;
                restoredStep(e,1000+tick*400);
                require(e.state().world.room[e.state().position]==room,"leader left a room before its unseen area appeared");
            }
            require(complete,"room coverage planner failed to show every floor tile");
            auto hidden=e.state();hidden.screenSeen.fill(0);hidden.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;
            hidden.gameplay.partyActions.clear();hidden.gameplay.partyPlaybacks.clear();hidden.gameplay.partyPhase=PartyPhase::Idle;
            hidden.gameplay.deadlineMs=hidden.gameplay.phaseStartedMs+750;
            Explorer fade(hidden);fade.observeViewport(0,0);
            require(std::none_of(fade.state().screenSeen.begin(),fade.state().screenSeen.end(),[](auto n){return n!=0;}),"fade counted as shown map");
        }
    });
    test("R30_walking_into_contact_preserves_combat_order",[&] {
        auto s=party(base);s.population.entities={{1,4,EntityKind::Friend,127,false},{2,4,EntityKind::Friend,103,false},{3,4,EntityKind::Friend,151,false},{4,10,EntityKind::Item,129,false}};
        s.gameplay.hostile={1,2,3};Explorer e(s);uint64_t now=1000;ActorCompletion done;done.keyed=true;
        bool sawEnemy=false;
        for(unsigned tick=0;tick<20 && !sawEnemy;++tick) {
            const auto u=restoredStep(e,now,done);done.actions.clear();unsigned attacks=0;
            for(const auto& a:u.actors)if(a.event==GameplayEvent::PlayerAttack || a.event==GameplayEvent::CompanionAttack || a.event==GameplayEvent::EnemyAttack) {
                ++attacks;sawEnemy=sawEnemy || a.event==GameplayEvent::EnemyAttack;
                done.actions.push_back({s.runId,a.round,a.actionId,s.floorIndex,a.actorId});
            }
            require(attacks<=1,"new contact started simultaneous attacks");
            require(std::count_if(e.state().gameplay.partyActions.begin(),e.state().gameplay.partyActions.end(),[](const auto& a){return !a.finished;})<=1,"new contact overlapped actor turns");
            now+=400;
        }
        require(sawEnemy,"walking contact never gave enemies a turn");
    });
    test("R31_mdes8_pending_attack_migrates_to_ordered_turns",[&] {
        auto s=party(base);s.population.entities={{1,4,EntityKind::Friend,126,false}};s.gameplay.hostile={1};
        Explorer e(s);restoredStep(e,1000);auto old=e.state();old.gameplay.partyPhase=PartyPhase::Allies;
        auto bytes=encodeSnapshot(old);removeVarietyBlock(bytes);
        const unsigned extension=8+kTiles*4;
        uint32_t length=0;for(unsigned i=0;i<4;++i)length|=uint32_t(bytes[bytes.size()-8+i])<<(8*i);
        bytes.erase(bytes.end()-8-extension,bytes.end()-8);bytes[4]=8;length-=extension;
        for(unsigned i=0;i<4;++i)bytes[bytes.size()-8+i]=uint8_t(length>>(8*i));
        uint32_t crc=0xffffffff;
        for(size_t i=0;i<bytes.size()-4;++i){crc^=bytes[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
        crc=~crc;for(unsigned i=0;i<4;++i)bytes[bytes.size()-4+i]=uint8_t(crc>>(8*i));
        const auto imported=decodeSnapshot(bytes,s.assetVersion);
        require(imported.gameplay.partyPhase==PartyPhase::Player && !imported.screenExploration,"MDES8 migration skipped pending attack or invented coverage");
        require(imported.gameplay.partyActions.front().id==old.gameplay.partyActions.front().id && imported.rng.state==old.rng.state,"MDES8 import changed frozen attack/RNG");
        e=Explorer(imported);ActorCompletion none;none.keyed=true;
        const auto frozen=encodeSnapshot(e.state());restoredStep(e,1001,none);
        require(frozen==encodeSnapshot(e.state()),"MDES8 pending attack completed without owner");
    });
    std::cout<<"PARTY_AI SUMMARY failures="<<failures<<" cases="<<cases<<std::endl;
    return failures;
}
} // namespace party_ai_regression
