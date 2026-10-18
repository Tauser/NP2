import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 for s in c.sketches:
  if s.attributes.itemByName('NP2','lowered_v08'):
   print('SKETCH',s.name,[[v*10 for v in p.asArray()] for p in [s.profiles.item(0).boundingBox.minPoint,s.profiles.item(0).boundingBox.maxPoint]])
 for f in c.features:
  print('FEATURE',f.timelineObject.index,f.name)
 try:
  for f in [c.features.loftFeatures.item(0),c.features.shellFeatures.item(0)]+list(c.features.combineFeatures):
   f.timelineObject.rollTo(False)
   print('STAGE',f.name,[(b.name,b.volume*1000,b.boundingBox.minPoint.y*10) for b in c.bRepBodies])
 finally:d.timeline.moveToEnd()
