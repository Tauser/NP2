import adsk.core,adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    for o in d.rootComponent.occurrences:
        print(o.component.name,'OCC',o.isLightBulbOn,'T',o.transform2.asArray())
        for b in o.component.bRepBodies:print(b.name,'B',b.isLightBulbOn,'VISIBLE',b.isVisible)
