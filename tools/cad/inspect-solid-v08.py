import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    for o in d.rootComponent.occurrences:
        if o.component.name.startswith('REF'):continue
        print('COMP',o.component.name,'BODIES',o.component.bRepBodies.count)
        for b in o.component.bRepBodies:
            print('BODY',b.name,'solid',b.isSolid,'lumps',b.lumps.count,'vol',b.volume*1000)
            for l in b.lumps:
                print('LUMP',l.volume*1000,[[v*10 for v in l.boundingBox.minPoint.asArray()],[v*10 for v in l.boundingBox.maxPoint.asArray()]])
    for i in range(d.timeline.count):
        t=d.timeline.item(i)
        if t.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState and t.errorOrWarningMessage:print('HEALTH',t.name,t.errorOrWarningMessage)
