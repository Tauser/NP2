"""Shallow internal pockets preserve 1.2 mm floor under reference plugs."""
import adsk.core,adsk.fusion,json
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V09' in app.activeDocument.name
 if d.attributes.itemByName('NP2','internal_relief_v09'):return
 o=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 c=o.component;mgr=adsk.fusion.TemporaryBRepManager.get()
 inverse=o.transform2.copy();inverse.invert()
 assert c.bRepBodies.count==1
 for a,b,label in [(22,56,'direito'),(-56,-22,'esquerdo')]:
  center=adsk.core.Point3D.create((a+b)/20,1.5,.26)
  ob=adsk.core.OrientedBoundingBox3D.create(center,adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),(b-a)/10,1.6,.28)
  temp=mgr.createBox(ob);assert mgr.transform(temp,inverse)
  main=c.bRepBodies.item(0)
  bf=c.features.baseFeatures.add();bf.name='Ferramenta rebaixo interno '+label+' / piso 1.2 mm'
  assert bf.startEdit();tool=c.bRepBodies.add(temp,bf);assert bf.finishEdit()
  tool=bf.bodies.item(0)
  tools=adsk.core.ObjectCollection.create();tools.add(tool)
  ci=c.features.combineFeatures.createInput(main,tools);ci.operation=adsk.fusion.FeatureOperations.CutFeatureOperation
  f=c.features.combineFeatures.add(ci);f.name='Alojamento raso plug interno '+label+' / exterior preservado'
 assert c.bRepBodies.count==1 and c.bRepBodies.item(0).lumps.count==1
 d.attributes.add('NP2','internal_relief_v09',json.dumps({'floor_under_plugs_mm':1.2,'pocket_xy_mm':[34,16],'wire_route':'upward into case; no downward bend below plugs'}))
 print('Two shallow internal pockets; minimum floor 1.2 mm; exterior outline unchanged')
