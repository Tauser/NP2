"""Check durable local exports and record hashes for resuming the CAD session."""
from pathlib import Path
import hashlib,json,zipfile

root=Path('D:/Projetos/NP2')
session=root/'mechanical/session-2026-10-10'
saved=json.loads((session/'save-manifest.json').read_text(encoding='utf8'))
assert not saved['failures']
assert len(saved['local_archives'])==saved['total_documents']
paths=[]
for item in saved['local_archives']:
 path=Path(item['archive']);assert path.stat().st_size==item['bytes'] and item['bytes']>100
 assert zipfile.is_zipfile(path),path
 paths.append(path)
latest=root/'mechanical/concept-v11'
readback=json.loads((session/'fusion-readback.json').read_text(encoding='utf8'))
assert len(readback)==2 and all(r['fusion_import']=='passed' for r in readback)
for name in ['NP2-v11-ABS-350x350.3mf','NP2-v11-ABS-impressao.zip']:
 path=latest/name
 with zipfile.ZipFile(path) as archive:assert archive.testzip() is None,path
stls=list((latest/'STL').glob('*.stl'));assert len(stls)==10
paths.extend(path for path in latest.rglob('*') if path.is_file())
paths.extend(path for path in (root/'tools/cad').iterdir() if path.suffix in ['.py','.ps1'])
paths.extend([root/'mechanical/RETOMAR-CASE.md',session/'save-manifest.json',session/'fusion-readback.json'])
records=[]
for path in sorted(set(paths)):
 records.append({'file':path.relative_to(root).as_posix(),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
data={'session_date':'2026-10-10','saved_fusion_documents':len(saved['local_archives']),'latest_stl_parts':len(stls),'final_f3d_fusion_readback':'passed','print_3mf_zip_crc':'passed','files':records}
(session/'integrity-sha256.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'saved_documents':len(saved['local_archives']),'latest_stl_parts':len(stls),'files_hashed':len(records),'archive_integrity':'passed'},ensure_ascii=False))
