import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct);mgr=adsk.fusion.TemporaryBRepManager.get()
 c=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'));inv=c.transform2.copy();inv.invert()
 for o in d.rootComponent.allOccurrences:
  if not o.fullPathName.startswith('REF OFICIAL'):continue
  for b in o.component.bRepBodies:
   t=mgr.copy(b.createForAssemblyContext(o));mgr.transform(t,inv);bb=t.boundingBox
   if bb.maxPoint.y*10<-54.5 or bb.minPoint.y*10>-42:continue
   if bb.maxPoint.z*10<-15.5 or bb.minPoint.z*10>-2.9:continue
   if any(bb.maxPoint.x*10>x-4 and bb.minPoint.x*10<x+4 for x in [-68,68]):
    print('NEIGHBOR',o.fullPathName,b.name,[[v*10 for v in p.asArray()] for p in [bb.minPoint,bb.maxPoint]])
