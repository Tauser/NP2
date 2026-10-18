import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    s=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    print('TRANSFORM',s.transform2.asArray())
    print('BODIES',s.component.bRepBodies.count)
    print('LOFTS',[(f.name,f.healthState) for f in s.component.features.loftFeatures])
    print('TAIL',[(d.timeline.item(i).name,d.timeline.item(i).healthState) for i in range(max(0,d.timeline.count-22),d.timeline.count)])
