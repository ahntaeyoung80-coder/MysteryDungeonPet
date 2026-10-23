#include "core/exploration.hpp"
#include "core/motion.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pet {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
std::string layoutKey(std::uint64_t runId,const World& world) {
    std::uint64_t hash=14695981039346656037ULL;
    for(auto tile:world.terrain){hash^=tile;hash*=1099511628211ULL;}
    return "floor-layout:"+std::to_string(runId)+":"+std::to_string(hash);
}
constexpr size_t kRecentLayoutLimit=64;
struct LayoutMemory {uint64_t sequence,hash;std::string key;};
std::vector<LayoutMemory> layoutMemory(const GameplayState& gameplay) {
    const std::string prefix="recent-layout:";
    std::vector<LayoutMemory> result;std::set<uint64_t> sequences,hashes;
    auto number=[](const std::string& text) {
        uint64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
        require(!text.empty() && parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size(),
                "invalid recent layout number");return value;
    };
    for(const auto& key:gameplay.onceKeys)if(key.compare(0,prefix.size(),prefix)==0) {
        const auto separator=key.find(':',prefix.size());
        require(separator!=std::string::npos,"invalid recent layout key");
        const auto sequence=number(key.substr(prefix.size(),separator-prefix.size()));
        const auto hash=number(key.substr(separator+1));
        require(sequences.insert(sequence).second && hashes.insert(hash).second,
                "duplicate recent layout entry");
        result.push_back({sequence,hash,key});
        require(result.size()<=kRecentLayoutLimit,"recent layout history exceeds bound");
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
    return result;
}
uint64_t normalizedLayout(const World& world) {
    int minX=kWidth,minY=kHeight,maxX=-1,maxY=-1;
    for(int tile=0;tile<kTiles;++tile)if(world.walkable(tile)) {
        minX=std::min(minX,tile%kWidth);maxX=std::max(maxX,tile%kWidth);
        minY=std::min(minY,tile/kWidth);maxY=std::max(maxY,tile/kWidth);
    }
    require(maxX>=minX && maxY>=minY,"empty layout shape");
    uint64_t hash=14695981039346656037ULL;
    auto add=[&](uint8_t value){hash^=value;hash*=1099511628211ULL;};
    add(uint8_t(maxX-minX+1));add(uint8_t(maxY-minY+1));
    for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x)add(world.terrain[y*kWidth+x]);
    return hash;
}
void rememberLayout(GameplayState& gameplay,uint64_t hash) {
    const auto history=layoutMemory(gameplay);
    if(std::any_of(history.begin(),history.end(),[&](const auto& entry){return entry.hash==hash;}))return;
    require(history.empty() || history.back().sequence!=UINT64_MAX,"recent layout counter exhausted");
    const auto sequence=history.empty()?0:history.back().sequence+1;
    if(history.size()==kRecentLayoutLimit)gameplay.onceKeys.erase(history.front().key);
    gameplay.onceKeys.insert("recent-layout:"+std::to_string(sequence)+":"+std::to_string(hash));
}
int companionSpawn(const World& world,int player,unsigned direction,const Mask& occupied) {
    for(unsigned step=0;step<8;++step) {
        const auto [dx,dy]=kDirections[(direction+4+step)%8];
        const int x=player%kWidth+dx,y=player/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int tile=y*kWidth+x;
        if(tile!=world.stairs && canMove(world,player,tile,occupied)) return tile;
    }
    return -1;
}
std::uint64_t totalWeight(const Weights& weights) {
    std::uint64_t total = 0;
    for (const auto& w : weights) {
        require(w.value <= std::numeric_limits<std::uint64_t>::max() - total,
                "weight sum overflow");
        total += w.value;
    }
    return total;
}
void validateProfile(const FloorProfile& p) {
    require(!p.edition.empty() && p.edition.size() <= 256, "invalid edition");
    require(p.floorItems.size() <= 1024 && p.normalFriends.size() <= 65536 &&
            p.guaranteed.size() <= 65536, "profile count exceeds limit");
    totalWeight(p.normalFriends);
    Weights categories;
    for (const auto& category : p.floorItems) {
        require(category.items.size() <= 65536, "item count exceeds limit");
        require(category.weight == 0 || totalWeight(category.items) > 0,
                "positive category has no conditional items");
        totalWeight(category.items);
        categories.push_back({category.originalId, category.weight});
    }
    totalWeight(categories);
}
void validatePolicy(const SpawnPolicy& p) {
    require(p.itemBudget <= kTiles && p.friendBudget <= kTiles, "spawn budget exceeds grid");
}
}
std::uint64_t Rng::next() {
    auto z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
std::uint64_t Rng::bounded(std::uint64_t bound) {
    require(bound != 0, "zero RNG bound");
    // Rejection removes modulo bias. Spawn *placement* never retries coordinates.
    const auto threshold = (std::uint64_t{0} - bound) % bound;
    for (;;) {
        const auto value = next();
        if (value >= threshold) return value % bound;
    }
}
bool World::walkable(int tile) const {
    return tile >= 0 && tile < kTiles && terrain[static_cast<std::size_t>(tile)] == 1;
}
const std::array<std::pair<int,int>,8> kDirections{{{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}}};
unsigned directionBetween(int from, int to) {
    const int dx=(to%kWidth>from%kWidth)-(to%kWidth<from%kWidth);
    const int dy=(to/kWidth>from/kWidth)-(to/kWidth<from/kWidth);
    for(unsigned i=0;i<8;++i) if(kDirections[i]==std::make_pair(dx,dy)) return i;
    return 0;
}
bool canAttack(const World& w,int from,int to) {
    if(!w.walkable(from)||!w.walkable(to)||from==to) return false;
    const int dx=to%kWidth-from%kWidth,dy=to/kWidth-from/kWidth;
    if(std::abs(dx)>1||std::abs(dy)>1) return false;
    // Binary normal-floor world: impassable terrain in either orthogonal side blocks a diagonal.
    // Actor occupancy on those side tiles is NOT terrain and does not block the corner.
    return !dx || !dy || (w.walkable(from+dx) && w.walkable(from+dy*kWidth));
}
bool canMove(const World& w,int from,int to,const Mask& occupied) {
    return canAttack(w,from,to) && !occupied[to];
}
Search bfs(const World& world, int start, const Mask& blocked) {
    Search result;
    result.distance.fill(-1);
    result.parent.fill(-1);
    if (!world.walkable(start) || blocked[start]) return result;
    std::array<int, kTiles> queue{}, diagonals{};
    int head = 0, tail = 0;
    queue[tail++] = start;
    result.distance[start] = 0;
    while (head != tail) {
        const int p = queue[head++], x = p % kWidth, y = p / kWidth;
        for (const auto& [dx, dy] : kDirections) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= kWidth || ny < 0 || ny >= kHeight) continue;
            const int n = ny * kWidth + nx;
            if (!canMove(world,p,n,blocked)) continue;
            const int distance=result.distance[p]+1;
            const int diagonal=diagonals[p]+(dx!=0 && dy!=0);
            const bool unseen=result.distance[n]<0;
            if(!unseen && (result.distance[n]!=distance ||
                std::make_pair(diagonal,p)>=std::make_pair(diagonals[n],result.parent[n]))) continue;
            // BFS visits every predecessor level before this node is popped.
            // Refine equal-length parents without re-enqueuing or changing
            // terrain/corner/occupancy rules. A fixed parent-tile tie gives the
            // same suffix from a restored position; local turn counts do not.
            result.distance[n]=distance;result.parent[n]=p;
            diagonals[n]=diagonal;
            if(unseen)queue[tail++]=n;
        }
    }
    return result;
}
std::vector<int> Search::pathTo(int goal) const {
    if (goal < 0 || goal >= kTiles || distance[goal] < 0) return {};
    std::vector<int> path;
    for (int p = goal; parent[p] != -1; p = parent[p]) path.push_back(p);
    std::reverse(path.begin(), path.end());
    return path;
}
World generate(Rng& rng, std::uint64_t version) {
    World world;
    world.version = version;
    world.room.fill(-1);
    std::array<std::pair<int, int>, 4> slots{{{1, 1}, {13, 1}, {1, 13}, {13, 13}}};
    for (std::size_t i = slots.size() - 1; i > 0; --i)
        std::swap(slots[i], slots[static_cast<std::size_t>(rng.bounded(i + 1))]);
    const int count = 2 + static_cast<int>(rng.bounded(2));
    for (int i = 0; i < count; ++i) {
        const int w = 3 + static_cast<int>(rng.bounded(4));
        const int h = 3 + static_cast<int>(rng.bounded(4));
        const int x = slots[i].first + static_cast<int>(rng.bounded(11 - w));
        const int y = slots[i].second + static_cast<int>(rng.bounded(11 - h));
        for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) {
            world.terrain[yy * kWidth + xx] = 1;
            world.room[yy * kWidth + xx] = static_cast<std::int8_t>(i);
        }
        world.centers.push_back((y + h / 2) * kWidth + x + w / 2);
    }
    for (int i = 1; i < count; ++i) {
        int x = world.centers[i - 1] % kWidth, y = world.centers[i - 1] / kWidth;
        const int bx = world.centers[i] % kWidth, by = world.centers[i] / kWidth;
        const bool horizontalFirst = rng.bounded(2) != 0;
        for (int axis = 0; axis < 2; ++axis) {
            const bool horizontal = axis == 0 ? horizontalFirst : !horizontalFirst;
            int& coordinate = horizontal ? x : y;
            const int target = horizontal ? bx : by;
            while (coordinate != target) {
                world.terrain[y * kWidth + x] = 1;
                coordinate += target > coordinate ? 1 : -1;
            }
            world.terrain[y * kWidth + x] = 1;
        }
    }
    world.start = world.centers.front();
    const auto search = bfs(world, world.start);
    world.stairs = world.centers[1];
    for (std::size_t i = 2; i < world.centers.size(); ++i)
        if (search.distance[world.centers[i]] > search.distance[world.stairs])
            world.stairs = world.centers[i];
    validateWorld(world);
    return world;
}
void validateWorld(const World& world) {
    require(world.version > 0, "invalid grid version");
    require(world.centers.size() == 2 || world.centers.size() == 3, "invalid room count");
    require(world.walkable(world.start) && world.walkable(world.stairs) &&
            world.start != world.stairs, "invalid start/stairs");
    require(world.start == world.centers.front(), "start is not first room center");
    for (std::size_t i = 0; i < world.centers.size(); ++i)
        require(world.walkable(world.centers[i]) && world.room[world.centers[i]] == static_cast<int>(i),
                "invalid room center");
    require(world.room[world.stairs] > 0, "stairs are in start room");
    const auto search = bfs(world, world.start);
    for (int p = 0; p < kTiles; ++p) {
        require(world.terrain[p] <= 1 && world.room[p] >= -1 &&
                world.room[p] < static_cast<int>(world.centers.size()), "invalid terrain/room");
        require(world.terrain[p] || world.room[p] == -1, "wall has room ID");
        require(!world.terrain[p] || search.distance[p] >= 0, "disconnected floor");
        const int x = p % kWidth, y = p / kWidth;
        require((x != 0 && x != kWidth - 1 && y != 0 && y != kHeight - 1) ||
                !world.terrain[p], "open boundary");
    }
}
bool sample(const Weights& weights, Rng& rng, std::uint32_t& id) {
    const auto total = totalWeight(weights);
    if (total == 0) return false;
    auto draw = rng.bounded(total);
    for (const auto& weight : weights) {
        if (draw < weight.value) { id = weight.originalId; return true; }
        draw -= weight.value;
    }
    throw std::logic_error("invalid weighted sample");
}
Population populate(const World& world, const FloorProfile& profile, Rng& rng,
                    const SpawnPolicy& policy, const Mask& occupied) {
    validateWorld(world);
    validateProfile(profile);
    validatePolicy(policy);
    Population result;
    std::vector<int> candidates;
    const auto search = bfs(world, world.start);
    for (int p = 0; p < kTiles; ++p)
        if (search.distance[p] >= 0 && p != world.start && p != world.stairs && !occupied[p])
            candidates.push_back(p);
    auto omit = [&](EntityKind kind, OmissionReason reason, std::uint32_t count, bool guaranteed) {
        if (count) result.omissions.push_back({kind, reason, count, guaranteed});
    };
    auto place = [&](EntityKind kind, std::uint32_t id) {
        const auto index = static_cast<std::size_t>(rng.bounded(candidates.size()));
        result.entities.push_back({static_cast<std::uint32_t>(result.entities.size() + 1),
                                   id, kind, candidates[index], false});
        candidates[index] = candidates.back();
        candidates.pop_back();
    };
    const auto guaranteedCount = static_cast<std::uint32_t>(profile.guaranteed.size());
    const auto guaranteedBudget = std::min(guaranteedCount, policy.itemBudget);
    const auto guaranteedPlaced = std::min(guaranteedBudget, static_cast<std::uint32_t>(candidates.size()));
    for (std::uint32_t i = 0; i < guaranteedPlaced; ++i)
        place(EntityKind::Item, profile.guaranteed[i]);
    omit(EntityKind::Item, OmissionReason::Budget, guaranteedCount - guaranteedBudget, true);
    omit(EntityKind::Item, OmissionReason::Space, guaranteedBudget - guaranteedPlaced, true);
    auto randomSpawns = [&](EntityKind kind, std::uint32_t request, std::uint32_t budget) {
        const auto allowed = std::min(request, budget);
        const auto attempts = std::min(allowed, static_cast<std::uint32_t>(candidates.size()));
        omit(kind, OmissionReason::Budget, request - allowed, false);
        omit(kind, OmissionReason::Space, allowed - attempts, false);
        Weights categories;
        for (std::size_t i = 0; i < profile.floorItems.size(); ++i)
            categories.push_back({static_cast<std::uint32_t>(i), profile.floorItems[i].weight});
        for (std::uint32_t i = 0; i < attempts; ++i) {
            std::uint32_t id = 0;
            bool available;
            if (kind == EntityKind::Item) {
                std::uint32_t category = 0;
                available = sample(categories, rng, category) &&
                            sample(profile.floorItems[category].items, rng, id);
            } else available = sample(profile.normalFriends, rng, id);
            if (!available) {
                omit(kind, OmissionReason::EmptyDistribution, attempts - i, false);
                break;
            }
            place(kind, id);
        }
    };
    randomSpawns(EntityKind::Item, policy.itemRequest, policy.itemBudget - guaranteedPlaced);
    randomSpawns(EntityKind::Friend, policy.friendRequest, policy.friendBudget);
    return result;
}
EncounterAction encounterPolicy(const Entity&) { return EncounterAction::Greet; }
Explorer::Explorer(std::uint64_t runId, std::uint64_t seed, std::string assetVersion,
                   std::vector<FloorProfile> floors, SpawnPolicy policy,std::vector<uint64_t> recentLayouts,ExpeditionState expedition) {
    require(!floors.empty() && floors.size() <= 128, "invalid floor count");
    validatePolicy(policy);
    for (const auto& p : floors) validateProfile(p);
    state_.runId = runId;
    state_.variety = initialVarietyState(runId,seed,1);
    state_.rng.state = seed;
    state_.assetVersion = std::move(assetVersion);
    state_.floors = std::move(floors);
    state_.expedition=std::move(expedition);
    validateFloorSequence(state_.floors,state_.expedition);
    state_.spawnPolicy = policy;
    require(recentLayouts.size()<=kRecentLayoutLimit,"recent layout input exceeds bound");
    std::set<uint64_t> unique;
    for(size_t i=0;i<recentLayouts.size();++i) {
        require(unique.insert(recentLayouts[i]).second,"duplicate recent layout input");
        state_.gameplay.onceKeys.insert("recent-layout:"+std::to_string(i)+":"+std::to_string(recentLayouts[i]));
    }
    enterFloor();
    validateState(state_);
}
void Explorer::setNextRunPackage(std::string assetVersion,std::vector<FloorProfile> floors,ExpeditionState expedition) {
    require(!assetVersion.empty() && assetVersion.size()<=256,"invalid next-run asset version");
    validateFloorSequence(floors,expedition);
    for(const auto& floor:floors)validateProfile(floor);
    nextRunAssetVersion_=std::move(assetVersion);nextRunFloors_=std::move(floors);nextRunExpedition_=std::move(expedition);
}
bool discardStaleEntityGoal(ExplorationState& s,bool movingFriend) {
    if(s.goalKind!=GoalKind::Friend && s.goalKind!=GoalKind::Item)return false;
    // Check only the disposable entity route. World, journals and combat still
    // undergo the full validator, including when importing an older save.
    if(s.target<0 || s.target>=kTiles)return false;
    const bool valid=std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e) {
        return e.instanceId==s.targetInstance && e.visible && !e.done &&
            e.kind==(s.goalKind==GoalKind::Item?EntityKind::Item:EntityKind::Friend) &&
            (e.kind==EntityKind::Item?e.tile==s.target:canAttack(s.world,s.target,e.tile));
    });
    if(valid)return false;
    if(movingFriend && s.goalKind==GoalKind::Friend)for(const auto& e:s.population.entities)
        if(e.instanceId==s.targetInstance && e.kind==EntityKind::Friend && e.visible && !e.done &&
           std::find(s.gameplay.hostile.begin(),s.gameplay.hostile.end(),e.instanceId)==s.gameplay.hostile.end()) {
            const auto reach=bfs(s.world,s.position,s.blocked);int next=-1;
            for(int tile=0;tile<kTiles;++tile)if(!s.blocked[tile] && canAttack(s.world,tile,e.tile) && reach.distance[tile]>=0 &&
               (next<0 || reach.distance[tile]<reach.distance[next]))next=tile;
            if(next>=0){s.target=next;return false;}
        }
    s.target=-1;s.goalKind=GoalKind::None;s.targetInstance=0;return true;
}
Explorer::Explorer(ExplorationState state) : state_(std::move(state)) {
    validateState(state_);
    if (state_.target != -1 && (!state_.gameplay.enabled || state_.gameplay.life == LifePhase::Explore)) {
        auto occupied=playerOccupancy();if(state_.goalKind!=GoalKind::Stairs) occupied[state_.world.stairs]=1;
        path_ = bfs(state_.world, state_.position, occupied).pathTo(state_.target);
        ++bfsCalls_;
        cachedWorld_ = state_.world.version;
        cachedOccupancy_ = state_.occupancyVersion;
    }
}
void Explorer::invalidate(bool clearTarget,bool movingFriend) {
    path_.clear(); pathIndex_=0; cachedWorld_=cachedOccupancy_=UINT64_MAX;
    if(clearTarget) {state_.target=-1;state_.goalKind=GoalKind::None;state_.targetInstance=0;}
    else discardStaleEntityGoal(state_,movingFriend);
}
std::string Explorer::key(const std::string& action, std::uint32_t instance) const {
    return action + ":" + std::to_string(state_.runId) + ":" +
           std::to_string(state_.floorIndex) + ":" + std::to_string(instance);
}
std::vector<uint64_t> Explorer::recentLayouts() const {
    std::vector<uint64_t> result;
    for(const auto& entry:layoutMemory(state_.gameplay))result.push_back(entry.hash);
    // Older snapshots do not contain this history. Their current world is
    // still available and must not be repeated by the very next run.
    if(!state_.world.centers.empty()) {
        const auto current=normalizedLayout(state_.world);
        if(std::find(result.begin(),result.end(),current)==result.end()) {
            if(result.size()==kRecentLayoutLimit)result.erase(result.begin());
            result.push_back(current);
        }
    }
    return result;
}
void Explorer::enterFloor() {
    const int previousFeet=state_.world.centers.empty()?-1:state_.position;
    const int previousCompanion=state_.gameplay.companion.enabled?state_.gameplay.companion.tile:-1;
    auto& layouts=state_.gameplay.onceKeys;
    if(previousFeet>=0) {
        layouts.insert(layoutKey(state_.runId,state_.world));
        rememberLayout(state_.gameplay,normalizedLayout(state_.world));
    }
    const auto recent=layoutMemory(state_.gameplay);
    bool accepted=false;
    if(state_.expedition.enabled()) {
        const auto& f=state_.expedition.floors.at(state_.floorIndex);
        if(!f.fixedTiles.empty()) {
            World w;w.room.fill(-1);w.version=static_cast<uint64_t>(state_.floorIndex)+1;
            for(auto t:f.fixedTiles)w.terrain[t]=1;
            w.start=f.fixedStart;w.stairs=f.fixedExit;w.centers={w.start,w.stairs};
            const auto reach=bfs(w,w.start);const int split=std::max(1,reach.distance[w.stairs]/2);
            for(auto t:f.fixedTiles)w.room[t]=reach.distance[t]<split?0:1;
            w.room[w.start]=0;w.room[w.stairs]=1;validateWorld(w);
            state_.mapSeed=state_.rng.state;state_.world=std::move(w);accepted=true;
        }
    }
    for(unsigned attempt=0;attempt<128 && !accepted;++attempt) {
        const auto mapSeed=state_.rng.state;
        auto world=generate(state_.rng,static_cast<std::uint64_t>(state_.floorIndex)+1);
        const auto key=layoutKey(state_.runId,world);
        const auto shape=normalizedLayout(world);
        if(world.stairs==previousFeet || world.stairs==previousCompanion || layouts.count(key) ||
           std::any_of(recent.begin(),recent.end(),[&](const auto& entry){return entry.hash==shape;})) continue;
        state_.mapSeed=mapSeed;state_.world=std::move(world);layouts.insert(key);rememberLayout(state_.gameplay,shape);accepted=true;break;
    }
    require(accepted,"no unique floor layout within generation budget");
    Mask entryExclusion{};
    if(previousFeet>=0) entryExclusion[previousFeet]=1;
    if(previousCompanion>=0) entryExclusion[previousCompanion]=1;
    if(state_.gameplay.companion.enabled) {
        auto& partner=state_.gameplay.companion;
        partner.tile=companionSpawn(state_.world,state_.world.start,state_.gameplay.playerDirection,entryExclusion);
        require(partner.tile>=0,"no companion floor entry");
        entryExclusion[partner.tile]=1;partner.movement={};partner.walkStartedMs=0;partner.hidden=false;
        partner.direction=directionBetween(partner.tile,state_.world.start);partner.actionRound=state_.gameplay.actionRound;
    }
    auto profile=state_.floors[state_.floorIndex];
    // Product supply policy, separate from the unmodified original distributions.
    // Respect explicit no-item fixtures and preserve the original fixed rooms.
    if(state_.expedition.enabled() && !state_.expedition.floors[state_.floorIndex].fixedFloorId &&
       state_.spawnPolicy.itemRequest && state_.spawnPolicy.itemBudget &&
       std::find(profile.guaranteed.begin(),profile.guaranteed.end(),ITEM_ORAN_BERRY)==profile.guaranteed.end())
        profile.guaranteed.insert(profile.guaranteed.begin(),ITEM_ORAN_BERRY);
    state_.population = populate(state_.world, profile,
                                 state_.rng, state_.spawnPolicy,entryExclusion);
    if(state_.expedition.enabled() && state_.expedition.floors.at(state_.floorIndex).fixedFloorId)populateFixedFloor();
    state_.position = state_.world.start;
    state_.visited.fill(0);state_.screenSeen.fill(0);
    state_.visited[state_.position] = 1;
    state_.blocked.fill(0);
    state_.discovered.fill(0); state_.failedTargets.fill(0);
    state_.visitedRooms=state_.stalledActions=state_.targetFailures=0;state_.knownStairs=-1;state_.survivalFirst=SurvivalFirst::None;
    discover();
    state_.occupancyVersion = 0;
    state_.floorSteps = 0;
    invalidate();
    state_.gameplay.movement={};state_.gameplay.walkStartedMs=0;state_.gameplay.navigationHistory.clear();
    state_.gameplay.partyActions.clear();state_.gameplay.partyPlaybacks.clear();state_.gameplay.partyPhase=PartyPhase::Idle;
    state_.gameplay.companionIntent=CompanionIntent::Follow;state_.gameplay.companionTarget=0;
    if (state_.gameplay.enabled) {if(state_.gameplay.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");++state_.gameplay.actionRound;assignRelationships();}
}
void Explorer::populateFixedFloor() {
    const auto& floor=state_.expedition.floors.at(state_.floorIndex);
    const auto& source=state_.expedition.revisit && floor.revisitFixedFloorId?floor.revisitSpawns:floor.fixedSpawns;
    state_.population={};
    Mask used{};used[state_.world.start]=used[state_.world.stairs]=1;
    if(state_.gameplay.companion.enabled)used[state_.gameplay.companion.tile]=1;
    // Keep source identity/order in the manifest. The pet retains its small generated
    // rooms; map original fixed-room coordinates to the nearest distinct free tile.
    for(const auto& spawn:source) {
        int best=-1,score=INT32_MAX;
        const int x=std::clamp(int(spawn.x)+(floor.fixedTiles.empty()?0:1),1,kWidth-2),y=std::clamp(int(spawn.y)+(floor.fixedTiles.empty()?0:1),1,kHeight-2);
        for(int tile=0;tile<kTiles;++tile)if(state_.world.walkable(tile) && !used[tile]) {
            const int candidate=std::abs(tile%kWidth-x)+std::abs(tile/kWidth-y);
            if(candidate<score){best=tile;score=candidate;}
        }
        require(best>=0,"fixed spawn exceeds available floor space");used[best]=1;
        Entity e;e.instanceId=static_cast<unsigned>(state_.population.entities.size()+1);
        e.originalId=spawn.md?spawn.md:spawn.item;e.kind=spawn.md?EntityKind::Friend:EntityKind::Item;e.tile=best;
        state_.population.entities.push_back(e);
    }
}
void Explorer::setBlocked(int tile, bool value) {
    require(state_.world.walkable(tile) && tile != state_.position, "invalid blocker tile");
    if (state_.blocked[tile] != static_cast<std::uint8_t>(value)) {
        require(state_.occupancyVersion != std::numeric_limits<std::uint64_t>::max(),
                "occupancy version exhausted");
        state_.blocked[tile] = value;
        ++state_.occupancyVersion;
        invalidate(false);
    }
}
bool Explorer::friendly(const Entity& e) const {
    return state_.gameplay.enabled && e.kind==EntityKind::Friend && e.visible &&
        std::find(state_.gameplay.hostile.begin(),state_.gameplay.hostile.end(),e.instanceId)==state_.gameplay.hostile.end();
}
std::uint32_t Explorer::companionOpponent() const {
    const auto& g=state_.gameplay;
    if(!g.independentParty || !g.companion.enabled || !g.companion.hearts)return 0;
    const auto reach=bfs(state_.world,state_.position);
    if(reach.distance[g.companion.tile]<0 || reach.distance[g.companion.tile]>3)return 0;
    for(const auto& e:state_.population.entities)
        if(e.kind==EntityKind::Friend && e.visible && !e.done && !friendly(e) &&
           canAttack(state_.world,g.companion.tile,e.tile))return e.instanceId;
    return 0;
}
bool Explorer::lowHealth() const {const auto& g=state_.gameplay;return g.enabled && !companionOpponent() && (g.hearts<=kLowHp || (!g.independentParty && g.companion.enabled && g.companion.hearts<=kLowHp));}
bool Explorer::mustRetreat() const {
    const auto& g=state_.gameplay;
    // A targetless cornered encounter commits to this existing enemy. Persist
    // that decision so restoring PlayerTurn cannot restart the retreat loop.
    if(g.life==LifePhase::Combat &&
       g.onceKeys.count("cornered:"+key("combat",g.enemyInstance)))return false;
    return (g.hearts<=kLowHp && !g.oran) ||
        (!g.independentParty && g.companion.enabled && g.companion.hearts<=kLowHp && !g.companion.oran);
}
Mask Explorer::occupancy(bool playerPath) const {
    auto mask=state_.blocked;
    for(const auto& e:state_.population.entities) if(e.kind==EntityKind::Friend && e.visible && (!e.done || (state_.gameplay.enabled && std::find(state_.gameplay.hostile.begin(),state_.gameplay.hostile.end(),e.instanceId)==state_.gameplay.hostile.end()))) if(!playerPath || !friendly(e)) mask[e.tile]=1;
    const auto& g=state_.gameplay;
    if(g.companion.enabled) {
        const bool swap=g.independentParty && playerPath && g.companionIntent==CompanionIntent::Follow &&
            g.companion.actionRound<g.actionRound && !partyPlayback(g,kCompanionActorId);
        if(!playerPath || (g.independentParty && !swap))mask[g.companion.tile]=1;
    }
    mask[state_.position]=0; return mask;
}
bool Explorer::enableCompanion() {
    auto& g=state_.gameplay;
    if(g.companion.enabled) {
        if(!g.independentParty && g.life==LifePhase::Explore) {
            g.independentParty=true;g.partyPhase=PartyPhase::Idle;invalidate(false);++state_.occupancyVersion;
        }
        return true;
    }
    if(!g.enabled) return false;
    auto occupied=occupancy();
    for(const auto& e:state_.population.entities) if(e.kind==EntityKind::Item && e.visible && !e.done) occupied[e.tile]=1;
    const int tile=companionSpawn(state_.world,state_.position,g.playerDirection,occupied);
    if(tile<0) return false;
    g.companion.enabled=true;g.companion.tile=tile;
    if(g.life==LifePhase::Explore)g.independentParty=true;g.companion.direction=directionBetween(tile,state_.position);g.navigationHistory.clear();
    invalidate(false);++state_.occupancyVersion;return true;
}
int Explorer::friendlyVacateTile(const Entity& e,int next) const {
    if(state_.gameplay.independentParty && canMove(state_.world,e.tile,state_.position,state_.blocked))return state_.position;
    if(!state_.gameplay.companion.enabled) return state_.position;
    auto occupied=occupancy();occupied[state_.position]=1;occupied[next]=1;
    const auto forward=directionBetween(state_.position,e.tile);
    for(unsigned step=0;step<8;++step) {
        const auto [dx,dy]=kDirections[(forward+step)%8];const int x=e.tile%kWidth+dx,y=e.tile/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int tile=y*kWidth+x;
        if(tile!=state_.world.stairs && canMove(state_.world,e.tile,tile,occupied)) return tile;
    }
    return -1;
}
int Explorer::companionFollowTile(int next) const {
    const auto& g=state_.gameplay;const auto& partner=g.companion;
    if(!partner.enabled) return -1;
    auto occupied=state_.blocked;
    for(const auto& e:state_.population.entities) if(e.kind==EntityKind::Friend && e.visible && (!e.done || friendly(e))) {
        const int tile=friendly(e)&&e.tile==next?friendlyVacateTile(e,next):e.tile;
        if(tile<0) return -1;
        occupied[tile]=1;
    }
    occupied[next]=1;
    int best=-1,bestScore=100;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        const int x=partner.tile%kWidth+dx,y=partner.tile/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int tile=y*kWidth+x;
        if(occupied[tile] || !canAttack(state_.world,tile,next)) continue;
        if(tile!=partner.tile && (partner.actionRound==g.actionRound || !canMove(state_.world,partner.tile,tile,occupied))) continue;
        const int score=tile==state_.position?0:tile==partner.tile?1:2;
        if(score<bestScore){best=tile;bestScore=score;}
    }
    return best;
}
void Explorer::recordCompanionMove(int from,std::uint64_t now) {
    auto& g=state_.gameplay;auto& partner=g.companion;if(!partner.enabled) return;
    if(from!=partner.tile) {
        partner.direction=directionBetween(from,partner.tile);partner.actionRound=g.actionRound;
        partner.movement={g.actionRound,now,400,partner.direction,from,partner.tile};
        if(!partner.walkStartedMs) partner.walkStartedMs=now;
    } else partner.walkStartedMs=0;
}
Mask Explorer::playerOccupancy() const {
    auto occupied=occupancy(true);
    if(state_.gameplay.companion.enabled && !state_.gameplay.independentParty) for(auto [dx,dy]:kDirections) {
        const int x=state_.position%kWidth+dx,y=state_.position/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int tile=y*kWidth+x;
        if(canMove(state_.world,state_.position,tile,occupied) && companionFollowTile(tile)<0) occupied[tile]=1;
    }
    if(!lowHealth() || retreatProgress_) return occupied;
    auto safe=occupied;
    for(const auto& e:state_.population.entities) if(e.kind==EntityKind::Friend && e.visible && !e.done && !friendly(e))
        for(int tile=0;tile<kTiles;++tile) if(tile!=state_.position && canAttack(state_.world,e.tile,tile)) safe[tile]=1;
    const auto reach=bfs(state_.world,state_.position,safe);
    const bool escape=std::any_of(reach.distance.begin(),reach.distance.end(),[](int d){return d>0;});
    return escape?safe:occupied;
}
void Explorer::enableScreenExploration(std::uint32_t size) {
    require(size>=24 && size<=200,"invalid exploration viewport");
    if(state_.screenExploration && state_.viewportSize==size)return;
    state_.screenExploration=true;state_.viewportSize=size;invalidate();
}
bool Explorer::roomFullyShown(int room) const {
    if(!state_.screenExploration || room<0)return true;
    for(int p=0;p<kTiles;++p)if(state_.world.walkable(p) && state_.world.room[p]==room && !state_.screenSeen[p])return false;
    return true;
}
void Explorer::observeViewport(int x,int y) {
    if(!state_.screenExploration || !dungeonScene(state_.gameplay) || state_.gameplay.sceneBeat!=SceneBeat::None)return;
    bool changed=false;const int size=static_cast<int>(state_.viewportSize);
    for(int p=0;p<kTiles;++p)if(state_.world.walkable(p) && !state_.screenSeen[p] &&
        p%kWidth*24>=x && p/kWidth*24>=y && p%kWidth*24+24<=x+size && p/kWidth*24+24<=y+size) {
        state_.screenSeen[p]=1;changed=true;
    }
    if(changed){state_.stalledActions=0;invalidate();}
}
bool Explorer::selectRoomView(const Search& search) {
    const int room=state_.world.room[state_.position];
    if(roomFullyShown(room))return false;
    int best=-1,bestCount=0,bestDistance=kTiles+1;
    for(int p=0;p<kTiles;++p)if(search.distance[p]>0 && state_.world.room[p]==room) {
        const auto camera=cameraFor(tilePoint(p),kWidth*24,kHeight*24,state_.viewportSize);
        int count=0;
        for(int q=0;q<kTiles;++q)if(state_.world.room[q]==room && state_.world.walkable(q) && !state_.screenSeen[q] &&
            q%kWidth*24>=camera.x && q/kWidth*24>=camera.y && q%kWidth*24+24<=camera.x+int(state_.viewportSize) && q/kWidth*24+24<=camera.y+int(state_.viewportSize))++count;
        if(count>bestCount || (count==bestCount && count && search.distance[p]<bestDistance)) {
            best=p;bestCount=count;bestDistance=search.distance[p];
        }
    }
    if(best<0)return false;
    state_.target=best;state_.goalKind=GoalKind::Auxiliary;return true;
}
void Explorer::discover() {
    const auto before=std::count(state_.discovered.begin(),state_.discovered.end(),1);
    const int room=state_.world.room[state_.position];
    if(room>=0) state_.visitedRooms|=1U<<room;
    // Original range model: a room is visible as a unit; corridor visibility is local.
    // Generated room IDs replace the original engine's room rectangle table.
    const auto nearby=bfs(state_.world,state_.position);
    for(int p=0;p<kTiles;++p) if(state_.world.walkable(p) &&
        ((room>=0 && state_.world.room[p]==room) || (nearby.distance[p]>=0 && nearby.distance[p]<=2))) state_.discovered[p]=1;
    if(state_.discovered[state_.world.stairs]) state_.knownStairs=state_.world.stairs;
    if(state_.survivalFirst==SurvivalFirst::None) {
        const bool oran=std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e) {
            return e.kind==EntityKind::Item && e.originalId==ITEM_ORAN_BERRY && e.visible && !e.done && state_.discovered[e.tile];
        });
        if(oran) state_.survivalFirst=SurvivalFirst::Oran;
        else if(state_.knownStairs>=0) state_.survivalFirst=SurvivalFirst::Stairs;
    }
    if(std::count(state_.discovered.begin(),state_.discovered.end(),1)>before) state_.stalledActions=0;
}
bool Explorer::frontier(int tile) const {
    if(!state_.discovered[tile]) return false;
    for(auto [dx,dy]:kDirections) {
        const int x=tile%kWidth+dx,y=tile/kWidth+dy;
        if(x>=0&&x<kWidth&&y>=0&&y<kHeight) {
            const int next=y*kWidth+x;
            if(!state_.discovered[next] && canMove(state_.world,tile,next)) return true;
        }
    }
    return false;
}
bool Explorer::selectPriority(const Search& search,bool items,bool frontiers) {
    if(!state_.gameplay.enabled && (state_.floorSteps>=kFloorStepLimit || state_.stalledActions>=kTiles*2)) return false;
    int best=3*(kTiles+1);
    if(items) for(const auto& e:state_.population.entities) {
        if(e.kind!=EntityKind::Item || e.done || !e.visible || !state_.discovered[e.tile] || state_.failedTargets[e.tile]>=3) continue;
        const int distance=search.distance[e.tile];
        const int priority=state_.gameplay.enabled && state_.gameplay.hearts<kMaxHp && !state_.gameplay.oran && e.originalId==ITEM_ORAN_BERRY?0:1;
        const int score=priority*(kTiles+1)+distance;
        if(distance>=0 && score<best) {best=score;state_.target=e.tile;state_.goalKind=GoalKind::Item;state_.targetInstance=e.instanceId;}
    }
    if(state_.target>=0) return true;
    if(!lowHealth() && selectFriendly(search)) return true;
    if(frontiers) for(int tile=0;tile<kTiles;++tile) {
        if(search.distance[tile]>0 && state_.failedTargets[tile]<3 && frontier(tile) && search.distance[tile]<best) {
            best=search.distance[tile];state_.target=tile;state_.goalKind=GoalKind::Frontier;
        }
    }
    return state_.target>=0;
}
bool Explorer::selectFriendly(const Search& search) {
    int best=kTiles+1;
    for(const auto& e:state_.population.entities) {
        if(e.kind!=EntityKind::Friend || e.done || !e.visible || state_.blocked[e.tile] || !state_.discovered[e.tile] ||
           (state_.gameplay.enabled && !friendly(e))) continue;
        for(int tile=0;tile<kTiles;++tile) if(state_.failedTargets[tile]<3 && search.distance[tile]>=0 &&
            search.distance[tile]<best && canAttack(state_.world,tile,e.tile)) {
            best=search.distance[tile];state_.target=tile;state_.targetInstance=e.instanceId;state_.goalKind=GoalKind::Friend;
        }
    }
    return state_.target>=0;
}
bool Explorer::selectSurvival(const Search& search) {
    if(!lowHealth()) return false;
    const auto stairs=[&]() {
        if(state_.knownStairs<0 || state_.failedTargets[state_.knownStairs]>=3) return false;
        auto reach=bfs(state_.world,state_.position,playerOccupancy());++bfsCalls_;
        if(reach.distance[state_.knownStairs]<0) {reach=bfs(state_.world,state_.position,occupancy(true));++bfsCalls_;}
        if(reach.distance[state_.knownStairs]<0) return false;
        state_.target=state_.knownStairs;state_.goalKind=GoalKind::Stairs;path_=reach.pathTo(state_.target);return true;
    };
    if(state_.survivalFirst==SurvivalFirst::Stairs && stairs()) return true;
    int best=kTiles+1;
    for(const auto& e:state_.population.entities) if(e.kind==EntityKind::Item && e.originalId==ITEM_ORAN_BERRY &&
        !e.done && e.visible && state_.discovered[e.tile] && state_.failedTargets[e.tile]<3 && search.distance[e.tile]>=0 && search.distance[e.tile]<best) {
        best=search.distance[e.tile];state_.target=e.tile;state_.goalKind=GoalKind::Item;state_.targetInstance=e.instanceId;
    }
    if(state_.target>=0) {path_=search.pathTo(state_.target);return true;}
    return stairs();
}
void Explorer::reevaluateStairs() {
    if(lowHealth()) return;
    if(state_.goalKind!=GoalKind::Stairs || state_.position==state_.world.stairs) return;
    const bool items=std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e) {
        return e.kind==EntityKind::Item && !e.done && e.visible && state_.discovered[e.tile] && state_.failedTargets[e.tile]<3;
    });
    bool frontiers=false;
    for(int tile=0;!frontiers && tile<kTiles;++tile) frontiers=state_.failedTargets[tile]<3 && frontier(tile);
    const bool friends=std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){
        return e.kind==EntityKind::Friend && !e.done && e.visible && state_.discovered[e.tile] &&
            (!state_.gameplay.enabled || friendly(e));
    });
    if(!items && !frontiers && !friends) return;
    auto occupied=playerOccupancy();occupied[state_.world.stairs]=1;
    const auto reach=bfs(state_.world,state_.position,occupied);++bfsCalls_;
    const int previousTarget=state_.target;
    state_.target=-1;state_.goalKind=GoalKind::None;state_.targetInstance=0;
    if(selectPriority(reach,items,frontiers)) {
        invalidate(false);path_=reach.pathTo(state_.target);
        cachedWorld_=state_.world.version;cachedOccupancy_=state_.occupancyVersion;
    } else {state_.target=previousTarget;state_.goalKind=GoalKind::Stairs;} // Keep the existing stairs route/cache.
}
void Explorer::plan() {
    if(fixedBattlePending(state_)) {
        invalidate();const auto search=bfs(state_.world,state_.position,playerOccupancy());int best=kTiles+1;
        for(const auto& e:state_.population.entities)if(!e.done && e.visible &&
            std::find(state_.gameplay.hostile.begin(),state_.gameplay.hostile.end(),e.instanceId)!=state_.gameplay.hostile.end())
            for(int tile=0;tile<kTiles;++tile)if(canAttack(state_.world,tile,e.tile) && search.distance[tile]>=0 && search.distance[tile]<best) {
                best=search.distance[tile];state_.target=tile;state_.goalKind=GoalKind::Auxiliary;
            }
        return;
    }

    auto occupied=playerOccupancy();
    // A discovered staircase is not a shortcut through unfinished exploration.
    if(state_.position!=state_.world.stairs) occupied[state_.world.stairs]=1;
    const auto explore=bfs(state_.world,state_.position,occupied);++bfsCalls_;invalidate();
    const bool survival=selectSurvival(explore);
    if(!survival && !selectRoomView(explore)) selectPriority(explore,!lowHealth(),true);
    if(state_.target<0) {
        const auto reach=bfs(state_.world,state_.position,playerOccupancy());++bfsCalls_;
        if(state_.knownStairs>=0 && state_.failedTargets[state_.knownStairs]<3 && reach.distance[state_.knownStairs]>=0) {
            state_.target=state_.knownStairs;state_.goalKind=GoalKind::Stairs;path_=reach.pathTo(state_.target);
        }
    } else if(!survival) path_=explore.pathTo(state_.target);
    if(state_.target<0 && state_.stalledActions<kTiles*2 && state_.floorSteps<kFloorStepLimit) {
        // A bounded, deterministic probe only when every regular goal is unavailable.
        if(state_.target<0 && state_.stalledActions<8) for(int tile=0;tile<kTiles;++tile) if(explore.distance[tile]>0 && explore.distance[tile]<=2 && !state_.visited[tile] && state_.failedTargets[tile]<3) {
            state_.target=tile;state_.goalKind=GoalKind::Auxiliary;path_=explore.pathTo(tile);break;
        }
    }
    cachedWorld_=state_.world.version;cachedOccupancy_=state_.occupancyVersion;
}
Event Explorer::moveTo(int next) {
    if(!canMove(state_.world,state_.position,next,occupancy(true))) return {};
    auto& g=state_.gameplay;
    int partner=-1;
    if(g.companion.enabled && !g.independentParty) {
        partner=companionFollowTile(next);if(partner<0)return {};
    } else if(g.companion.enabled && next==g.companion.tile) {
        if(g.companionIntent!=CompanionIntent::Follow || g.companion.actionRound==g.actionRound ||
           partyPlayback(g,kCompanionActorId))return {};
        partner=state_.position;
    }
    for(auto& e:state_.population.entities) if(e.tile==next && friendly(e)) {
        const int vacant=friendlyVacateTile(e,next);if(vacant<0) return {};
        e.tile=vacant;e.direction=directionBetween(next,vacant);e.movement={};
        e.actionRound=state_.gameplay.actionRound;
        ++state_.occupancyVersion;
    }
    state_.position=next;
    if(state_.gameplay.enabled) {
        state_.floorSteps=std::min(kFloorStepLimit+kTiles,state_.floorSteps+1);
        state_.stalledActions=std::min(kFloorStepLimit+kTiles,state_.stalledActions+1);
    } else {++state_.floorSteps;++state_.stalledActions;}
    if(state_.gameplay.companion.enabled && partner>=0 && state_.gameplay.companion.tile!=partner) {state_.gameplay.companion.tile=partner;++state_.occupancyVersion;}
    if(state_.goalKind==GoalKind::Friend && std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){return e.instanceId==state_.targetInstance && !canAttack(state_.world,state_.target,e.tile);})) invalidate();
    state_.visited[state_.position]=1;
    return {EventKind::Move,{},0};
}
Event Explorer::advance(bool poseFinished) {
    if(varietyBusy(state_.variety))return {};
    if (state_.gameplay.enabled && (state_.gameplay.life!=LifePhase::Explore || state_.gameplay.sceneBeat!=SceneBeat::None) &&
        !(state_.gameplay.life == LifePhase::Pose && poseFinished) &&
        !(state_.gameplay.independentParty && state_.gameplay.sceneBeat==SceneBeat::None &&
          (state_.gameplay.life==LifePhase::Explore || state_.gameplay.life==LifePhase::Combat))) return {};
    if (state_.phase == Phase::Home) return {};
    if (state_.phase == Phase::Pose) {
        if (poseFinished || ++state_.poseSteps >= kPoseTimeout) {
            state_.phase = Phase::Home;
            return {EventKind::ReturnedHome, {}, 0};
        }
        return {};
    }
    discover();
    if(lowHealth()) plan();
    reevaluateStairs();
    // Runtime paths are transient and are rebuilt by snapshot construction.
    // Resolve the next step from the same current decision in continuous play.
    // The standalone explorer retains its across-step path cache.
    if(state_.gameplay.enabled) invalidate(false);
    if(!fixedBattlePending(state_) && state_.position==state_.world.stairs && (lowHealth() || roomFullyShown(state_.world.room[state_.position]))) {invalidate();state_.target=state_.world.stairs;state_.goalKind=GoalKind::Stairs;}
    else for(unsigned attempt=0;attempt<4;++attempt) {
        if(state_.target>=0 && state_.position==state_.target) {
            if(state_.goalKind==GoalKind::Item || state_.goalKind==GoalKind::Friend) break;
            invalidate();
        }
        if(state_.target<0) plan();
        if(state_.target<0 || (!state_.gameplay.enabled && state_.floorSteps>kFloorStepLimit+kTiles)) {
            state_.phase=Phase::Home;invalidate();return {EventKind::ReturnedHome,{},0};
        }
        if(cachedWorld_!=state_.world.version || cachedOccupancy_!=state_.occupancyVersion || pathIndex_>=path_.size()) {
            auto occupied=playerOccupancy();if(state_.goalKind!=GoalKind::Stairs) occupied[state_.world.stairs]=1;
            path_=bfs(state_.world,state_.position,occupied).pathTo(state_.target);++bfsCalls_;pathIndex_=0;
            cachedWorld_=state_.world.version;cachedOccupancy_=state_.occupancyVersion;
        }
        if(pathIndex_<path_.size() && canMove(state_.world,state_.position,path_[pathIndex_],occupancy(true))) {
            // Discover on the next tick so a stairs route can notice newly revealed goals.
            const auto event=moveTo(path_[pathIndex_++]);
            if(event.kind!=EventKind::Move) invalidate(false);
            return event;
        }
        ++state_.targetFailures;
        auto& failures=state_.failedTargets[state_.target];failures=std::min(3,int(failures)+1);
        if(failures>=3) invalidate();
        else invalidate(false);
    }
    if(state_.target>=0 && state_.position!=state_.target) return {};
    Event event;
    if (state_.goalKind == GoalKind::Item || state_.goalKind == GoalKind::Friend) {
        for (auto& entity : state_.population.entities) {
            if (entity.instanceId != state_.targetInstance || entity.done) continue;
            const bool item = entity.kind == EntityKind::Item;
            if (!item) (void)encounterPolicy(entity);
            const auto onceKey = key(item ? "pickup" : "greet", entity.instanceId);
            if (state_.onceKeys.insert(onceKey).second) {
                if (item) {state_.inventory.push_back({onceKey, entity.originalId});
                    if(entity.originalId!=ITEM_ORAN_BERRY)(void)changeStock(state_,0,entity.originalId,1);}
                event = {item ? EventKind::Pickup : EventKind::Greeting, onceKey, entity.originalId};
            }
            entity.done = true;
            break;
        }
    } else if (state_.goalKind == GoalKind::Stairs) {
        if(fixedBattlePending(state_)){invalidate();return {};}
        const auto transitionKey = key("stairs");
        state_.onceKeys.insert(transitionKey);
        if (state_.floorIndex + 1 < state_.floors.size()) {
            ++state_.floorIndex;
            enterFloor();
            return {EventKind::FloorChanged, transitionKey, 0};
        }
        const auto clearKey = key("clear");
        state_.completed = true;
        if(state_.expedition.enabled())state_.expedition.outcome=ExpeditionOutcome::Success;
        state_.phase = Phase::Pose;
        state_.poseSteps = 0;
        if (state_.onceKeys.insert(clearKey).second) event = {EventKind::Pose, clearKey, 0};
    }
    invalidate();
    return event;
}
void validateState(const ExplorationState& s) {
    validateVarietyState(s);
    require(s.viewportSize>=24 && s.viewportSize<=200,"invalid exploration viewport");
    for(int p=0;p<kTiles;++p)require(s.screenSeen[p]<=1 && (!s.screenSeen[p] || s.world.walkable(p)),"invalid screen coverage");
    const auto& g = s.gameplay;
    validatePartyState(s);
    (void)layoutMemory(g);
    require(g.navigationHistory.size()<=32,"navigation history exceeds bound");
    require(static_cast<unsigned>(g.deathStage)<=2 && static_cast<unsigned>(g.sceneBeat)<=27 && static_cast<unsigned>(g.dialogueStage)<=1,"invalid death stage");
    if(g.deathStage==DeathStage::PartnerTalk)require(g.life==LifePhase::Death,"death dialogue outside defeat");
    if(g.sceneBeat>=SceneBeat::CompletionFadeIn && g.sceneBeat<=SceneBeat::CompletionFadeOut)
        require(completionScene(g) && s.completed && !g.homeScene,"invalid completion scene");
    if(g.sceneBeat==SceneBeat::FloorFadeOut || g.sceneBeat==SceneBeat::FloorFadeIn) {
        require(g.enabled && g.life==LifePhase::Explore && !g.homeScene && s.target==-1 && !g.movement.actionId,"invalid floor presentation");
        require(g.sceneBeat==SceneBeat::FloorFadeOut ? s.position==s.world.stairs : (s.floorIndex>0 && s.position==s.world.start),"invalid floor transition position");
    }
    if(g.sceneBeat==SceneBeat::WakeDelay || g.sceneBeat==SceneBeat::AlarmEnter || g.sceneBeat==SceneBeat::AlarmLeave) require(g.life==LifePhase::Wake && g.homeScene,"invalid wake delay");
    if(g.sceneBeat==SceneBeat::RoomExit) require(g.life==LifePhase::Ready && g.homeScene,"invalid bedroom exit");
    require(g.actionRound>0 && g.playerActionRound<=g.actionRound && g.playerDirection<8,"invalid action round");
    require(g.walkStartedMs<=g.phaseStartedMs,"invalid saved walking clock");
    require(static_cast<unsigned>(g.life) <= 10 && static_cast<unsigned>(g.combat) <= 17 &&
            g.hearts <= kMaxHp && g.oran <= kMaxStock && g.enemyHits <= 2 && g.damage <= 2 && g.day > 0 &&
            g.hostile.size() <= kTiles && g.onceKeys.size() <= 4096, "invalid gameplay state");
    if(g.life==LifePhase::Dialogue) {
        if(companionDialogue(g)) require(g.companion.enabled && !g.companion.hidden && g.companion.hearts>0 &&
            canAttack(s.world,s.position,g.companion.tile),"invalid companion dialogue owner");
        else require(std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance && e.kind==EntityKind::Friend && !e.done;}),"invalid dialogue owner");
    }
    require(g.enemyInstance<=kTiles || companionDialogue(g),"companion actor id outside dialogue");
    if (g.enabled) {
        require(g.deadlineMs >= g.phaseStartedMs, "invalid gameplay deadline");
        require(g.life != LifePhase::Combat || g.independentParty || (s.phase == Phase::Explore && g.enemyInstance > 0 &&
            std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) {
                return e.instanceId == g.enemyInstance && e.kind == EntityKind::Friend &&
                    (!e.done || g.combat == CombatPhase::Victory);
            })), "invalid combat owner");
        require((g.life == LifePhase::Explore || g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) ? s.phase == Phase::Explore :
            (g.life == LifePhase::Pose ? s.phase == Phase::Pose : s.phase == Phase::Home), "lifecycle/explorer mismatch");
        require(!g.homeScene || (g.life != LifePhase::Explore && g.life != LifePhase::Pose && g.life != LifePhase::Combat && g.life!=LifePhase::Dialogue), "home in dungeon state");
        require(g.life == LifePhase::Explore || g.life == LifePhase::Pose || g.life == LifePhase::Combat ||
                g.life == LifePhase::ReturnHome || g.life==LifePhase::Death || g.life==LifePhase::Dialogue || g.homeScene, "life event without home scene");
        if (g.life == LifePhase::ReturnHome || g.homeScene)
            require(g.onceKeys.count("return:" + std::to_string(s.runId)) != 0, "return transaction missing");
        const bool guildStart=(g.life==LifePhase::Wake || g.life==LifePhase::Ready) &&
            g.onceKeys.count("guild-start:"+std::to_string(s.runId));
        if (!guildStart && (g.life == LifePhase::Eat || g.life == LifePhase::Sleep || g.life == LifePhase::Wake || g.life == LifePhase::Ready))
            require(g.onceKeys.count("meal:" + std::to_string(s.runId)) && g.meals > 0, "meal transaction missing");
        std::set<std::uint32_t> unique;
        for (auto id : g.hostile) require(unique.insert(id).second &&
            std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) { return e.instanceId == id && e.kind == EntityKind::Friend; }), "invalid hostile catalogue");
        for (const auto& once : g.onceKeys) require(!once.empty() && once.size() <= 256, "invalid gameplay journal");
    }
    require(!s.assetVersion.empty() && s.assetVersion.size() <= 256, "invalid asset version");
    require(s.runId != 0 && !s.floors.empty() && s.floors.size() <= 128 &&
            s.floorIndex < s.floors.size(), "invalid run/floor");
    validatePolicy(s.spawnPolicy);
    for(const auto& p:s.floors)validateProfile(p);
    validateExpedition(s);
    validateWorld(s.world);
    require(static_cast<unsigned>(s.survivalFirst)<=2,"invalid survival discovery order");
    require(s.knownStairs==-1 || (s.knownStairs==s.world.stairs && s.discovered[s.world.stairs]),"invalid remembered stairs");
    require(s.visitedRooms<8 && s.stalledActions<=Explorer::kFloorStepLimit+kTiles+1,"invalid discovery counters");
    for(int p=0;p<kTiles;++p) require(s.discovered[p]<=1 && s.failedTargets[p]<=3 && (!s.discovered[p]||s.world.walkable(p)),"invalid discovery mask");
    auto validateMove=[&](const MoveRecord& m,int tile) {
        require(m.direction<8 && (!m.actionId || (m.actionId<=g.actionRound && m.durationMs==400 && m.startedMs<=UINT64_MAX-400 && m.to==tile && canMove(s.world,m.from,m.to))),"invalid saved movement");
    };
    validateMove(g.movement,s.position);for(const auto& e:s.population.entities) validateMove(e.movement,e.tile);
    const auto& partner=g.companion;
    require(partner.hearts<=kMaxHp && partner.oran<=kMaxStock && partner.direction<8 && partner.actionRound<=g.actionRound &&
        partner.walkStartedMs<=g.phaseStartedMs,"invalid companion stats/clock");
    if(partner.enabled) {
        require(g.enabled && s.world.walkable(partner.tile) && !s.blocked[partner.tile] && partner.tile!=s.position,"invalid companion tile");
        if(s.phase!=Phase::Home && !g.independentParty) require(canAttack(s.world,s.position,partner.tile),"companion exceeded one-tile radius");
        validateMove(partner.movement,partner.tile);
        for(const auto& e:s.population.entities) {
            const bool live=e.kind==EntityKind::Friend && e.visible && (!e.done || std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end());
            require(!live || e.tile!=partner.tile,"companion overlaps an actor");
        }
    }
    require(!g.enemyTargetsCompanion || partner.enabled,"enemy targets absent companion");
    require(static_cast<unsigned>(g.combat)<11 || g.combat==CombatPhase::PlayerApproach || partner.enabled,"companion combat without companion");
    require(s.world.version == static_cast<std::uint64_t>(s.floorIndex) + 1, "floor/grid version mismatch");
    require(s.world.walkable(s.position) && !s.blocked[s.position] && s.visited[s.position],
            "invalid explorer position");
    require(static_cast<unsigned>(s.phase) <= static_cast<unsigned>(Phase::Home), "invalid phase");
    require(static_cast<unsigned>(s.goalKind) <= static_cast<unsigned>(GoalKind::Auxiliary) &&
            ((s.target == -1) == (s.goalKind == GoalKind::None)), "invalid goal");
    if (s.target != -1) {
        require(s.phase == Phase::Explore && s.world.walkable(s.target) &&
                bfs(s.world, s.position).distance[s.target] >= 0, "invalid goal tile");
        if (s.goalKind == GoalKind::Stairs) require(s.target == s.world.stairs, "invalid stairs goal");
        if (s.goalKind == GoalKind::Item || s.goalKind == GoalKind::Friend)
            require(std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) {
                return e.instanceId==s.targetInstance && e.visible && (e.kind==EntityKind::Item ? e.tile==s.target : canAttack(s.world,s.target,e.tile)) && !e.done &&
                       e.kind == (s.goalKind == GoalKind::Item ? EntityKind::Item : EntityKind::Friend);
            }), "invalid entity goal");
    }
    require(s.floorSteps <= Explorer::kFloorStepLimit + kTiles + 1 &&
            s.poseSteps <= Explorer::kPoseTimeout, "invalid deadline counter");
    require(s.inventory.size() <= 128 * kTiles && s.onceKeys.size() <= 128 * (kTiles + 2),
            "journal count exceeds limit");
    for (int p = 0; p < kTiles; ++p)
        require(s.visited[p] <= 1 && s.blocked[p] <= 1 &&
                (!(s.visited[p] || s.blocked[p]) || s.world.walkable(p)), "invalid tile mask");
    const auto prefix = [&](const std::string& action, std::uint32_t floor, std::uint32_t instance) {
        return action + ":" + std::to_string(s.runId) + ":" + std::to_string(floor) + ":" + std::to_string(instance);
    };
    std::set<std::string> expected;
    for (std::uint32_t i = 0; i < s.floorIndex; ++i) expected.insert(prefix("stairs", i, 0));
    if (s.completed) {
        require(s.floorIndex + 1 == s.floors.size() && s.phase != Phase::Explore,
                "invalid completed phase");
        require(s.position == s.world.stairs, "clear away from stairs");
        expected.insert(prefix("stairs", s.floorIndex, 0));
        expected.insert(prefix("clear", s.floorIndex, 0));
    } else require(s.phase != Phase::Pose, "pose without clear");
    require(s.population.entities.size() <= kTiles && s.population.omissions.size() <= 16,
            "population count exceeds limit");
    Mask occupied{},itemOccupied{};
    for (std::size_t i = 0; i < s.population.entities.size(); ++i) {
        const auto& e = s.population.entities[i];
        const bool liveActor=e.kind==EntityKind::Friend && e.visible && (!e.done || (g.enabled && std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end()));
        require(e.instanceId == i + 1 && s.world.walkable(e.tile) &&
                (e.kind==EntityKind::Friend || (e.tile!=s.world.start && e.tile!=s.world.stairs)) &&
                (!liveActor || (e.tile!=s.position && !occupied[e.tile])) && (e.kind!=EntityKind::Item || !itemOccupied[e.tile]) &&
                static_cast<unsigned>(e.kind) <= 1, "invalid entity");
        if(liveActor) occupied[e.tile]=1;
        if(e.kind==EntityKind::Item) itemOccupied[e.tile]=1;
        require(static_cast<unsigned>(e.tracking)<=1 && e.direction<8 && e.remainingHits<=2 && e.actionRound<=g.actionRound &&
            e.blockedTurns<=3 && (e.lastSeen==-1||s.world.walkable(e.lastSeen)) && (e.chaseTarget==-1||s.world.walkable(e.chaseTarget)),"invalid actor tracking");
        const auto onceKey = prefix(e.kind == EntityKind::Item ? "pickup" : "greet",
                                   s.floorIndex, e.instanceId);
        require((s.onceKeys.count(onceKey) != 0) == e.done, "entity/journal mismatch");
        if (e.done) expected.insert(onceKey);
        if (e.done && e.kind == EntityKind::Item) {
            const auto it = std::find_if(s.inventory.begin(), s.inventory.end(),
                                        [&](const auto& item) { return item.onceKey == onceKey; });
            require(it != s.inventory.end() && it->originalId == e.originalId, "pickup/inventory mismatch");
        }
    }
    for (const auto& omission : s.population.omissions)
        require(static_cast<unsigned>(omission.kind) <= 1 && static_cast<unsigned>(omission.reason) <= 2 &&
                omission.count > 0, "invalid omission");
    std::set<std::string> inventoryKeys;
    for (const auto& item : s.inventory) {
        require(item.onceKey.size() <= 256 && s.onceKeys.count(item.onceKey) &&
                inventoryKeys.insert(item.onceKey).second, "invalid inventory journal");
        require(item.onceKey.rfind("pickup:", 0) == 0, "inventory is not a pickup");
        expected.insert(item.onceKey);
    }
    for (const auto& onceKey : s.onceKeys) {
        require(onceKey.size() <= 256, "onceKey exceeds limit");
        const auto a = onceKey.find(':'); const auto b = onceKey.find(':', a + 1);
        const auto c = onceKey.find(':', b + 1);
        require(a != std::string::npos && b != std::string::npos && c != std::string::npos,
                "invalid journal key syntax");
        const auto action = onceKey.substr(0, a);
        auto parse = [&](std::size_t start, std::size_t end) {
            std::uint64_t n = 0;
            const auto result = std::from_chars(onceKey.data() + start, onceKey.data() + end, n);
            require(result.ec == std::errc{} && result.ptr == onceKey.data() + end, "invalid journal key number");
            return n;
        };
        const auto run=parse(a + 1, b);
        if(run<s.runId && action=="pickup" && inventoryKeys.count(onceKey)) {
            // Historical pickups belong to an earlier expedition. Its floor count
            // is not the current route's size; retain the format's global bound.
            require(run>0 && parse(b+1,c)<128 && parse(c+1,onceKey.size())>0 && parse(c+1,onceKey.size())<=kTiles,"invalid historical pickup");
            continue;
        }
        require(run == s.runId, "journal run mismatch");
        const auto floor = parse(b + 1, c); const auto instance = parse(c + 1, onceKey.size());
        require(floor <= s.floorIndex && instance <= kTiles &&
                onceKey == prefix(action, static_cast<std::uint32_t>(floor), static_cast<std::uint32_t>(instance)),
                "invalid journal floor/instance");
        if (action == "pickup" || action == "greet") {
            require(instance > 0, "zero entity instance");
            if (action == "pickup") require(inventoryKeys.count(onceKey), "pickup without inventory");
            if (floor == s.floorIndex) require(expected.count(onceKey), "current entity missing from journal");
        } else require((action == "stairs" || action == "clear") && expected.count(onceKey), "unknown journal key");
    }
    for (const auto& onceKey : expected) require(s.onceKeys.count(onceKey), "missing transaction key");
}
std::vector<FloorProfile> fixtureProfiles(std::uint32_t floorCount) {
    require(floorCount > 0 && floorCount <= 128, "invalid fixture floor count");
    std::vector<FloorProfile> floors;
    for (std::uint32_t i = 0; i < floorCount; ++i)
        floors.push_back({"test-fixture", 0, i,
                          {{0, 1, {{10, 1}, {11, 3}}}, {1, 3, {{20, 2}, {21, 1}}}},
                          {{100, 1}, {101, 2}, {102, 7}}, {30}});
    return floors;
}
} // namespace pet
