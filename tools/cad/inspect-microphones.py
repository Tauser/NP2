import adsk.core,adsk.fusion,json
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 mgr=adsk.fusion.TemporaryBRepManager.get()
 shell=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 inv=shell.transform2.copy();inv.invert()
 print('DOCUMENT',app.activeDocument.name)
 for o in d.rootComponent.allOccurrences:
  if not o.fullPathName.startswith('REF OFICIAL'):continue
  if any(x in o.fullPathName.lower() for x in ['mic1_asm','mic2_asm']):
   print('COMP',o.fullPathName,'BODIES',o.component.bRepBodies.count)
   for b in o.component.bRepBodies:
    t=mgr.copy(b.createForAssemblyContext(o));mgr.transform(t,inv)
    print('BODY',b.name,'SOLID',t.isSolid,'VOLUME',t.volume*1000,'BBOX',[[v*10 for v in p.asArray()] for p in [t.boundingBox.minPoint,t.boundingBox.maxPoint]])
    for face in t.faces:
     g=face.geometry
     if g.objectType=='adsk::core::Cylinder':print('CYLINDER',g.origin.asArray(),g.axis.asArray(),g.radius*10)
    for edge in t.edges:
     g=edge.geometry
     if g.objectType=='adsk::core::Circle3D':print('CIRCLE',g.center.asArray(),g.normal.asArray(),g.radius*10)
 for o in d.rootComponent.occurrences:
  if o.component.name.startswith(('01 /','02 /')):
   for s in o.component.sketches:
    if 'MIC' in s.name:print('SKETCH',o.component.name,s.name,s.timelineObject.index,[[v*10 for v in p.asArray()] for p in [s.boundingBox.minPoint,s.boundingBox.maxPoint]])
