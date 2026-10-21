"""사람이 웹·ROM을 대조한 연결 규칙을 안정된 분류 ID와 외래키로 정리한다.
숫자순·MAPPA 그룹만으로 스토리 연결을 추론하지 않는다. 런타임 설정이 아니다.
"""
from pathlib import Path
import json,csv
OUT=Path(__file__).resolve().parent
read=lambda name:json.loads((OUT/name).read_text())
def write(name,obj):(OUT/name).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
def run():
 definitions=json.loads((OUT.parent/'2026-10-21-full-map-audit/dungeon-definitions.json').read_text())
 families=json.loads((OUT.parent/'2026-10-21-full-map-audit/route-groups.json').read_text())
 grounds=read('grounds.json');by_ground={g['level']:g for g in grounds};by_dungeon={d['dungeonId']:d for d in definitions}
 gid=lambda level:by_ground[level]['id']
 # 마지막 열은 해당 맵이 연결하는 구간이다. 입구·막간·분기·중간 쉼터를 분리한다.
 specs=[
 ('D06P11A',None,8,'entry','길드 원정 출발 / Side Path9 왕복'),
 ('D07P11A',8,10,'between_dungeons','Craggy Coast 통과 / Rock Path11 왕복'),
 ('D08P11A',10,12,'camp','원정 베이스캠프 / Forest Path13 왕복'),
 ('D09P11A',12,14,'between_dungeons','Foggy Forest 뒤 Steam Cave 입구'),
 ('D10P21A',14,15,'midpoint','Steam Cave 8층 뒤'),
 ('D12P21A',17,18,'midpoint','Amp Plains 10층 뒤'),
 ('D15P21A',21,22,'midpoint','Quicksand Cave 10층 뒤'),
 ('D16P31A',24,25,'puzzle','수정 기둥 퍼즐 / Crystal Crossing 입구'),
 ('D18P11A',None,27,'entry','미래 탈출 뒤 Chasm Cave 입구'),
 ('D19P11A',27,28,'between_dungeons','Chasm Cave 뒤 Dark Hill 입구'),
 ('D20P11A',28,29,'between_dungeons','Dark Hill 뒤 Sealed Ruin 입구'),
 ('D21P21A',29,30,'midpoint','Sealed Ruin 8층 뒤'),
 ('D22P11A',31,32,'between_dungeons','Sealed Ruin Pit 뒤 Dusk Forest 입구; SE5도 재사용'),
 ('D23P11A',32,33,'between_dungeons','Dusk Forest 뒤 Celebi 합류 / Deep Dusk Forest 입구'),
 ('D24P11A',33,34,'entry_after_story_camp','과거 귀환·Sharpedo Bluff 은신 뒤 Treeshroud Forest 입구'),
 ('D25P11A',None,35,'entry','Brine Cave 입구'),
 ('D26P21A',35,36,'midpoint','Brine Cave 9층 뒤'),
 ('D27P11A',37,38,'between_dungeons','Lapras 이동 뒤 Hidden Land 입구; 선택 귀환 가능'),
 ('D28P21A',38,39,'midpoint','Hidden Land 15층 뒤'),
 ('D29P11A',40,41,'between_dungeons','Old Ruins·Rainbow Stoneship 뒤 시한의 탑 입구'),
 ('D30P21A',41,42,'midpoint','시한의 탑 13층 뒤 Temporal Spire'),
 ('D31P11A',None,44,'entry','Mystifying Forest 입구'),
 ('D35P21A',51,52,'midpoint','Miracle Sea 18층 뒤'),
 ('D38P12A',None,64,'entry','Spacial Rift 재시도 입구; 첫 진입 연출은 별도'),
 ('D39P21A',64,65,'midpoint','Spacial Rift 15층 뒤'),
 ('D41P21A',67,68,'midpoint','Dark Crater 15층 뒤'),
 ('D42P21A',123,124,'midpoint','SE1 Star Cave 7층 뒤; 실패 복귀에도 재사용'),
 ('D45P21A',130,131,'midpoint','SE2 Fortune Ravine 8층 뒤'),
 ('D46P11A',None,133,'entry','SE5 Barren Valley 입구'),
 ('D46P21A',133,134,'midpoint','SE5 Barren Valley 5층 뒤'),
 ('D47P11A',135,136,'between_dungeons','SE5 Dark Wasteland 입구'),
 ('D48P11A',136,137,'between_dungeons','SE5 시한의 탑 입구'),
 ('D48P21A',137,138,'midpoint','SE5 시한의 탑 10층 뒤; 본편 L068과 별도'),
 ('D50P11A',140,141,'between_dungeons','SE5 Black Swamp 뒤 Spacial Cliffs 입구'),
 ('D51P11A',141,142,'between_dungeons','SE5 Porygon 이동 뒤 Dark Ice Mountain 입구'),
 ('D51P21A',142,143,'midpoint','SE5 Dark Ice Mountain 10층 뒤'),
 ('D52P11A',144,145,'between_dungeons','SE5 Icicle Forest 입구'),
 ('D53P11A',145,146,'between_dungeons','SE5 Vast Ice Mountain 입구'),
 ('D53P21A',146,147,'midpoint','SE5 Vast Ice Mountain 9층 뒤'),
 ('D54P11A',None,149,'entry','SE4 Southern Jungle 입구'),
 ('D55P11A',149,150,'between_dungeons','SE4 Boulder Quarry 입구'),
 ('D55P21A',150,151,'midpoint','SE4 Boulder Quarry 5층 뒤'),
 ('D56P11A',152,155,'branch_hub','SE4 Left154 / Right153 반복 뒤 숨은 통로로 Limestone Cavern'),
 ('D56P21A',155,156,'midpoint','SE4 Limestone Cavern 5층 뒤'),
 ('D57P21A',162,163,'midpoint','SE3 Lower Spring Cave 4층 뒤; 유일하게 배우 동상을 확인한 막간'),
 ('D73P11A',None,111,'entry','Shaymin Village / Sky Peak 출발'),
 ('D73P21A',111,112,'station','Sky Peak 1st Station'),
 ('D73P22A',112,113,'station','Sky Peak 2nd Station'),
 ('D73P23A',113,114,'station','Sky Peak 3rd Station'),
 ('D73P24A',114,115,'station','Sky Peak 4th Station'),
 ('D73P26A',116,117,'station','Sky Peak 6th Station'),
 ('D73P27A',117,118,'station','Sky Peak 7th Station'),
 ('D73P28A',118,119,'station','Sky Peak 8th Station / Ampharos'),
 ('D73P29A',119,120,'station','Sky Peak 9th Station'),
 ('D79P11A',46,47,'between_dungeons','Blizzard Island 뒤 Crevice Cave 입구'),
 ('D79P21A',47,48,'midpoint','Crevice Cave 10층 뒤'),
 ('P19P01A',None,129,'entry','SE2 Eastern Cave 입구; 전날 던전과 강제 연결하지 않음'),
 ]
 connectors=[];via={}
 for level,a,b,kind,note in specs:
  g=by_ground[level];assert g['statueStatus']=='confirmed'
  row={'id':f"JNC-{level}",'groundId':g['id'],'fromDungeonId':a,'toDungeonId':b,'kind':kind,'note':note,
       'save':True,'storage':any(i['storageConfirmed'] for i in g['interactions'])}
  connectors.append(row)
  if a is not None:via[(a,b)]=[level]
 # 별도 동상 없는 연출 맵도 연결 경유지로 기록한다. 미검출을 완전한 부재 증명으로 쓰지 않는다.
 via.update({(33,34):['H02P99A','D24P11A'],(54,55):['D32P11A'],(55,56):['D32P12A'],(56,57):['D32P12A'],
   (57,58):['D32P13A'],(58,59):['D32P13A'],(124,125):['D42P41A'],(125,126):['D42P31A'],(126,127):['D42P42A'],
   (158,159):['D57P41A'],(159,160):['D57P41A'],(160,161):['D57P42A'],(161,162):['D57P42A'],
   (115,121):['D73P25A'],(121,116):['D73P25A'],(138,139):['D30P32A','D22P11A']})
 links={}
 def link(a,b,condition='forward_success',kind='advance',levels=None):
  identity=f'LNK-D{a:03d}-D{b:03d}'+('~'+kind if kind!='advance' else '')
  gl=levels if levels is not None else via.get((a,b),[])
  if identity not in links:
   links[identity]={'id':identity,'from':f'SEG-D{a:03d}','to':f'SEG-D{b:03d}',
    'fromDungeonId':a,'toDungeonId':b,'viaGroundIds':[gid(g) for g in gl],
    'statueGroundIds':[gid(g) for g in gl if by_ground[g]['statueStatus']=='confirmed'],
    'kind':kind,'condition':condition,'guildReturnBeforeNext':False,
    'groundResolution':'confirmed_path' if gl else 'no_separate_connector_assigned'}
  return identity
 runs=[]
 for family in families:
  ids=family['dungeonIds']
  if len(ids)<2 or family['category']=='dojo' or family['group']==35:continue
  if family['group']==72:ids=[111,112,113,114,115,121,116,117,118,119,120,122]
  if family['group']==17:ids=[24,25,26]
  runs.append({'id':f'RUN-D{ids[0]:03d}','name':'Crystal Cave → Crystal Crossing' if family['group']==17 else family['name'],'category':family['category'],
    'dungeonIds':ids,'sequenceKind':'first_story_forward','links':[link(a,b) for a,b in zip(ids,ids[1:])],
    'note':'구간 완료는 원정 완료와 다름. 보스 고정층도 별도 SEG ID를 유지한다.'})
 for identity,name,ids in [(32,'Dusk Forest → Deep Dusk Forest',[32,33]),(46,'Blizzard Island → Crevice Cave',[46,47,48,49])]:
  runs.append({'id':f'RUN-D{identity:03d}','name':name,'category':'main-story' if identity==32 else 'post-story',
   'dungeonIds':ids,'sequenceKind':'first_story_forward','links':[link(a,b) for a,b in zip(ids,ids[1:])],
   'note':'서로 다른 MAPPA 그룹이지만 원정은 연속한다.'})
 arcs=[]
 for identity,name,ids,note in [
 ('ARC-M07','길드 대원정 / Fogbound Lake',[8,10,12,14,15,16],'야영·스토리 대기 포함. Side Path9/Rock Path11/Forest Path13은 진행 경로가 아닌 왕복 분기.'),
 ('ARC-M14','미래 탈출 → 과거 은신 → Treeshroud Forest',[27,28,29,30,31,32,33,34],'33→34 사이 시간의 통로·해변·Sharpedo Bluff 은신/취침을 거친다. 길드 강제 귀환 없이 이어지지만 직접 맞닿은 던전은 아니다.'),
 ('ARC-M18','Brine Cave → Hidden Land → 시한의 탑',[35,36,37,38,39,40,41,42,43],'Lapras·Old Ruins·Rainbow Stoneship 포함. 길드 장면은 동료들의 시점 전환이며 주인공의 귀환이 아님. Hidden Land 입구 선택 귀환과 실패 복귀는 별도.'),
 ('ARC-SE04','Here Comes Team Charm!',[149,150,151,152,155,156,157],'153/154는 D56P11A로 돌아오는 분기. 숨은 통로 발견 후155로 전진.'),
 ('ARC-SE05','In the Future of Darkness',[133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148],'Porygon 이동·막간/휴식 포함. 본편과 별개 시간대·배우·저장을 사용하며 본편41/42와 SE137/138을 섞지 않는다.')]:
  arcs.append({'id':identity,'name':name,'dungeonIds':ids,'links':[link(a,b) for a,b in zip(ids,ids[1:])],
    'continuity':'story_forward_without_forced_guild_return','note':note})
 branches=[]
 for a,b,ground in [(8,9,'D06P11A'),(10,11,'D07P11A'),(12,13,'D08P11A'),(155,153,'D56P11A'),(155,154,'D56P11A')]:
  branches.append({'anchorSegment':f'SEG-D{a:03d}','optionalSegment':f'SEG-D{b:03d}','groundId':gid(ground),
   'fromGroundId':gid(ground),'viaDungeonId':b,'returnGroundId':gid(ground),'nextForwardDungeonId':a,
   'semantics':'갈림길에서 선택 후 같은 ground로 복귀; anchor 던전 완료 후 자동 연결하지 않음'})
 for a,ground in [(54,'D32P11A'),(56,'D32P12A'),(58,'D32P13A')]:
  link(a,a,'required_unown_stones_missing','puzzle_loop',[ground])
  links[f'LNK-D{a:03d}-D{a+1:03d}']['condition']='required_unown_stones_collected_and_monument_activated'
 alternate=[link(115,116,'Sky Peak 재방문: Carnivine 고정전121 생략','revisit',['D73P25A']),
  link(20,21,'첫 조사 때는 길드 귀환. Quicksand 해금 전 재탐색/엔딩 후 선택 연결만 허용','conditional_revisit',['D14P11A'])]
 entry_map={row['toDungeonId']:row['groundId'] for row in connectors if row['fromDungeonId'] is None}
 for r in runs:r['entryGroundId']=entry_map.get(r['dungeonIds'][0])
 segments=[{'id':f"SEG-D{d['dungeonId']:03d}",'dungeonId':d['dungeonId'],'symbol':d['symbol'],
  'familyId':f"FAM-G{d['group']:03d}",'mappaGroup':d['group'],'startAfter':d['startAfter'],
  'floors':d['declaredFloors'],'resolvedFloors':d['resolvedFloors'],'fixedFloors':d['fixedFloors']} for d in definitions]
 data={'schemaVersion':1,'status':'research_classification_not_runtime_configuration','segments':segments,
  'families':[{'id':f"FAM-G{f['group']:03d}",'name':f['name'],'group':f['group'],'category':f['category'],'dungeonIds':f['dungeonIds'],'webUrl':f['url']} for f in families],
  'connectors':connectors,'sectionRoutes':runs,'storyArcs':arcs,'links':list(links.values()),'branches':branches,'alternateLinks':alternate,
  'notContinuousByDefault':[
   {'from':20,'to':21,'reason':'북쪽 사막 첫 탐색 뒤 길드에서 보고·식사·다음 날 재출발. 연속은 조건부 변형만.'},
   {'from':62,'to':63,'reason':'Mt. Travail에서 Drowzee를 데려온 뒤 마을/길드·Azurill의 꿈 진입. 정상 통과 직결이 아님.'},
   {'from':128,'to':129,'reason':'SE2 Murky Forest→Armaldo의 거처·훈련/날짜 전환. 하나의 던전 원정으로 묶지 않음.'},
   {'from':129,'to':130,'reason':'SE2 Eastern Cave 뒤 거처 복귀·다음 모험. 에피소드 전체 순서와 한 번의 원정을 구별.'}],
  'unresolved':[
   {'topic':'Aegis Cave 동상','status':'web_rom_disagreement','grounds':[gid(g) for g in ['D32P11A','D32P12A','D32P13A','D32P14A']],
    'detail':'웹 헤더는 3/4 rest stops로 다르다. stone 퍼즐 ground는 확인했지만 전체 SSA에서 actor entid554와 해당 ground의 HANYOU_SAVE를 못 찾음. 동상 확정 수에 포함하지 않음. 별도 오브젝트/엔진 처리는 미확인.'},
   {'topic':'SE1/SE3 웹의 추가 rest stop','status':'terminology_or_missing_evidence',
    'detail':'Star Cave의 Deep5, Spring Cave의4/Upper5 표기를 모두 캥카 ground로 간주하지 않음. 실제 동상과 상호작용을 확인한 D42P21A/D57P21A만 확정.'},
   {'topic':'Concealed Ruins71','status':'invalid_floor_reference','detail':'앞 조사에서 group35 floor30 참조 부재.70→71 자동 연결 후보로 채택하지 않음.'}]}
 known_groups={f['group'] for f in data['families']}
 for group in range(100):
  if group not in known_groups:
   data['families'].append({'id':f'FAM-G{group:03d}','name':'원작 테스트/시스템/미확정 그룹','group':group,'category':'system-or-unresolved','dungeonIds':[d['dungeonId'] for d in definitions if d['group']==group],'webUrl':None})
 for family in data['families']:
  family['definitionDungeonIds']=[d['dungeonId'] for d in definitions if d['group']==family['group']]
 for segment in data['segments']:
  identity=segment['dungeonId']
  segment['usage']='ground_alias_not_playable' if 175<=identity<=178 else 'test' if identity==0 else 'demo' if 165<=identity<=170 else 'dojo' if identity>=180 else 'rescue_system' if identity==179 else 'dummy_unresolved' if identity in (105,106,171,172,173) else 'dungeon'
 for row in data['links']:
  if row['fromDungeonId']==115 and row['toDungeonId']==121:row['condition']='first_visit_Carnivine_event_pending'
  row['evidenceBasis']='웹의 진행 순서 + ROM ground/목적지 opcode 대조; 분기 실행 검증은 아님'
  refs=[]
  for evidence in read('routing-evidence.json'):
   if row['viaGroundIds'] and evidence['path'].split('/')[1] not in {g.split('-')[-1] for g in row['viaGroundIds']} and evidence['path']!='SCRIPT/COMMON/unionall.ssb':continue
   for line in evidence['lines']:
    if line['opcode'].startswith('main_EnterDungeon('+str(row['toDungeonId'])+','):
     refs.append({'path':evidence['path'],'sha256':evidence['sha256'],**line})
  row['destinationOpcodeRefs']=refs[:3]
 write('routes.json',data)
 with (OUT/'connectors.csv').open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(connectors[0]));w.writeheader();w.writerows(connectors)
 # CSV는 평탄한 외래키 인덱스. 실제 참조/조건은 JSON에 보존한다.
 with (OUT/'links.csv').open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=['id','from','to','viaGroundIds','statueGroundIds','kind','condition']);w.writeheader()
  for l in links.values():w.writerow({k:';'.join(l[k]) if isinstance(l[k],list) else l[k] for k in w.fieldnames})
 print('segments',len(segments),'routes',len(runs),'arcs',len(arcs),'connectors',len(connectors),'links',len(links))
if __name__=='__main__':run()
