#include "core/exploration.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace pet {
const PartyPlayback* partyPlayback(const GameplayState& g,uint32_t actor) {
    const auto found=std::find_if(g.partyPlaybacks.begin(),g.partyPlaybacks.end(),
        [&](const auto& v){return v.actorId==actor;});
    return found==g.partyPlaybacks.end()?nullptr:&*found;
}
uint32_t partyClip(const GameplayState& g,uint32_t actor) {
    if(const auto* v=partyPlayback(g,actor)) return v->clip;
    const auto moving=std::any_of(g.partyActions.begin(),g.partyActions.end(),[&](const auto& a){
        return a.actorId==actor && a.kind==PartyActionKind::Move && !a.finished;
    });
    return moving?0:7;
}
void validatePartyState(const ExplorationState& s) {
    const auto& g=s.gameplay;
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    check(g.nextPartyId>0 && static_cast<unsigned>(g.partyPhase)<=2 &&
        static_cast<unsigned>(g.companionIntent)<=2,"invalid independent party state");
    check(g.partyActions.size()<=kTiles+2 && g.partyPlaybacks.size()<=kTiles+2,
        "party state exceeds actor bound");
    auto find=[&](uint32_t actor)->const Entity* {
        const auto it=std::find_if(s.population.entities.begin(),s.population.entities.end(),
            [&](const auto& e){return e.instanceId==actor;});
        return it==s.population.entities.end()?nullptr:&*it;
    };
    auto validActor=[&](uint32_t actor) {
        if(!actor)return true;
        if(actor==kCompanionActorId)return g.companion.enabled;
        const auto* e=find(actor);
        return e && e->kind==EntityKind::Friend &&
            std::find(g.hostile.begin(),g.hostile.end(),actor)!=g.hostile.end();
    };
    if(g.companionTarget)check(find(g.companionTarget)!=nullptr,"party companion target missing");
    if(!g.independentParty) {
        check(g.partyActions.empty() && g.partyPlaybacks.empty() && g.partyPhase==PartyPhase::Idle &&
            !g.companionTarget && g.companionIntent==CompanionIntent::Follow,
            "party actions without independent mode");return;
    }
    check(g.enabled && g.companion.enabled,"independent party requires a companion");
    std::set<uint32_t> owners;std::set<uint64_t> ids;
    for(const auto& a:g.partyActions) {
        check(validActor(a.actorId) && owners.insert(a.actorId).second &&
            a.id>0 && a.id<g.nextPartyId && ids.insert(a.id).second &&
            a.round==g.actionRound && static_cast<unsigned>(a.kind)<=3 &&
            a.startedMs<=g.phaseStartedMs && a.deadlineMs>=a.startedMs,
            "invalid party action");
        const auto round=a.actorId==0?g.playerActionRound:
            a.actorId==kCompanionActorId?g.companion.actionRound:find(a.actorId)->actionRound;
        check(round==a.round,"party action did not consume its actor turn");
        if(a.kind==PartyActionKind::Attack) {
            const bool ally=a.actorId==0 || a.actorId==kCompanionActorId;
            check(validActor(a.targetId) && a.targetId!=a.actorId &&
                (ally?(a.targetId!=0 && a.targetId!=kCompanionActorId):
                       (a.targetId==0 || a.targetId==kCompanionActorId)) &&
                a.damage<=2 && (a.hit?(a.damage==1 || a.damage==2):(!a.damage && !a.critical)),
                "invalid frozen party attack");
            const auto attack="party-attack:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
                std::to_string(a.round)+":"+std::to_string(a.actorId);
            const auto impact="party-impact:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
                std::to_string(a.round)+":"+std::to_string(a.actorId)+":"+std::to_string(a.id);
            check(g.onceKeys.count(attack) && (g.onceKeys.count(impact)!=0)==a.finished,
                "party attack transaction mismatch");
            const auto* v=partyPlayback(g,a.actorId);
            check(a.finished || (v && v->id==a.id && v->clip==1),"pending attack playback missing");
        } else check(!a.hit && !a.critical && !a.damage,"nonattack carries damage");
    }
    owners.clear();ids.clear();
    for(const auto& v:g.partyPlaybacks) {
        check(validActor(v.actorId) && owners.insert(v.actorId).second &&
            v.id>0 && v.id<g.nextPartyId && ids.insert(v.id).second &&
            v.round>0 && v.round<=g.actionRound && v.startedMs<=g.phaseStartedMs &&
            v.deadlineMs>=v.startedMs && v.direction<8 &&
            (v.clip==1 || v.clip==6 || v.clip==33),"invalid party playback");
    }
}
GameplayUpdate Explorer::advanceParty(uint64_t now,const ActorCompletion& completed) {
    auto& s=state_;auto& g=s.gameplay;
    GameplayUpdate result;bool changed=false,defeated=false;
    auto entity=[&](uint32_t id)->Entity* {
        auto it=std::find_if(s.population.entities.begin(),s.population.entities.end(),
            [&](const auto& e){return e.instanceId==id;});
        return it==s.population.entities.end()?nullptr:&*it;
    };
    auto hostile=[&](const Entity& e) {
        return e.kind==EntityKind::Friend && e.visible && !e.done && e.remainingHits &&
            std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end();
    };
    auto position=[&](uint32_t actor) {
        if(!actor)return s.position;
        if(actor==kCompanionActorId)return g.companion.tile;
        auto* e=entity(actor);if(!e)throw std::logic_error("party actor missing");
        return e->tile;
    };
    auto direction=[&](uint32_t actor)->uint32_t& {
        if(!actor)return g.playerDirection;
        if(actor==kCompanionActorId)return g.companion.direction;
        auto* e=entity(actor);if(!e)throw std::logic_error("party actor missing");
        return e->direction;
    };
    auto consumed=[&](uint32_t actor)->uint64_t& {
        if(!actor)return g.playerActionRound;
        if(actor==kCompanionActorId)return g.companion.actionRound;
        auto* e=entity(actor);if(!e)throw std::logic_error("party actor missing");
        return e->actionRound;
    };
    auto id=[&] {
        if(!g.nextPartyId || g.nextPartyId==UINT64_MAX)throw std::runtime_error("party action ID exhausted");
        return g.nextPartyId++;
    };
    auto event=[&](GameplayEvent kind,uint32_t actor,uint32_t target,uint64_t action,
                   uint32_t damage=0,bool critical=false) {
        result.actors.push_back({kind,actor,target,damage,action,g.actionRound,critical});
        if(result.event==GameplayEvent::None) {
            result.event=kind;result.actorId=actor;result.damage=damage;result.critical=critical;
        }
        changed=true;
    };
    auto playback=[&](uint32_t actor,uint32_t clip,uint32_t facing,uint64_t action=0) {
        PartyPlayback v;v.id=action?action:id();v.round=g.actionRound;v.startedMs=now;
        if(now>UINT64_MAX-5000)throw std::runtime_error("party playback clock overflow");
        v.deadlineMs=now+5000;v.actorId=actor;v.clip=clip;v.direction=facing;
        auto it=std::find_if(g.partyPlaybacks.begin(),g.partyPlaybacks.end(),
            [&](const auto& old){return old.actorId==actor;});
        if(it==g.partyPlaybacks.end())g.partyPlaybacks.push_back(v);else *it=v;
        changed=true;
    };
    auto completedPlayback=[&](const PartyPlayback& v) {
        if(now>=v.deadlineMs)return true;
        if(completed.keyed) return std::any_of(completed.actions.begin(),completed.actions.end(),[&](const auto& c){
            return c.runId==s.runId && c.floorIndex==s.floorIndex && c.round==v.round &&
                c.actorId==v.actorId && c.actionId==v.id;
        });
        return v.actorId==0?completed.player:
            v.actorId==kCompanionActorId?completed.partner:completed.enemy;
    };
    auto impactKey=[&](const PartyAction& a) {
        return "party-impact:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
            std::to_string(a.round)+":"+std::to_string(a.actorId)+":"+std::to_string(a.id);
    };
    // Work from copies: an impact may replace the target's playback. Tokens
    // identify a particular actor/clip, so an old completion cannot finish it.
    const auto playing=g.partyPlaybacks;
    for(const auto& v:playing) {
        const auto* current=partyPlayback(g,v.actorId);
        if(!current || current->id!=v.id || !completedPlayback(v))continue;
        g.partyPlaybacks.erase(std::remove_if(g.partyPlaybacks.begin(),g.partyPlaybacks.end(),
            [&](const auto& x){return x.actorId==v.actorId && x.id==v.id;}),g.partyPlaybacks.end());
        changed=true;
        if(v.clip==1) {
            auto a=std::find_if(g.partyActions.begin(),g.partyActions.end(),
                [&](const auto& x){return x.id==v.id && x.actorId==v.actorId;});
            if(a==g.partyActions.end() || a->finished)continue;
            a->finished=true;
            if(!g.onceKeys.insert(impactKey(*a)).second)throw std::logic_error("duplicate party impact");
            if(!a->hit)continue;
            const auto target=a->targetId;
            uint32_t left=0;
            if(!target){g.hearts-=std::min(g.hearts,a->damage);left=g.hearts;}
            else if(target==kCompanionActorId) {
                g.companion.hearts-=std::min(g.companion.hearts,a->damage);left=g.companion.hearts;
            } else {
                auto* e=entity(target);if(!e)throw std::logic_error("party target missing");
                e->remainingHits-=std::min(e->remainingHits,a->damage);left=e->remainingHits;
                if(g.enemyInstance==target)g.enemyHits=left;
            }
            playback(target,left?6:33,direction(target));
            event(left?GameplayEvent::Hurt:target==0?GameplayEvent::PlayerFaint:
                target==kCompanionActorId?GameplayEvent::CompanionFaint:GameplayEvent::EnemyFaint,
                target,a->actorId,a->id,a->damage,a->critical);
        } else if(v.clip==33) {
            if(v.actorId==0 || v.actorId==kCompanionActorId) {
                g.playerHidden=!g.hearts;g.companion.hidden=!g.companion.hearts;
                g.defeated=true;g.life=LifePhase::Death;g.deathStage=DeathStage::FadeOut;
                g.phaseStartedMs=now;g.deadlineMs=now+750;invalidate();s.phase=Phase::Home;
                g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
                g.walkStartedMs=g.companion.walkStartedMs=0;defeated=true;
                event(GameplayEvent::CombatDefeat,v.actorId,0,v.id);break;
            }
            if(auto* e=entity(v.actorId);e && !e->done) {
                e->done=true;s.onceKeys.insert(key("greet",e->instanceId));
                g.onceKeys.insert("victory:"+key("combat",e->instanceId));
                invalidate(s.targetInstance==e->instanceId);
                event(GameplayEvent::CombatVictory,e->instanceId,0,v.id);
            }
        }
    }
    if(defeated)return result;
    for(auto& a:g.partyActions) if(!a.finished && a.kind!=PartyActionKind::Attack && now>=a.deadlineMs) {
        a.finished=true;changed=true;
    }
    auto available=[&](uint32_t actor) {
        if(!actor)return g.hearts>0;
        if(actor==kCompanionActorId)return g.companion.enabled && g.companion.hearts>0;
        auto* e=entity(actor);return e && hostile(*e);
    };
    auto pending=[&](uint32_t actor) {
        return std::any_of(g.partyActions.begin(),g.partyActions.end(),
            [&](const auto& a){return a.actorId==actor && !a.finished;});
    };
    auto start=[&](uint32_t actor,PartyActionKind kind,uint32_t target,uint64_t duration) {
        PartyAction a;a.id=id();a.round=g.actionRound;a.actorId=actor;a.targetId=target;
        a.kind=kind;a.startedMs=now;
        if(duration>UINT64_MAX-now)throw std::runtime_error("party action clock overflow");
        a.deadlineMs=now+duration;a.finished=!duration && kind!=PartyActionKind::Attack;
        consumed(actor)=g.actionRound;
        if(kind==PartyActionKind::Attack) {
            const auto roll=rollBasicAttack(s.rng,actor==0 || actor==kCompanionActorId);
            a.hit=roll.hit;a.critical=roll.critical;a.damage=roll.damage;
            const auto once="party-attack:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
                std::to_string(a.round)+":"+std::to_string(actor);
            if(!g.onceKeys.insert(once).second)throw std::logic_error("duplicate party action");
            direction(actor)=directionBetween(position(actor),position(target));
            playback(actor,1,direction(actor),a.id);
            const auto foe=actor==0 || actor==kCompanionActorId?target:actor;
            g.enemyInstance=foe;if(auto* e=entity(foe))g.enemyHits=e->remainingHits;
            g.enemyTargetsCompanion=target==kCompanionActorId;
            g.hit=a.hit;g.critical=a.critical;g.damage=a.damage;
            g.combat=actor==0?CombatPhase::PlayerAttack:
                actor==kCompanionActorId?CombatPhase::CompanionAttack:CombatPhase::EnemyAttack;
            event(actor==0?GameplayEvent::PlayerAttack:
                actor==kCompanionActorId?GameplayEvent::CompanionAttack:GameplayEvent::EnemyAttack,
                actor,target,a.id,a.damage,a.critical);
        }
        if(actor==0 && kind!=PartyActionKind::Move)g.walkStartedMs=0;
        if(actor==kCompanionActorId && kind!=PartyActionKind::Move)g.companion.walkStartedMs=0;
        g.partyActions.push_back(a);changed=true;return a.id;
    };
    auto move=[&](uint32_t actor,int from,int to) {
        const auto action=start(actor,PartyActionKind::Move,0,400);
        const auto facing=directionBetween(from,to);direction(actor)=facing;
        MoveRecord record{g.actionRound,now,400,facing,from,to};
        if(!actor) {
            g.movement=record;if(!g.walkStartedMs)g.walkStartedMs=now;
        } else if(actor==kCompanionActorId) {
            g.companion.tile=to;g.companion.movement=record;
            if(!g.companion.walkStartedMs)g.companion.walkStartedMs=now;
            ++s.occupancyVersion;
        } else {
            auto* e=entity(actor);e->tile=to;e->movement=record;++s.occupancyVersion;
        }
        invalidate(false);
        event(!actor?GameplayEvent::Move:actor==kCompanionActorId?
            GameplayEvent::CompanionMove:GameplayEvent::ActorMove,actor,0,action);
    };
    auto heal=[&](uint32_t actor) {
        auto& hp=actor==0?g.hearts:g.companion.hearts;
        auto& berry=actor==0?g.oran:g.companion.oran;
        if(!berry || hp+2>kMaxHp)return false;
        --berry;hp+=2;const auto action=start(actor,PartyActionKind::Berry,0,1000);
        const auto once="party-heal:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
            std::to_string(g.actionRound)+":"+std::to_string(actor);
        if(!g.onceKeys.insert(once).second)throw std::logic_error("duplicate party heal");
        event(GameplayEvent::UseOranBerry,actor,0,action);return true;
    };
    auto seen=[&](int from,int to) {
        const int room=s.world.room[from];
        if(room>=0 && room==s.world.room[to])return true;
        const auto sight=bfs(s.world,from);return sight.distance[to]>=0 && sight.distance[to]<=2;
    };
    auto nearestEnemy=[&](uint32_t actor,bool adjacent)->uint32_t {
        uint32_t best=0;int distance=std::numeric_limits<int>::max();
        const int from=position(actor);const auto reach=bfs(s.world,from);
        for(const auto& e:s.population.entities) {
            if(!hostile(e) || (adjacent?!canAttack(s.world,from,e.tile):!seen(from,e.tile)))continue;
            const int d=reach.distance[e.tile];
            if(d>=0 && (d<distance || (d==distance && (!best || e.instanceId<best)))) {
                best=e.instanceId;distance=d;
            }
        }
        return best;
    };
    auto approach=[&](uint32_t actor,int target)->int {
        const int from=position(actor);
        auto blocked=occupancy();blocked[s.position]=1;
        blocked[g.companion.tile]=1;blocked[from]=0;
        const auto reach=bfs(s.world,from,blocked);
        int goal=-1,best=kTiles+1;
        for(int tile=0;tile<kTiles;++tile) {
            if(tile==s.world.stairs || !canAttack(s.world,tile,target) || reach.distance[tile]<=0)continue;
            if(reach.distance[tile]<best) {goal=tile;best=reach.distance[tile];}
        }
        if(goal<0)return -1;
        const auto path=reach.pathTo(goal);return path.empty()?-1:path.front();
    };
    auto planCompanion=[&] {
        const auto actor=kCompanionActorId;
        if(!available(actor) || consumed(actor)==g.actionRound || partyPlayback(g,actor))return;
        if(heal(actor))return;
        auto* target=entity(g.companionTarget);
        if(g.companionIntent==CompanionIntent::Engage && target && !target->done && !target->remainingHits) {
            start(actor,PartyActionKind::Wait,0,0);return;
        }
        if(g.companionIntent==CompanionIntent::Engage && (!target || !hostile(*target))) {
            g.companionIntent=CompanionIntent::Return;g.companionTarget=0;changed=true;
        }
        if(g.companionIntent==CompanionIntent::Return &&
           canAttack(s.world,s.position,g.companion.tile)) {
            g.companionIntent=CompanionIntent::Follow;changed=true;
        }
        if(g.companionIntent==CompanionIntent::Follow) {
            if(!canAttack(s.world,s.position,g.companion.tile)) {
                g.companionIntent=CompanionIntent::Return;changed=true;
            } else if(const auto foe=nearestEnemy(actor,false)) {
                g.companionTarget=foe;g.companionIntent=CompanionIntent::Engage;
                target=entity(foe);changed=true;
            }
        }
        if(g.companionIntent==CompanionIntent::Engage && target && hostile(*target)) {
            if(canAttack(s.world,g.companion.tile,target->tile)) {
                start(actor,PartyActionKind::Attack,target->instanceId,5000);return;
            }
            const int next=approach(actor,target->tile);
            if(next>=0){move(actor,g.companion.tile,next);return;}
            g.companionIntent=CompanionIntent::Return;g.companionTarget=0;changed=true;
        }
        int next=-1;
        if(g.companionIntent==CompanionIntent::Return) next=approach(actor,s.position);
        else if(g.movement.actionId==g.actionRound && g.movement.from!=g.companion.tile) {
            auto blocked=occupancy();blocked[s.position]=1;blocked[g.companion.tile]=0;
            if(canAttack(s.world,g.movement.from,s.position) &&
               canMove(s.world,g.companion.tile,g.movement.from,blocked))next=g.movement.from;
        }
        if(next>=0)move(actor,g.companion.tile,next);
        else start(actor,PartyActionKind::Wait,0,0);
    };
    auto planPlayer=[&] {
        if(!available(0) || consumed(0)==g.actionRound || partyPlayback(g,0))return;
        if(heal(0))return;
        const auto adjacent=nearestEnemy(0,true);
        if(adjacent && g.hearts>kLowHp) {
            start(0,PartyActionKind::Attack,adjacent,5000);return;
        }
        if(s.position==s.world.stairs) {start(0,PartyActionKind::Wait,0,400);return;}
        // Actor action rights affect occupancy. Rebuild at the decision boundary
        // so an in-memory path and a restored path cannot use different rounds.
        invalidate(false);plan();
        const auto signature=navigationFingerprint(s);
        const auto repeats=static_cast<unsigned>(std::count(g.navigationHistory.begin(),g.navigationHistory.end(),signature));
        if(g.navigationHistory.size()==32)g.navigationHistory.erase(g.navigationHistory.begin());
        g.navigationHistory.push_back(signature);
        int detour=-1;
        if(repeats && !(s.target==s.position && s.goalKind==GoalKind::Item))
            detour=detourTile(repeats,signature);
        if(s.target<0 && detour<0) {
            if(adjacent)start(0,PartyActionKind::Attack,adjacent,5000);
            else {const auto action=start(0,PartyActionKind::Wait,0,400);event(GameplayEvent::Wait,0,0,action);}
            return;
        }
        if(s.goalKind==GoalKind::Friend) {
            auto* npc=entity(s.targetInstance);
            if(npc && friendly(*npc) && !npc->done && canAttack(s.world,s.position,npc->tile)) {
                start(0,PartyActionKind::Wait,0,0);
                g.enemyInstance=npc->instanceId;g.dialogueStage=DialogueStage::Player;
                g.playerDirection=directionBetween(s.position,npc->tile);
                g.life=LifePhase::Dialogue;g.phaseStartedMs=now;g.deadlineMs=now+1200;
                event(GameplayEvent::DialoguePlayer,0,npc->instanceId,0);return;
            }
        }
        const int from=s.position,oldPartner=g.companion.tile;
        const auto step=detour>=0?moveTo(detour):advance();
        if(step.kind==EventKind::ReturnedHome && !s.completed) {
            s.phase=Phase::Explore;invalidate();
            if(adjacent)start(0,PartyActionKind::Attack,adjacent,5000);
            else {const auto action=start(0,PartyActionKind::Wait,0,400);event(GameplayEvent::Wait,0,0,action);}
            return;
        }
        if(step.kind==EventKind::Move) {
            move(0,from,s.position);
            if(g.companion.tile!=oldPartner)move(kCompanionActorId,oldPartner,g.companion.tile);
            result.navigationDetour=detour>=0;return;
        }
        if(step.kind==EventKind::Pickup) {
            const auto action=start(0,PartyActionKind::Wait,0,400);
            if(step.originalId==ITEM_ORAN_BERRY) {
                auto& p=g.companion;
                if(!p.oran && (g.oran || p.hearts<g.hearts))++p.oran;
                else if(!g.oran)++g.oran;
            }
            event(GameplayEvent::Pickup,0,step.originalId,action);result.onceKey=step.onceKey;return;
        }
        if(step.kind==EventKind::Pose) {
            g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
            g.life=LifePhase::Pose;g.phaseStartedMs=now;g.deadlineMs=now+5000;
            event(GameplayEvent::Pose,0,0,0);result.onceKey=step.onceKey;return;
        }
        const auto action=start(0,PartyActionKind::Wait,0,400);
        event(GameplayEvent::Wait,0,0,action);
    };
    auto planEnemy=[&](Entity& e,bool early) {
        if(!hostile(e) || e.actionRound==g.actionRound || partyPlayback(g,e.instanceId))return;
        const int target=nearestPartyTile(e);
        const bool inSight=seen(e.tile,target);
        if(inSight){e.tracking=Tracking::Chase;e.lastSeen=target;e.chaseTarget=target;}
        if(canAttack(s.world,e.tile,target)) {
            if(!early)start(e.instanceId,PartyActionKind::Attack,
                target==s.position?0:kCompanionActorId,5000);
            return;
        }
        if(e.tracking==Tracking::Chase && e.lastSeen>=0) {
            auto blocked=occupancy();blocked[e.tile]=0;blocked[s.position]=1;
            blocked[g.companion.tile]=1;
            const auto reach=bfs(s.world,e.tile,blocked);
            int goal=-1,best=kTiles+1;
            for(int tile=0;tile<kTiles;++tile)
                if((tile==e.lastSeen || (inSight && canAttack(s.world,tile,e.lastSeen))) &&
                   reach.distance[tile]>0 && reach.distance[tile]<best) {
                    best=reach.distance[tile];goal=tile;
                }
            if(goal>=0) {
                const auto path=reach.pathTo(goal);const int from=e.tile;
                e.blockedTurns=0;move(e.instanceId,from,path.front());return;
            }
            if(e.tile==e.lastSeen || (e.blockedTurns=std::min(3U,e.blockedTurns+1))>=3) {
                e.tracking=Tracking::Idle;e.chaseTarget=-1;
            }
        }
        start(e.instanceId,PartyActionKind::Wait,0,0);
    };
    // One call may finish a phase and start the next, but zero-duration waits
    // cannot create an unbounded update loop.
    for(unsigned transition=0;transition<4;++transition) {
        if(!g.hearts || !g.companion.hearts)break;
        if(g.partyPhase==PartyPhase::Idle) {
            if(s.position==s.world.stairs && g.companionIntent!=CompanionIntent::Engage &&
               canAttack(s.world,s.position,g.companion.tile)) {
                invalidate();g.movement={};g.companion.movement={};
                g.walkStartedMs=g.companion.walkStartedMs=0;
                g.partyActions.clear();g.partyPlaybacks.clear();
                g.life=LifePhase::Explore;g.sceneBeat=SceneBeat::FloorFadeOut;
                g.phaseStartedMs=now;g.deadlineMs=now+kFloorFadeMs;return result;
            }
            g.partyPhase=PartyPhase::Allies;changed=true;
        }
        if(g.partyPhase==PartyPhase::Allies) {
            planPlayer();
            if(g.life==LifePhase::Dialogue || g.life==LifePhase::Pose)return result;
            planCompanion();
            const bool attacking=std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){
                return (a.actorId==0 || a.actorId==kCompanionActorId) && a.kind==PartyActionKind::Attack;
            });
            if(!attacking)for(auto& e:s.population.entities)planEnemy(e,true);
            const bool alliesDone=g.playerActionRound==g.actionRound &&
                g.companion.actionRound==g.actionRound && !pending(0) && !pending(kCompanionActorId);
            if(!alliesDone)break;
            g.partyPhase=PartyPhase::Enemies;changed=true;
        }
        if(g.partyPhase==PartyPhase::Enemies) {
            for(auto& e:s.population.entities)planEnemy(e,false);
            bool done=true;
            for(const auto& e:s.population.entities)
                if(hostile(e) && (e.actionRound!=g.actionRound || pending(e.instanceId)))done=false;
            if(!done)break;
            if(g.actionRound==UINT64_MAX)throw std::runtime_error("action round exhausted");
            ++g.actionRound;g.partyActions.clear();g.partyPhase=PartyPhase::Idle;
            invalidate(false);changed=true;
        }
    }
    const bool fighting=!g.partyPlaybacks.empty() ||
        std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){
            return a.kind==PartyActionKind::Attack && !a.finished;
        });
    const auto life=fighting?LifePhase::Combat:LifePhase::Explore;
    if(g.life!=life){g.life=life;changed=true;}
    if(auto* e=entity(g.enemyInstance))g.enemyHits=e->remainingHits;
    uint64_t deadline=UINT64_MAX;
    for(const auto& a:g.partyActions)if(!a.finished)deadline=std::min(deadline,a.deadlineMs);
    for(const auto& v:g.partyPlaybacks)deadline=std::min(deadline,v.deadlineMs);
    if(deadline==UINT64_MAX)deadline=now+400;
    if(g.deadlineMs!=deadline){g.deadlineMs=deadline;changed=true;}
    if(changed)g.phaseStartedMs=now;
    return result;
}
} // namespace pet
