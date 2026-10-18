import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 s=c.sketches.itemByName('Contorno frontal do corpo / R6')
 s.timelineObject.rollTo(False)
 target=-56.41169779390614/10
 for curve in s.sketchCurves.sketchLines:
  g=curve.geometry
  if abs(g.startPoint.x-g.endPoint.x)<1e-7:
   curve.isFixed=False
   p=curve.startSketchPoint if g.startPoint.y<g.endPoint.y else curve.endSketchPoint
   assert p.move(adsk.core.Vector3D.create(0,target-p.geometry.y,0))
   curve.isFixed=True
 assert s.profiles.count==1
 d.timeline.moveToEnd()
 print('Lower side lines terminate at R6 tangencies; closed outline preserved')
