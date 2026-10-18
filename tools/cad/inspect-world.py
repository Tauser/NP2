import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    for o in d.rootComponent.occurrences:
        pnts=[];native=[]
        for b in o.component.bRepBodies:
            for v in b.vertices:
                p=v.geometry.copy();native.append(p.copy());p.transformBy(o.transform2);pnts.append(p)
        print(o.component.name,'WORLD',[(round(min(getattr(p,a) for p in pnts)*10,3),round(max(getattr(p,a) for p in pnts)*10,3)) for a in ['x','y','z']],
              'NATIVE',[(round(min(getattr(p,a) for p in native)*10,3),round(max(getattr(p,a) for p in native)*10,3)) for a in ['x','y','z']])
    for cid in ['ViewLayoutGridOnCommand','SketchGridOn']:
        cd=adsk.core.Application.get().userInterface.commandDefinitions.itemById(cid)
        print(cid,cd.controlDefinition.objectType,dir(cd.controlDefinition))
