"""Confirm NP2 saved files and asynchronous cloud completion."""
import adsk.core,json
BASE='D:/Projetos/NP2/mechanical/session-2026-10-10/'
def run(_context: str):
 app=adsk.core.Application.get()
 with open(BASE+'cloud-save-manifest.json',encoding='utf8') as f:saved=json.load(f)
 project=next(p for h in app.data.dataHubs for p in h.dataProjects if p.id==saved['project_id'])
 root=project.rootFolder;folders=[root]+list(root.dataFolders)
 files={}
 for folder in folders:
  for f in folder.dataFiles:files[f.id]=(folder,f)
 results=[]
 for row in saved['saved']:
  match=files.get(row['file_id'])
  if match is None:
   match=next(((folder,f) for folder,f in files.values() if folder.id==row['folder_id'] and f.name==row['requested_name']),None)
  if match:
   folder,f=match
   results.append({'name':f.name,'id':f.id,'folder':folder.name,'folder_id':folder.id,'complete':f.isComplete,'version':f.versionNumber})
  else:results.append({'name':row['requested_name'],'folder':row['folder'],'complete':False,'missing_from_folder_listing':True})
 data={'project':project.name,'hub':saved['hub'],'expected':saved['total_documents'],'found':sum(not r.get('missing_from_folder_listing',False) for r in results),'complete':sum(r['complete'] for r in results),'files':results,'save_failure':saved['failure']}
 with open(BASE+'cloud-check.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print(json.dumps(data,ensure_ascii=False))
