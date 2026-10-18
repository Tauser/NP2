import adsk.core,adsk.fusion,json
def run(_context: str):
 app=adsk.core.Application.get();data=app.data
 hubs=[]
 for hub in data.dataHubs:
  projects=[]
  for p in hub.dataProjects:
   projects.append({'name':p.name,'id':p.id,'root_folder_id':p.rootFolder.id})
  hubs.append({'name':hub.name,'id':hub.id,'projects':projects})
 docs=[]
 for doc in app.documents:
  if 'V11' not in doc.name:continue
  file=doc.dataFile
  docs.append({'name':doc.name,'saved':bool(file),'file_id':file.id if file else None,'parent_folder':file.parentFolder.name if file else None})
 print(json.dumps({'offline':app.isOffLine,'active_document':app.activeDocument.name,'hubs':hubs,'v11_documents':docs},ensure_ascii=False))
