import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 s=c.sketches.itemByName('Contorno frontal do corpo / R6')
 print('PROFILES',s.profiles.count,'BOUNDS',s.profiles.item(0).boundingBox.minPoint.y*10)
 for curve in s.sketchCurves:
  g=curve.geometry
  print('CURVE',g.objectType,[x*10 for x in g.startPoint.asArray()],[x*10 for x in g.endPoint.asArray()],getattr(g,'radius',0)*10)
 print('LOWER ATTRIBUTE',d.attributes.itemByName('NP2','lower_floor_v08').value)
