"""Read back exported F3D files in Fusion, without closing existing documents."""
import adsk.core,adsk.fusion,json

def run(_context: str):
 app=adsk.core.Application.get();original=app.activeDocument;results=[]
 for name,expected in [('NP2-case-v11.f3d',None),('NP2-v11-pecas-impressao.f3d',10)]:
  path='D:/Projetos/NP2/mechanical/concept-v11/'+name
  doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(path));assert doc
  try:
   d=adsk.fusion.Design.cast(app.activeProduct);assert d
   count=d.rootComponent.occurrences.count
   if expected is not None:assert count==expected
   else:
    components=[o.component.name for o in d.rootComponent.occurrences]
    assert any(n.startswith('09 /') for n in components)
    assert any(n.startswith('REF OFICIAL') for n in components)
   results.append({'archive':name,'fusion_import':'passed','root_occurrences':count})
  finally:
   # Only close the temporary readback document created above.
   doc.close(False)
 original.activate()
 with open('D:/Projetos/NP2/mechanical/session-2026-10-10/fusion-readback.json','w',encoding='utf8') as f:json.dump(results,f,ensure_ascii=False,indent=2)
 print(json.dumps(results,ensure_ascii=False))
