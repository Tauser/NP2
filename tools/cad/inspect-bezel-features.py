import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('01 /'))
    for f in c.features:print(f.objectType,f.name)
    for s in c.sketches:print('SKETCH',s.name)
