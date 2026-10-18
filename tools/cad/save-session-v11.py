"""Local durable checkpoints for every NP2 / official reference document.

Export archives instead of requiring a cloud project or closing unsaved tabs.
"""
import adsk.core,adsk.fusion,os,re,json

def run(_context: str):
 app=adsk.core.Application.get();original=app.activeDocument
 out='D:/Projetos/NP2/mechanical/session-2026-10-10'
 os.makedirs(out,exist_ok=True)
 documents=[doc for doc in app.documents if doc.name.startswith(('NP2','REF OFICIAL'))]
 saved=[];failures=[];assembly=None
 for index,doc in enumerate(documents,1):
  name=doc.name
  try:
   assert doc.activate()
   d=adsk.fusion.Design.cast(app.activeProduct)
   if not d:continue
   safe=re.sub(r'[<>:"/\\|?*]','-',name).strip(' .')[:110]
   path=out+'/'+str(index).zfill(2)+'-'+safe+'.f3d'
   assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(path))
   saved.append({'document':name,'archive':path,'bytes':os.path.getsize(path)})
   if 'Mesa V11' in name:
    assembly=doc
    latest='D:/Projetos/NP2/mechanical/concept-v11/NP2-case-v11'
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(latest+'.f3d'))
    assert d.exportManager.execute(d.exportManager.createSTEPExportOptions(latest+'.step'))
   if name.startswith('NP2 - V11 / pecas para ABS'):
    latest='D:/Projetos/NP2/mechanical/concept-v11/NP2-v11-pecas-impressao.f3d'
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(latest))
  except Exception as e:failures.append({'document':name,'error':str(e)})
  with open(out+'/save-manifest.json','w',encoding='utf8') as f:
   json.dump({'session_date':'2026-10-10','local_archives':saved,'failures':failures,'total_documents':len(documents)},f,ensure_ascii=False,indent=2)
 if original:original.activate()
 elif assembly:assembly.activate()
 print(json.dumps({'saved':len(saved),'failures':failures,'folder':out,'latest_v11_refreshed':bool(assembly)},ensure_ascii=False))
 assert not failures,'Some documents could not be saved; inspect save-manifest.json'
