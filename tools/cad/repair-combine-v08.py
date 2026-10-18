import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 f=c.features.combineFeatures.itemByName('Apoios e suporte volume integrados ao corpo')
 f.timelineObject.rollTo(True)
 bs=list(c.bRepBodies);main=max(bs,key=lambda b:b.volume)
 others=adsk.core.ObjectCollection.create()
 for b in bs:
  if b!=main:others.add(b)
 f.targetBody=main;f.toolBodies=others
 d.timeline.moveToEnd();d.computeAll()
 print('REPAIRED',f.healthState,f.errorOrWarningMessage)
 print('BODIES',[(b.name,b.volume*1000,b.lumps.count) for b in c.bRepBodies])
 for i in range(d.timeline.count):
  t=d.timeline.item(i)
  if t.errorOrWarningMessage:print('HEALTH',t.name,t.healthState,t.errorOrWarningMessage)
 for f in c.features.loftFeatures:
  print('LOFT',f.name,'HEALTH',f.healthState,'BOUNDS',[(b.name,[[v*10 for v in p.asArray()] for p in [b.boundingBox.minPoint,b.boundingBox.maxPoint]]) for b in f.bodies])
