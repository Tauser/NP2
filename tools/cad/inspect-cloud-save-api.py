import adsk.core,json
def run(_context: str):
 app=adsk.core.Application.get();matches=[(h,p) for h in app.data.dataHubs for p in h.dataProjects if p.name=='NP2'];assert len(matches)==1
 h,p=matches[0];folder=p.rootFolder
 print(json.dumps({'hub':h.name,'project':p.name,'files':[{'name':f.name,'id':f.id} for f in folder.dataFiles],'folders':[{'name':f.name,'id':f.id} for f in folder.dataFolders],'saveAs_doc':adsk.core.Document.saveAs.__doc__,'isComplete_doc':getattr(adsk.core.DataFile,'isComplete').__doc__,'folder_add_doc':adsk.core.DataFolders.add.__doc__},ensure_ascii=False))
