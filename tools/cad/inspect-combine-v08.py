import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 f=c.features.combineFeatures.itemByName('Apoios e suporte volume integrados ao corpo')
 f.timelineObject.rollTo(True)
 try:
  print('TARGET',f.targetBody.name,'TOOLS',[(b.name,b.volume*1000) for b in f.toolBodies])
  for b in c.bRepBodies:print('BODY',b.name,b.volume*1000,[[x*10 for x in p.asArray()] for p in [b.boundingBox.minPoint,b.boundingBox.maxPoint]])
 finally:d.timeline.moveToEnd()
