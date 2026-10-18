import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V04' in app.activeDocument.name
    items=adsk.core.ObjectCollection.create()
    for o in root.occurrences:items.add(o)
    inp=d.createInterferenceInput(items);inp.areCoincidentFacesIncluded=False
    result=d.analyzeInterference(inp)
    print('INTERFERENCES',result.count)
    for r in result:
        print('OVERLAP',r.entityOne.parentComponent.name,r.entityTwo.parentComponent.name,'mm3',r.interferenceBody.volume*1000)
        pts=[p.geometry.copy() for p in r.interferenceBody.vertices]
        inv=root.occurrences.item(0).transform2.copy();inv.invert()
        for p in pts:p.transformBy(inv)
        print('OVERLAP LOCAL MM',[(min(getattr(p,a) for p in pts)*10,max(getattr(p,a) for p in pts)*10) for a in ['x','y','z']])
    for o in root.occurrences:
        pts=[]
        for b in o.component.bRepBodies:
            for vx in b.vertices:
                p=vx.geometry.copy();p.transformBy(o.transform2);pts.append(p)
        if pts:
            print('BOUNDS',o.component.name,[(round(min(getattr(p,a) for p in pts)*10,3),round(max(getattr(p,a) for p in pts)*10,3)) for a in ['x','y','z']])
            bb=o.boundingBox
            print('OCC BOUNDS',o.component.name,bb.minPoint.asArray(),bb.maxPoint.asArray())
    print('HEALTH',[(d.timeline.item(i).name,d.timeline.item(i).errorOrWarningMessage) for i in range(d.timeline.count) if d.timeline.item(i).healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState])
