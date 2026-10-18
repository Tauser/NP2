"""Save session designs into the user's existing Fusion NP2 project."""
import adsk.core,json,re
OUT='D:/Projetos/NP2/mechanical/session-2026-10-10/cloud-save-manifest.json'

def run(_context: str):
 app=adsk.core.Application.get();assert not app.isOffLine
 matches=[(h,p) for h in app.data.dataHubs for p in h.dataProjects if p.name=='NP2'];assert len(matches)==1
 hub,project=matches[0];root=project.rootFolder;original=app.activeDocument
 docs=[doc for doc in app.documents if doc.name.startswith(('NP2','REF OFICIAL'))]
 assert docs,'No session documents found'
 main=next(doc for doc in docs if 'Mesa V11' in doc.name)
 printdoc=next(doc for doc in docs if 'V11' in doc.name and 'pecas para ABS' in doc.name)
 def folder(name):
  found=next((f for f in root.dataFolders if f.name==name),None)
  return found or root.dataFolders.add(name)
 history=folder('Historico da sessao - 2026-10-10');studies=folder('Estudos V11');assert history and studies
 jobs=[]
 for doc in [main,printdoc]+[doc for doc in docs if doc not in [main,printdoc]]:
  old=doc.name
  if doc==main:
   target=root;name='NP2 - Mesa V11 - dutos MIC removiveis';description='Case V11: dutos MIC removiveis em quatro metades abertas, traseira reta, berco integrado na tampa, sem bateria. Revisao CAD; prototipo fisico e audio pendentes.'
  elif doc==printdoc:
   target=root;name='NP2 - V11 - pecas para ABS - mesa 350x350';description='Dez pecas orientadas para impressao: seis pecas principais e quatro metades opcionais dos dutos MIC. ABS; mesa 350x350. Conferir o fatiamento antes de imprimir.'
  elif 'V11' in old:
   target=studies;name=re.sub(r'[<>:"/\\|?*]','-',old).strip(' .');description='Vista de estudo da V11. Consultar a montagem e o documento de pecas de impressao na raiz do projeto.'
  else:
   target=history;name=str(docs.index(doc)+1).zfill(2)+' - '+re.sub(r'[<>:"/\\|?*]','-',old).strip(' .');description='Checkpoint historico da sessao de 10/10/2026. A revisao atual e V11, disponivel na raiz do projeto NP2.'
  jobs.append((doc,old,target,name,description))
 saved=[];failure=None
 for doc,old,target,name,description in jobs:
  try:
   assert doc.activate()
   existing=doc.dataFile
   if existing and existing.parentFolder.id==target.id:
    if doc.isModified:assert doc.save(description)
   else:
    assert not any(f.name==name for f in target.dataFiles),'A file with this name already exists; preserve it and resolve association before retrying'
    assert doc.saveAs(name,target,description,'')
   f=doc.dataFile
   saved.append({'original_document':old,'document':doc.name,'requested_name':name,'folder':target.name,'folder_id':target.id,'file_id':f.id if f else None,'cloud_complete':f.isComplete if f else False})
  except Exception as e:
   failure={'document':old,'error':str(e)}
  data={'project':project.name,'project_id':project.id,'hub':hub.name,'saved':saved,'total_documents':len(jobs),'failure':failure}
  with open(OUT,'w',encoding='utf8') as output:json.dump(data,output,ensure_ascii=False,indent=2)
  if failure:break
 main.activate()
 print(json.dumps(data,ensure_ascii=False))
 assert not failure,'Cloud save interrupted; consult cloud-save-manifest.json'
