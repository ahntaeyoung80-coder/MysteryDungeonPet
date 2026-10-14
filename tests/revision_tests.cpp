#include "core/exploration.hpp"
#include "core/gameplay_scene.hpp"
#include "core/package.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
ExplorationState fixture(unsigned floors=2) {
    Explorer initial(900,42,"revision",fixtureProfiles(floors),{0,0,0,0});auto s=initial.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<=7;++y) for(int x=2;x<=7;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=0;}
    for(int y=2;y<=7;++y) for(int x=10;x<=15;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=1;}
    for(int x=7;x<=10;++x) s.world.terrain[4*kWidth+x]=1;
    s.world.centers={4*kWidth+4,4*kWidth+12};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
    s.position=3*kWidth+3;s.visited.fill(0);s.visited[s.position]=1;s.discovered.fill(0);s.knownStairs=-1;
    return s;
}
Explorer restored(const Explorer& e) {return Explorer(decodeSnapshot(encodeSnapshot(e.state()),"revision"));}
int main() {try {
    auto s=fixture();s.discovered[s.world.stairs]=1;s.knownStairs=s.world.stairs;
    s.population.entities={{1,10,EntityKind::Item,3*kWidth+6,false}};
    Explorer items(s);items.advance();check(items.goal()==GoalKind::Item,"known stairs/frontier override item");
    auto pending=items.state();pending.population.entities.push_back({2,11,EntityKind::Item,3*kWidth+5,false});
    items=Explorer(pending);items.advance();check(items.state().targetInstance==1,"new closer item replaced unfinished goal");
    while(items.state().targetInstance==1) items.advance();
    check(items.state().inventory.size()==1,"item goal did not finish exactly once");
    items.advance();check(items.goal()==GoalKind::Item && items.state().targetInstance==2,"next item missing");
    s.population.entities.clear();Explorer frontier(s);frontier.advance();check(frontier.goal()==GoalKind::Frontier,"stairs precede unfinished room");
    s.discovered=s.world.terrain;Explorer stairs(s);stairs.advance();check(stairs.goal()==GoalKind::Stairs,"completed exploration missing stairs goal");
    const auto target=stairs.state().target;auto consistent=restored(stairs);consistent.advance();check(consistent.state().target==target && consistent.goal()==GoalKind::Stairs,"stairs route changed without new goals");
    pending=stairs.state();pending.population.entities={{1,10,EntityKind::Item,3*kWidth+5,false}};pending.discovered[3*kWidth+5]=0;
    auto alreadySeen=pending;alreadySeen.discovered=alreadySeen.world.terrain;
    Explorer newItem(alreadySeen);newItem.advance();check(newItem.goal()==GoalKind::Item,"new item in already discovered room did not re-evaluate stairs");
    Explorer reevaluate(pending);reevaluate.advance();check(reevaluate.goal()==GoalKind::Item,"newly revealed item did not re-evaluate stairs");
    s.population.entities={{1,10,EntityKind::Item,3*kWidth+6,false}};Explorer failed(s);failed.advance();
    failed.setBlocked(3*kWidth+6,true);failed=restored(failed);failed.advance();
    check(failed.state().failedTargets[3*kWidth+6]==3 && failed.goal()!=GoalKind::Item,"repeated inaccessible item reselected");
    for(unsigned i=0;i<5;++i) {failed=restored(failed);failed.advance();check(failed.state().targetFailures==3,"failed item loops on restore");}
    // Save both sides of the black-frame transaction. No movement, combat, RNG or planning during fades.
    s=fixture();s.position=s.world.stairs;s.visited[s.position]=1;s.gameplay.enabled=true;
    Explorer floor(s);floor.advanceGameplay(1000);check(floor.state().gameplay.sceneBeat==SceneBeat::FloorFadeOut,"stairs transition not staged");
    auto before=encodeSnapshot(floor.state());const auto plans=floor.bfsCalls();
    floor.advance();floor.advanceGameplay(1999,true);check(before==encodeSnapshot(floor.state()) && plans==floor.bfsCalls(),"fade-out ticks/replans");
    floor=restored(floor);check(floor.advanceGameplay(2000).event==GameplayEvent::FloorChanged,"black frame did not change floor");
    check(floor.state().floorIndex==1 && floor.state().position==floor.state().world.start && floor.state().target==-1 && floor.state().gameplay.sceneBeat==SceneBeat::FloorFadeIn,"new floor did not reset presentation/goal");
    before=encodeSnapshot(floor.state());floor=restored(floor);floor.advanceGameplay(2999,true);floor.advance();
    check(before==encodeSnapshot(floor.state()),"fade-in repeated transition or ticked");
    check(floor.advanceGameplay(3000).event==GameplayEvent::Explore && floor.state().floorIndex==1 && floor.state().onceKeys.size()==1,"restore duplicated floor journal");
    Surface image(2,2);std::fill(image.pixels.begin(),image.pixels.end(),255);
    GameplayState fading;fading.sceneBeat=SceneBeat::FloorFadeOut;fading.phaseStartedMs=1000;
    gameplayOverlay(image,fading,2000);check(image.pixels[0]==0 && image.pixels[3]==255,"stairs black is transparent");
    std::fill(image.pixels.begin(),image.pixels.end(),255);fading.sceneBeat=SceneBeat::FloorFadeIn;
    gameplayOverlay(image,fading,1000);check(image.pixels[0]==0 && image.pixels[3]==255,"new floor visible before fade-in");
    GameplayState g;check(showFriendlyDisc(g,true,false),"friendly dungeon disc absent");
    check(!showFriendlyDisc(g,true,true) && !showFriendlyDisc(g,false,false),"player/enemy disc leaked");
    for(auto life:{LifePhase::Dialogue,LifePhase::Eat,LifePhase::Sleep,LifePhase::Wake,LifePhase::Ready}) {
        g.life=life;g.homeScene=life!=LifePhase::Dialogue;check(!showFriendlyDisc(g,true,false),"disc leaked into a scene");
    }
    // The fourth schema only extends enums; a real MDES3-shaped idle save remains readable.
    auto old=encodeSnapshot(Explorer(fixture()).state());old[4]=3;uint32_t crc=0xffffffff;
    for(size_t i=0;i<old.size()-4;++i) {crc^=old[i];for(int bit=0;bit<8;++bit) crc=(crc>>1) ^ ((crc&1)?0xedb88320U:0);}
    crc=~crc;
    for(unsigned b=0;b<4;++b) old[old.size()-4+b]=uint8_t(crc>>(8*b));
    check(decodeSnapshot(old,"revision").position==fixture().position,"MDES3 idle compatibility");
    std::cout<<"revision priority/failure/fade/restore/disc tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
