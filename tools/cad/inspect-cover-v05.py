import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V05' in app.activeDocument.name
    rear=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('03 /'))
    for b in rear.bRepBodies:
        bb=b.boundingBox
        print(b.name,'VOLUME',b.volume*1000,'BBOX',[[x*10 for x in bb.minPoint.asArray()],[x*10 for x in bb.maxPoint.asArray()]])
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:print('HEALTH',it.name,it.objectType,it.errorOrWarningMessage)
