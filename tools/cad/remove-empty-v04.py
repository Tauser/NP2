import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    s=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for name in ['Acabamento final / USB A','Acabamento final / USB C']:
        f=s.features.extrudeFeatures.itemByName(name)
        if f and f.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            print('DELETE EMPTY',name,f.deleteMe())
