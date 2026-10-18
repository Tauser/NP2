import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    s=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for f in reversed(list(s.features.extrudeFeatures)):
        if f.name.startswith('Acabamento final /') or f.name.startswith('Bolso INTERNO cursor'):
            print('REMOVE',f.name);f.deleteMe()
    p=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('08 /'))
    print('POWER LOCAL BOUNDS',p.bRepBodies.item(0).boundingBox.minPoint.asArray(),p.bRepBodies.item(0).boundingBox.maxPoint.asArray())
