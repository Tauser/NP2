"""Open U seats, remove unnecessary pin ears, trim supports inside rear rim."""
import adsk.core,adsk.fusion,importlib.util,json
spec=importlib.util.spec_from_file_location('mic','D:/Projetos/NP2/tools/cad/removable-mics-v11.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;OP=v.OP

def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
 assert 'Mesa V11' in app.activeDocument.name
 assert not d.attributes.itemByName('NP2','mic_removal_clear_v11')
 shell_o=next(o for o in root.occurrences if o.component.name.startswith('02 /'));shell=shell_o.component
 duct_o=next(o for o in root.occurrences if o.component.name.startswith('09 /'));duct=duct_o.component
 inverse=shell_o.transform2.copy();inverse.invert();mgr=adsk.fusion.TemporaryBRepManager.get()
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  m.cut_round(shell,'MIC '+label+' / assento U aberto para retirada',7.1,4.2,.1,x,-49.2,-3.02,-3.3)
  # No small register pins: screw passage and cartridge edges align halves.
  m.cut_round(duct,'MIC '+label+' / eliminar orelha e pino desnecessarios',7.2,4.4,.1,x,-45.4,-16.399,-3.1)
  box=mgr.createBox(adsk.core.OrientedBoundingBox3D.create(m.pt(x,61,8),adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),.8,.65,1.8));mgr.transform(box,inverse)
  bf=shell.features.baseFeatures.add();bf.name='MIC '+label+' / ferramenta limite traseiro'
  assert bf.startEdit();shell.bRepBodies.add(box,bf);bf.finishEdit();tool=bf.bodies.item(0)
  main=next(b for b in shell.bRepBodies if b!=tool)
  v.old.combine(shell,main,[tool],OP.CutFeatureOperation,'MIC '+label+' / apoio dentro da borda traseira')
 assert shell.bRepBodies.count==1 and duct.bRepBodies.count==4
 for o in [duct_o,shell_o]:
  o.component.isConstructionFolderLightBulbOn=False
  for s in o.component.sketches:s.isLightBulbOn=False
 if d.snapshots.hasPendingSnapshot:d.snapshots.add()
 data=json.loads(d.attributes.itemByName('NP2','removable_mics_v11').value)
 data['halves_joint']='no extra pins; align screw half-bores and perimeter, bond and seal away from PCB; cartridge not glued to case'
 data['extraction']='screen/bezel/screw removed; move +native Z 0.2 mm, +native Y 1 mm, then forward +Z'
 data['seats']='open U at inlet; planar positive screw stop on inclined support'
 d.attributes.add('NP2','removable_mics_v11',json.dumps(data));d.attributes.add('NP2','mic_removal_clear_v11','U seats; no pin ears; rear limit 57.75 mm')
 with open('D:/Projetos/NP2/mechanical/concept-v11/mic-design.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print('Open U seats, simpler unpinned halves; rear supports clipped inside case and cover')
