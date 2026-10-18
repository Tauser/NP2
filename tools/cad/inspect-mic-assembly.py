import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 for o in d.rootComponent.occurrences:
  print('OCC',o.component.name,o.transform2.asArray())
  if o.component.name.startswith(('01 /','02 /','REF / Juntas')):
   print('BODIES',[(b.name,b.lumps.count,b.volume*1000) for b in o.component.bRepBodies])
