"""보존 ROM의 전체 스크립트·동상·ground/tile 연결을 읽기 전용으로 조사한다.
실행: local/p1-venv/bin/python docs/research/2026-10-21-continuous-expeditions/audit_connectors.py
대사는 해독/저장하지 않는다. 추출 결과는 게임 런타임 입력이 아니다.
"""
from copy import copy
from pathlib import Path
import csv, hashlib, json, re, time
from datetime import datetime, timezone
from ndspy.rom import NintendoDSRom
from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.hardcoded.ground_dungeon_tilesets import HardcodedGroundDungeonTilesets
from skytemple_files.hardcoded.dungeons import HardcodedDungeons

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
sha = lambda b: hashlib.sha256(b).hexdigest()
def write(name, data):
    (OUT/name).write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n')
def walk(folder, prefix=''):
    for i, name in enumerate(folder.files):
        yield prefix+name, folder.firstID+i
    for name, sub in folder.folders:
        yield from walk(sub, prefix+name+'/')
def run():
    started = time.monotonic()
    source = ROOT/'assets/original/source/user-supplied.nds'
    before = sha(source.read_bytes())
    rom = NintendoDSRom.fromFile(str(source)); config = get_ppmdu_config_for_rom(rom)
    codec = copy(config); codec.string_encoding = 'latin-1'
    paths = dict(walk(rom.filenames)); scripts = {}; inventory = []; statues = []; layout_count = 0
    for path, index in paths.items():
        raw = rom.files[index]
        if not path.startswith('SCRIPT/'): continue
        if path.endswith('.ssb'):
            script = FileType.SSB.deserialize(raw, codec).to_ssb_script()[0]
            scripts[path] = script
            inventory.append({'path':path, 'bytes':len(raw), 'sha256':sha(raw)})
        elif path.endswith(('.ssa','.sse','.sss')):
            layout_count += 1
            layout = FileType.SSA.deserialize(raw, config.script_data); actors = []
            for li, layer in enumerate(layout.layer_list):
                for ai, actor in enumerate(layer.actors):
                    if actor.actor.entid != 554: continue
                    p = actor.pos
                    actors.append({'layer':li, 'actorIndex':ai, 'actorId':actor.actor.id,
                        'actorName':actor.actor.name, 'entid':554, 'scriptId':actor.script_id,
                        'position':{'xRelative':p.x_relative,'yRelative':p.y_relative,
                                    'xOffset':p.x_offset,'yOffset':p.y_offset}})
            if actors: statues.append({'path':path, 'sha256':sha(raw), 'actors':actors})
    with (OUT/'script-manifest.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['path','bytes','sha256']);writer.writeheader();writer.writerows(inventory)
    write('statue-layouts.json',statues)
    mapping = HardcodedGroundDungeonTilesets.get_ground_dungeon_tilesets(get_binary_from_rom(rom,config.bin_sections.overlay11),config)
    definitions = HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom,config.bin_sections.arm9),config)
    mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    write('ground-tileset-mappings.json',[{'groundLevel':int(m.ground_level),'dungeonId':int(m.dungeon_id),
        'floorId':int(m.floor_id),'unk3':int(m.unk3)} for m in mapping])
    bglist=FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'))
    levels={s['path'].split('/')[1] for s in statues if s['path'].endswith('enter.sse')}
    # 동상이 없는 분기·고정 전투·연출 구간도 구별해 색인화한다.
    levels.update(['D32P11A','D32P12A','D32P13A','D32P14A','D57P41A','D57P42A','D73P25A',
        'D42P31A','D42P41A','D42P42A','D28P31A','D30P32A','D05P31A','D14P11A',
        'D17P45A','D21P41A','D26P43A','D73P41A','H02P99A','P02P01A'])
    grounds=[]; evidence=[]
    op_pattern=re.compile(r'\b(?:back_SetGround|main_SetGround|main_EnterGround|main_EnterDungeon|back_SetDungeonBanner|flag_SetScenario|message_Menu|CallCommon|JumpCommon|supervision_ExecuteActingSub)\(')
    for level in sorted(levels):
        lev=config.script_data.level_list__by_name[level]; bg=bglist.level[lev.mapid]
        refs=[]
        for name,ext in [(bg.bma_name,'bma'),(bg.bpc_name,'bpc'),(bg.bpl_name,'bpl')]+[(n,'bpa') for n in bg.bpa_names if n]:
            p=f'MAP_BG/{name.lower()}.{ext}';refs.append({'path':p,'sha256':sha(rom.getFileByName(p))})
        layout=next((s for s in statues if s['path']==f'SCRIPT/{level}/enter.sse'),None)
        interactions=[]
        for actor in layout['actors'] if layout else []:
            p=f"SCRIPT/{level}/enter{actor['scriptId']:02d}.ssb";s=scripts.get(p,'')
            interactions.append({'actorId':actor['actorId'],'scriptId':actor['scriptId'],'path':p,
                'sha256':sha(rom.getFileByName(p)) if s else None,'saveConfirmed':'CORO_HANYOU_SAVE_FUNC' in s,
                'storageConfirmed':'message_Menu(MENU_STORAGE)' in s,
                'opcodes':[l.strip() for l in s.splitlines() if re.search(r'HANYOU_SAVE|MENU_STORAGE|SCENARIO_MAIN_BIT_FLAG',l)]})
        tile=next((m for m in mapping if m.ground_level==lev.id),None);tile_ref=None
        if tile:
            d=definitions[tile.dungeon_id];local=min(d.number_floors-1,tile.floor_id-1)
            gf=min(d.start_after+local,len(mappa.floor_lists[d.mappa_index])-1);fl=mappa.floor_lists[d.mappa_index][gf]
            tile_ref={'sourceDungeonId':int(tile.dungeon_id),'sourceFloorIdRaw':int(tile.floor_id),
                'resolvedLocalFloor0':int(gf-d.start_after),'mappaGroup':int(d.mappa_index),'groupFloor0':int(gf),
                'tilesetId':int(fl.layout.tileset_id),'fixedFloorId':int(fl.layout.fixed_floor_id)}
        grounds.append({'id':f'GND-L{lev.id:03d}-{level}','level':level,'configLevelId':lev.id,'mapBgIndex':lev.mapid,
            'mapType':lev.mapty,'backgroundSources':refs,'tilesetMapping':tile_ref,
            'statueLayout':layout,'interactions':interactions,
            'statueStatus':'confirmed' if interactions and all(x['saveConfirmed'] for x in interactions) else 'not_detected_by_actor_scan'})
    bound_scripts={i['path'] for g in grounds for i in g['interactions']}
    for path,script in scripts.items():
        filename=path.rsplit('/',1)[-1]
        if filename.lower().startswith(('um','us')):continue
        if filename.startswith('enter') and filename!='enter00.ssb' and path not in bound_scripts:continue
        if path.split('/')[1] not in levels and path!='SCRIPT/COMMON/unionall.ssb':continue
        lines=[]
        for i,l in enumerate(script.splitlines()):
            # 메시지 본문과 ROM 문자열 데이터는 수록하지 않는다.
            if op_pattern.search(l):
                if ('CallCommon' in l or 'JumpCommon' in l) and not re.search(r'HANYOU_SAVE',l):continue
                lines.append({'line':i+1,'opcode':l.strip()})
        if lines:evidence.append({'path':path,'sha256':sha(rom.getFileByName(path)),'lines':lines})
    write('grounds.json',grounds);write('routing-evidence.json',evidence)
    assert before==sha(source.read_bytes())
    write('extraction.json',{'checkedAtUtc':datetime.now(timezone.utc).isoformat(),'seconds':time.monotonic()-started,
        'romSha256':before,'romUnchanged':True,'scripts':len(scripts),'layouts':layout_count,'parseErrors':0,
        'statueLayouts':len(statues),'statueEnterGrounds':sum(x['path'].endswith('enter.sse') for x in statues),
        'grounds':len(grounds),'overlay11Sha256':sha(get_binary_from_rom(rom,config.bin_sections.overlay11)),
        'stringPolicy':'latin-1 only to preserve opcode structure; no dialogue decoding or translation claim',
        'groundIdPolicy':'SkyTemple EoS_NA config ID + exact ROM script directory; not a new ROM dungeon ID'})
    print('scripts',len(scripts),'layouts',layout_count,'grounds',len(grounds),'errors',0)
if __name__=='__main__':run()
