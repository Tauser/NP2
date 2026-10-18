import adsk.core, adsk.fusion, json, os
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'REF OFICIAL' in app.activeDocument.name
    data=[]
    for o in root.allOccurrences:
        if o.component.bRepBodies.count:
            bodies=[]
            for b in o.component.bRepBodies:
                p=b.createForAssemblyContext(o);bb=p.boundingBox
                bodies.append({'name':b.name,'min':[round(v*10,4) for v in bb.minPoint.asArray()],'max':[round(v*10,4) for v in bb.maxPoint.asArray()]})
            data.append({'path':o.fullPathName,'bodies':bodies})
    out='D:/Projetos/NP2/mechanical/reference/waveshare-7b/official-body-dimensions.json'
    with open(out,'w',encoding='utf-8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
    print('PARTS',len(data),'JSON',out)
    for part in data:
        names=part['path']+' '.join(b['name'] for b in part['bodies'])
        if any(x in names.lower() for x in ['pcb','usb','lcd','screw','m2','spacer','switch','tp','glass','type','j1','j2','j3','j4','j5']):
            print(json.dumps(part,ensure_ascii=False))
    root.isOriginFolderLightBulbOn=False
    for o in root.allOccurrences:o.component.isOriginFolderLightBulbOn=False
    app.activeViewport.fit();app.activeViewport.refresh()
    app.activeViewport.saveAsImageFile('D:/Projetos/NP2/mechanical/reference/waveshare-7b/official-3d.png',1600,1200)
