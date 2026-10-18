import adsk.core,adsk.fusion,importlib.util,math
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 s=c.sketches.itemByName('Contorno frontal do corpo / R6')
 assert abs(s.profiles.item(0).boundingBox.minPoint.y*10+54)<.001
 s.attributes.itemByName('NP2','lowered_v08').deleteMe()
 path='D:/Projetos/NP2/tools/cad/lower-floor-v08.py'
 spec=importlib.util.spec_from_file_location('lower',path);v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
 v.lower_outline(d,s,8/math.cos(math.radians(18)))
 d.computeAll()
 print('FRONT PROFILE',s.profiles.item(0).boundingBox.minPoint.y*10)
 print('SHELL',[(b.name,b.boundingBox.minPoint.y*10,b.volume*1000,b.lumps.count) for b in c.bRepBodies])
 for i in range(d.timeline.count):
  t=d.timeline.item(i)
  if t.errorOrWarningMessage:print('HEALTH',t.name,t.errorOrWarningMessage)
