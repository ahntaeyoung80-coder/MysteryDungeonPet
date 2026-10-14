#include "core/exploration.hpp"
#include "core/exploration_package.hpp"
#include "core/gameplay_scene.hpp"
#include "core/guild_scene.hpp"
#include "core/animator.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0, restores = 0;
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void pass(int scenario, const char* message) { std::cout << "{\"scenario\":" << scenario << ",\"status\":\"pass\",\"evidence\":\"" << message << "\"}\n"; }
pet::GameplayUpdate advance(pet::Explorer& e, std::uint64_t now, bool done = false) {
    auto copy = pet::Explorer(pet::decodeSnapshot(pet::encodeSnapshot(e.state()), e.state().assetVersion));
    const auto expected = e.advanceGameplay(now, done), actual = copy.advanceGameplay(now, done);
    check(expected.event == actual.event && expected.onceKey == actual.onceKey &&
          pet::encodeSnapshot(e.state()) == pet::encodeSnapshot(copy.state()), "restore replay differs");
    ++restores;
    return expected;
}
pet::Explorer empty() {
    auto floors = pet::fixtureProfiles(1); floors[0].guaranteed.clear();
    pet::Explorer e(100, 42, "gameplay-fixture", floors, {0,0,0,0});
    e.enableGameplay(1000); return e;
}
pet::Explorer encounter(bool hostile = true) {
    auto e = empty(); auto s = e.state();
    int tile = -1;
    const auto search = pet::bfs(s.world, s.position);
    for (int p = 0; p < pet::kTiles; ++p)
        if (search.distance[p] == 1 && p != s.world.stairs) { tile = p; break; }
    check(tile != -1, "no adjacent encounter fixture");
    s.population.entities.push_back({1, 4, pet::EntityKind::Friend, tile, false});
    s.gameplay.hostile = hostile ? std::vector<std::uint32_t>{1} : std::vector<std::uint32_t>{};
    s.gameplay.deadlineMs = 1000;
    return pet::Explorer(s);
}
void forceRoll(pet::Explorer& e, bool player, bool hit, bool critical) {
    auto s = e.state();
    for (std::uint64_t seed = 0; seed < 10000; ++seed) {
        pet::Rng rng{seed}; const auto roll = pet::rollBasicAttack(rng, player);
        if (roll.hit == hit && roll.critical == critical) { s.rng.state = seed; e = pet::Explorer(s); return; }
    }
    throw std::runtime_error("roll seed not found");
}
pet::GameplayUpdate next(pet::Explorer& e, bool done = true) {
    return advance(e, e.state().gameplay.deadlineMs, done);
}
void enemyTurn(pet::Explorer& e, bool hit = false, bool crit = false) {
    using namespace pet;
    if (e.state().gameplay.combat == CombatPhase::PlayerAttack) next(e);
    if (e.state().gameplay.combat == CombatPhase::EnemyHurt) next(e);
    check(e.state().gameplay.combat == CombatPhase::EnemyTurn, "enemy turn missing");
    forceRoll(e, false, hit, crit);
    check(next(e).event == GameplayEvent::EnemyAttack, "enemy attack missing");
    next(e);
    if (e.state().gameplay.combat == CombatPhase::PlayerHurt) next(e);
}
void homePipeline(pet::Explorer& e, bool defeat) {
    using namespace pet;
    check(e.state().gameplay.life == LifePhase::ReturnHome, "return state missing");
    const auto start = e.state().gameplay.phaseStartedMs;
    const auto position = e.state().position, target = e.state().target;
    const auto plans = e.bfsCalls(); const auto moves = e.state().floorSteps;
    check(target == -1, "return retains a dungeon target");
    if(e.state().gameplay.fadeInOnly) {
        check(e.state().gameplay.homeScene,"defeat target scene not ready for fade-in");
        next(e);check(e.state().gameplay.life==LifePhase::Emotion,"defeat fade-in stuck");
    } else {
    for (auto time : {start, start+500, start+749}) {
        advance(e, time);
        check(e.state().position == position && e.bfsCalls() == plans && e.state().floorSteps == moves, "return walks/replans");
    }
    check(advance(e, start+750).event == GameplayEvent::HomeScene, "home scene missing");
    check(e.state().gameplay.homeScene, "home is transparent/dungeon scene retained");
    advance(e, start+1500);
    check(e.state().gameplay.life == (defeat ? LifePhase::Emotion : LifePhase::Eat), "return exceeds 1500ms");
    }
    if (defeat) check(next(e).event == GameplayEvent::Eat, "defeat emotion does not lead to eating");
    const auto meals = e.state().gameplay.meals;
    advance(e, e.state().gameplay.phaseStartedMs+1);
    check(e.state().gameplay.meals == meals && meals == 1, "meal consumed twice");
    advance(e, e.state().gameplay.phaseStartedMs+1000, true);
    check(e.state().gameplay.life == LifePhase::Eat && gameplayLoop(e.state().gameplay), "communal meal ended after one Eat clip");
    check(e.state().gameplay.deadlineMs-e.state().gameplay.phaseStartedMs==7000,"meal not shortened by one second");
    check(next(e).event==GameplayEvent::Sleep && e.state().gameplay.sceneBeat==SceneBeat::DiningExit,"meal exit fade missing");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::NightFadeIn && bedroomScene(e.state().gameplay) && nightScene(e.state().gameplay),"bedtime exposed day background");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::BedFace && gameplayClip(e.state().gameplay)==22,"bedtime greeting missing");
    e.advanceGameplay(e.state().gameplay.phaseStartedMs+1,ActorCompletion{true,false,false});check(e.state().gameplay.sceneBeat==SceneBeat::BedFace,"bedtime greeting skipped partner");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::BedLay && gameplayClip(e.state().gameplay)==27,"laying animation missing");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::Sleeping && gameplayClip(e.state().gameplay)==13 && !gameplayLoop(e.state().gameplay),"sleep onset must be one finite animation");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::BedFadeOut && nightScene(e.state().gameplay),"sleep onset did not fade directly");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::BedFadeIn && !nightScene(e.state().gameplay),"morning fade-in exposed night");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::Waking && gameplayClip(e.state().gameplay)==14,"Wake14 daylight missing");
    const auto oldDay=e.state().gameplay.day;const auto oldHearts=e.state().gameplay.hearts;
    e.advanceGameplay(e.state().gameplay.phaseStartedMs+1,ActorCompletion{false,true,true});check(e.state().gameplay.life==LifePhase::Wake,"other actor finished player Wake");
    e.advanceGameplay(e.state().gameplay.phaseStartedMs+2,ActorCompletion{true,false,false});check(e.state().gameplay.life==LifePhase::Wake,"partner Wake skipped");
    e.advanceGameplay(e.state().gameplay.phaseStartedMs+3,ActorCompletion{true,false,true});check(e.state().gameplay.sceneBeat==SceneBeat::MorningFace && gameplayClip(e.state().gameplay)==22,"morning greeting missing");
    e.advanceGameplay(e.state().gameplay.phaseStartedMs+1,ActorCompletion{true,false,false});check(e.state().gameplay.sceneBeat==SceneBeat::MorningFace,"greeting did not wait for partner");
    next(e);check(e.state().gameplay.sceneBeat==SceneBeat::RoomExit && gameplayClip(e.state().gameplay)==0,"greeting did not lead to Walk0 exit");
    const auto exitPlans=e.bfsCalls(),exitRng=e.state().rng.state;const auto exitInventory=e.state().inventory.size();
    advance(e,e.state().gameplay.deadlineMs-1,true);check(e.state().gameplay.day==oldDay && e.state().gameplay.hearts==oldHearts && e.bfsCalls()==exitPlans && e.state().rng.state==exitRng && e.state().inventory.size()==exitInventory,"room exit advanced dungeon or refreshed state early");
    advance(e,e.state().gameplay.deadlineMs,false);check(e.state().gameplay.sceneBeat==SceneBeat::RoomExit,"exit ignored completion");
    next(e);check(e.state().gameplay.day==oldDay+1 && e.state().gameplay.hearts==kMaxHp && e.state().gameplay.sceneBeat==SceneBeat::None && e.state().gameplay.life==LifePhase::Explore,"next day not committed once after exit");


}
}
int main(int argc, char** argv) {
    try {
        using namespace pet;
        if (argc < 2) throw std::runtime_error("gameplay assets required");
        const auto player = loadPackage(std::filesystem::path(argv[1]) / "actors/asset-0/asset.mdpa");
        const auto enemy = loadPackage(std::filesystem::path(argv[1]) / "actors/asset-1/asset.mdpa");
        const auto home = loadPackage(std::filesystem::path(argv[1]) / "home/home.mdpa");
        for (auto clip : {0U,1U,5U,6U,7U,14U,15U,17U,22U,33U})
            for (unsigned d = 0; d < 8; ++d) { check(!player.frames(clip,d).empty(), "original player clip missing"); check(!enemy.frames(clip,d).empty(), "original enemy clip missing"); }
        check(home.frames(0,0).front().rect.w == 504 && home.frames(0,0).front().rect.h == 360 && home.frames(2,0).front().rect.w==384, "home map not linked");
        const auto rooms = loadGuildScene(std::filesystem::path(argv[1]) / "guild/guild.mdgs");
        check(rooms.size() == 2 && rooms[0].actors.size() == 13 && rooms[1].actors.size() == 2, "original guild cast not connected");
        check(rooms[0].player().position.x == 220 && rooms[0].player().position.y == 184 && rooms[0].player().direction == 0, "dining player SSA placement lost");
        check(rooms[1].player().position.x == 204 && rooms[1].player().position.y == 184 && rooms[1].player().direction == 2, "player not in original right bed");
        const auto partner = std::find_if(rooms[1].actors.begin(), rooms[1].actors.end(), [](const auto& a) { return a.kind == GuildActorKind::Partner; });
        check(partner != rooms[1].actors.end() && partner->position.x == 156 && partner->position.y == 184 && partner->direction == 6, "partner not in original left bed");
        unsigned guildMembers = 0, changing = 0;
        for (const auto& actor : rooms[0].actors) if (actor.kind == GuildActorKind::GuildMember || actor.kind == GuildActorKind::Object) {
            const auto asset = loadPackage(std::filesystem::path(argv[1]) / actor.assetFile);
            check(!asset.frames(actor.actionClip, actor.direction).empty(), "guild original clip missing");
            std::uint64_t ticks=0;for(const auto& frame:asset.frames(actor.actionClip,actor.direction)) ticks+=frame.ticks;
            const auto& rate=asset.resolve(actor.actionClip);
            check(ticks*1000*rate.speedDenominator <= 7000ULL*asset.tickHz*rate.speedNumerator,"meal cuts a communal animation cycle");
            Animator animation; animation.start(asset, actor.actionClip, actor.direction, true, 0);
            const auto initial = animation.advance(0).frame;
            bool changed = false;
            for (std::int64_t time = 100000; time < 3000000; time += 100000) changed = changed || animation.advance(time).frame != initial;
            guildMembers += actor.kind == GuildActorKind::GuildMember;
            changing += changed && actor.kind == GuildActorKind::GuildMember;
        }
        check(guildMembers == 10 && changing >= 8, "guild meal is a static background");
        std::cout << "{\"guildMembers\":" << guildMembers << ",\"animatedGuildMembers\":" << changing << ",\"bedPlacement\":\"pass\"}\n";

        auto clear = empty(); auto s = clear.state();
        s.position = s.world.stairs; s.visited = s.world.terrain;
        clear = Explorer(s);
        advance(clear,1000);
        check(advance(clear,2000).event == GameplayEvent::Pose && gameplayClip(clear.state().gameplay) == 17, "stairs missing Pose17");
        advance(clear,2001); check(clear.state().gameplay.life == LifePhase::Pose, "Pose repeated");
        Animator pose; pose.start(player,17,0,false,0);
        std::int64_t at = 0; Advance result;
        do { result = pose.advance(at); if (result.nextDeadlineUs) at = *result.nextDeadlineUs; } while (!result.finished);
        check(advance(clear,2000+static_cast<std::uint64_t>(at/1000),true).event == GameplayEvent::ReturnHome, "Pose completion missing return");
        homePipeline(clear,false);
        pass(1,"actual Pose17 and communal Eat15 EventSleep13 Wake14; next day Explore");
        pass(2,"return 1500ms; zero position/floorStep/BFS change; cached target cleared");

        auto fight = encounter(); check(advance(fight,1000).event == GameplayEvent::HostileEncounter, "recognition did not start combat");
        check(fight.state().gameplay.combat == CombatPhase::PlayerTurn, "enemy acts first");
        const auto paused = encodeSnapshot(fight.state()); const auto pausedPlans = fight.bfsCalls();
        fight.advance(); check(encodeSnapshot(fight.state()) == paused && fight.bfsCalls() == pausedPlans, "legacy tick moves during combat");
        forceRoll(fight,true,true,false); check(next(fight).event == GameplayEvent::PlayerAttack && fight.state().gameplay.enemyHits == 1, "normal attack damage incorrect");
        check(gameplayClip(fight.state().gameplay) == 1, "Attack1 missing");
        enemyTurn(fight);
        check(fight.state().gameplay.combat == CombatPhase::PlayerTurn, "counterattack does not return player turn");
        pass(3,"recognition -> player Move355 Attack1 -> enemy Move355 counterattack");
        const auto floor = fight.state().floorIndex; const auto position = fight.state().position;
        forceRoll(fight,true,true,false); next(fight); next(fight); next(fight);
        check(fight.state().gameplay.combat == CombatPhase::Faint, "two normal hits missing Faint33");
        advance(fight,fight.state().gameplay.phaseStartedMs+1);
        check(fight.state().gameplay.combat == CombatPhase::Faint && !fight.state().population.entities[0].done, "victory before Faint ends");
        check(next(fight).event == GameplayEvent::CombatVictory, "Faint completion missing victory");
        check(next(fight).event == GameplayEvent::Explore && fight.state().position == position && fight.state().floorIndex == floor && !fight.state().gameplay.homeScene, "victory returned home/moved floor");
        pass(4,"two normal player hits -> enemy Faint33 completion -> Explore");
        pass(11,"victory preserves original floor and tile; no ReturnHome transaction");

        auto crit = encounter(); advance(crit,1000); forceRoll(crit,true,true,true);
        const auto critical = next(crit);
        check(critical.critical && critical.damage == 2 && crit.state().gameplay.enemyHits == 0, "player critical not double damage");
        pass(5,"player critical applies two damage and removes two internal hit counts");

        auto dead = encounter(); {auto start=dead.state();start.gameplay.hearts=3;dead=Explorer(start);} advance(dead,1000);
        for (unsigned i = 0; i < 3; ++i) {
            // Force an unavoidable enemy attack to test death playback independently of low-HP escape AI.
            auto attacked=dead.state();attacked.gameplay.life=LifePhase::Combat;attacked.gameplay.combat=CombatPhase::EnemyTurn;
            attacked.gameplay.enemyInstance=1;attacked.gameplay.playerActionRound=attacked.gameplay.actionRound;
            attacked.population.entities[0].actionRound=attacked.gameplay.actionRound-1;dead=Explorer(attacked);
            enemyTurn(dead,true);
            check(dead.state().gameplay.hearts == 2-i, "normal enemy hit not one heart");
        }
        check(dead.state().gameplay.combat == CombatPhase::PlayerFaint, "third enemy hit missing death");
        next(dead);check(dead.state().gameplay.life==LifePhase::Death && dead.state().gameplay.playerHidden,"Faint did not hide player");
        next(dead);check(dead.state().gameplay.deathStage==DeathStage::Blackout,"opaque blackout missing");
        next(dead);homePipeline(dead,true);
        pass(6,"three enemy normal hits -> defeat -> return -> expression -> meal -> sleep -> wake");
        auto enemyCrit = encounter(); advance(enemyCrit,1000); forceRoll(enemyCrit,true,false,false); next(enemyCrit); enemyTurn(enemyCrit,true,true);
        check(enemyCrit.state().gameplay.hearts == kMaxHp-2 && enemyCrit.state().gameplay.damage == 2, "enemy critical not two hearts");
        pass(7,"enemy critical removes two hearts");

        auto berries = empty(); s = berries.state(); s.gameplay.hearts = 1;
        const auto search = bfs(s.world,s.position);
        int near = -1, far = -1;
        for (int p = 0; p < kTiles; ++p) if (p != s.world.stairs && search.distance[p] > 0) {
            if (near == -1 || search.distance[p] < search.distance[near]) near = p;
            if (far == -1 || search.distance[p] > search.distance[far]) far = p;
        }
        s.population.entities = {{1,10,EntityKind::Item,near,false}, {2,70,EntityKind::Item,far,false}};
        s.knownStairs=s.world.stairs;s.discovered[s.world.stairs]=1;
        berries=Explorer(s);advance(berries,1000);
        check(berries.goal()==GoalKind::Stairs,"low HP did not escape to stairs discovered before berry");
        pass(8,"low HP retreats to previously discovered stairs before searching for an unseen berry");
        auto heal = encounter(); advance(heal,1000); s = heal.state(); s.gameplay.hearts = 1; s.gameplay.oran = 1; heal = Explorer(s);
        check(next(heal).event == GameplayEvent::UseOranBerry && heal.state().gameplay.hearts == 3 && heal.state().gameplay.oran == 0, "Oran did not heal two/cap/consume");
        check(gameplayClip(heal.state().gameplay) == 7 && heal.state().gameplay.combat == CombatPhase::BerryText, "berry plays use animation");
        const auto rng = heal.state().rng.state;
        advance(heal,heal.state().gameplay.phaseStartedMs+1,true);
        check(heal.state().rng.state == rng && heal.state().gameplay.combat == CombatPhase::BerryText, "berry text consumed second turn");
        next(heal); forceRoll(heal,false,true,false); next(heal);
        check(heal.state().gameplay.combat == CombatPhase::EnemyAttack && heal.state().gameplay.hearts == 3, "Oran skipped enemy turn or damage preceded attack completion");
        pass(9,"text only; one berry consumed; 1->3 hearts; enemy turn follows");
        auto friendly = encounter(false);auto onlyContact=friendly.state();onlyContact.discovered=onlyContact.world.terrain;onlyContact.knownStairs=onlyContact.world.stairs;onlyContact.blocked[onlyContact.world.stairs]=1;friendly=Explorer(onlyContact); const auto friendlyPosition = friendly.state().position;
        check(advance(friendly,1000).event==GameplayEvent::DialoguePlayer && friendly.state().gameplay.life==LifePhase::Dialogue && !friendly.state().population.entities[0].done,"friendly dialogue starts done/overlapped");
        auto dialogSave=encodeSnapshot(friendly.state());advance(friendly,1100,true);check(encodeSnapshot(friendly.state())==dialogSave,"dialogue pacing skipped");
        check(next(friendly).event==GameplayEvent::DialogueNpc && !friendly.state().population.entities[0].done,"NPC response skipped");
        check(next(friendly).event==GameplayEvent::Explore && friendly.state().population.entities[0].done && friendly.state().position==friendlyPosition,"dialogue completion missing");
        pass(10,"friendly waits; adjacent greeting; no attack; Explore resumes");

        // Upgrade an authentic MDES1 layout (MDES2's default extension is 84 bytes).
        Explorer legacy(200,42,"legacy",fixtureProfiles(1));
        auto v1=readFile(std::filesystem::path(argv[1]).parent_path().parent_path().parent_path()/"tests/fixtures/legacy-v1.mdes",1048576);
        Explorer upgraded(decodeSnapshot(v1,"legacy")); upgraded.enableGameplay(1000);
        check(upgraded.state().gameplay.enabled && upgraded.state().position == legacy.state().position,"MDES1 migration lost position");
        // Exercise unforced production rolls across repeated days and restoration.
        for (unsigned seed=1;seed<=20;++seed) {
            auto floors=fixtureProfiles(3); floors[0].guaranteed={70};
            Explorer run(seed,seed,"days",floors); run.enableGameplay(1000);
            std::uint64_t clock=1000;
            for (unsigned step=0;run.state().gameplay.day<3;++step) {
                check(step<10000,"multi-day run stuck");
                clock=std::max(clock+350,run.state().gameplay.deadlineMs);
                advance(run,clock,true);
            }
            check(run.state().gameplay.hearts==kMaxHp && run.state().gameplay.oran<=1,"multi-day inventory/hearts violated");
        }

        // Probability audit exercises the same persisted RNG and relationship assignment as production.
        Rng rolls{42}; unsigned playerHits=0, enemyHits=0, playerCrits=0, enemyCrits=0;
        for (unsigned i=0;i<100000;++i) {
            const auto p=rollBasicAttack(rolls,true), e=rollBasicAttack(rolls,false);
            playerHits+=p.hit; enemyHits+=e.hit; playerCrits+=p.critical; enemyCrits+=e.critical;
        }
        check(playerHits > 79000 && playerHits < 81000 && enemyHits > 79000 && enemyHits < 81000, "shared hit probability differs");
        check(playerCrits > 15000 && playerCrits < 17000 && enemyCrits > 3500 && enemyCrits < 4500, "critical probability differs");
        unsigned hostile=0,total=0;
        for (unsigned seed=1;seed<=1000;++seed) { Explorer e(seed,seed,"ratio",fixtureProfiles(1)); e.enableGameplay(1000); hostile+=static_cast<unsigned>(e.state().gameplay.hostile.size()); total+=2; }
        check(hostile > 1400 && hostile < 1600 && total == 2000, "relationship ratio differs from 3:1");
        // Production fade compositor keeps all pixels opaque and returns to original pixels.
        Surface image(200,200); std::fill(image.pixels.begin(),image.pixels.end(),255);
        auto fade=empty().state().gameplay; fade.life=LifePhase::ReturnHome; fade.phaseStartedMs=1000;
        gameplayOverlay(image,fade,1750); check(image.pixels[0]==0 && image.pixels[3]==255,"fade exposes transparent desktop");
        std::fill(image.pixels.begin(),image.pixels.end(),255); fade.homeScene=true;
        gameplayOverlay(image,fade,2500); check(image.pixels[0]==255 && image.pixels[3]==255,"fade-in did not reveal home");
        pass(12,"every scenario transition restored with matching state/RNG/journals; no duplicate damage meal or return");
        std::cout << "{\"checks\":" << checks << ",\"restores\":" << restores << ",\"relationshipHostile\":" << hostile << ",\"relationshipTotal\":" << total << ",\"playerHits\":" << playerHits << ",\"enemyHits\":" << enemyHits << ",\"playerCriticals\":" << playerCrits << ",\"enemyCriticals\":" << enemyCrits << "}\n";
        if (argc == 4) {
            const auto package = loadExplorationPackage(argv[2]);
            Explorer scenario(900,42,package.assetVersion,package.floors,{0,0,0,0});
            while (scenario.state().floorIndex+1<scenario.state().floors.size() || scenario.state().position!=scenario.state().world.stairs) scenario.advance();
            const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            scenario.enableGameplay(now);
            std::filesystem::create_directories(argv[3]);
            saveSnapshot(std::filesystem::path(argv[3])/"clear.mdes",scenario.state());
            Explorer combatScenario(901,42,package.assetVersion,package.floors,{0,1,0,1});
            combatScenario.enableGameplay(now);
            auto combatState=combatScenario.state();
            const auto actor=std::find_if(combatState.population.entities.begin(),combatState.population.entities.end(),[](const auto& entity) { return entity.kind==EntityKind::Friend; });
            check(actor!=combatState.population.entities.end(),"original combat fixture has no actor");
            const auto reach=bfs(combatState.world,actor->tile);
            for (int p=0;p<kTiles;++p) if (reach.distance[p]==1) { combatState.position=p; combatState.visited[p]=1; break; }
            combatState.gameplay.hostile={actor->instanceId}; combatState.gameplay.deadlineMs=now;
            for (std::uint64_t seed=0;seed<10000;++seed) {
                Rng preview{seed}; const auto first=rollBasicAttack(preview,true), reply=rollBasicAttack(preview,false), second=rollBasicAttack(preview,true);
                if (first.hit && !first.critical && reply.hit && !reply.critical && second.hit) { combatState.rng.state=seed; break; }
            }
            combatScenario=Explorer(combatState); combatScenario.advanceGameplay(now);
            saveSnapshot(std::filesystem::path(argv[3])/"combat.mdes",combatScenario.state());
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
