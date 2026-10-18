import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();current=app.activeDocument
    next(doc for doc in app.documents if doc.name.startswith('REF OFICIAL')).activate()
    d=adsk.fusion.Design.cast(app.activeProduct)
    for o in d.rootComponent.allOccurrences:
        if o.component.bRepBodies.count and any(t in o.fullPathName for t in ['+BOARD:','+7INCH-1024X600-TP:','+7INCH-1024X600-JHD']):
            print('COMP',o.component.name,'body faces',[b.faces.count for b in o.component.bRepBodies])
            for b in o.component.bRepBodies:
                for f in b.faces:
                    if f.area>5:
                        p=f.createForAssemblyContext(o);bb=p.boundingBox
                        print(o.component.name,'area',f.area,'BBmm',tuple(v*10 for v in bb.minPoint.asArray()),tuple(v*10 for v in bb.maxPoint.asArray()),'geom',f.geometry.objectType)
    current.activate()
