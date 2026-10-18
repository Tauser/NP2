"""Two isolated acoustic paths around the lower glass/PCB edge.

Dimensions use the official mic envelopes, not an assumed tiny port centre.
The terminal chamber surrounds each complete mic; a soft ring seals to PCB.
"""
import adsk.core,adsk.fusion,importlib.util,json
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP
FRONT_Y=-53.5
MIC_Y=-45.399988462187
STATIONS=[(-3.03,FRONT_Y),(-10.9,-53.15),(-14,-50)]

def disk(comp,name,x,y,z,diam):
 s=m.sketch(comp,z,name)
 s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(x,y),diam/20)
 m.finish(s);s.isLightBulbOn=False
 return s

def loft(comp,a,b,name):
 ci=comp.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
 ci.loftSections.add(a.profiles.item(0));ci.loftSections.add(b.profiles.item(0))
 f=comp.features.loftFeatures.add(ci);f.name=name
 return f.bodies.item(0)

def combine(comp,target,tools,operation,name):
 objects=adsk.core.ObjectCollection.create()
 for tool in tools:objects.add(tool)
 ci=comp.features.combineFeatures.createInput(target,objects);ci.operation=operation
 f=comp.features.combineFeatures.add(ci);f.name=name
 return f.bodies.item(0)

def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V10' in app.activeDocument.name
 if d.attributes.itemByName('NP2','mic_ducts_v10'):return
 root=d.rootComponent
 front=next(o.component for o in root.occurrences if o.component.name.startswith('01 /'))
 shell_o=next(o for o in root.occurrences if o.component.name.startswith('02 /'));shell=shell_o.component
 assert front.bRepBodies.count==1 and shell.bRepBodies.count==1
 paint=shell.bRepBodies.item(0).appearance
 # Close obsolete visible ports, then clear the old short nipple/side beams.
 m.circles(front,'MIC / fechar furos antigos desalinhados',[(x,-51,1.65) for x in [-68,76]],0,-3,OP.JoinFeatureOperation)
 for x,w,label in [(-75.4,20.8,'esquerdo'),(77.5,9.4,'direito')]:
  m.cut_round(shell,'MIC / remover bocal antigo '+label,w,4.2,.2,x,-51,-3.02,-3.2)
 m.circles(front,'MIC / portas alinhadas X -68 e +68 / diam 1.6',[(x,FRONT_Y,1.6) for x in [-68,68]],.1,-3.3,OP.CutFeatureOperation)
 # Build outside envelopes as separate solids to avoid joining imported HW.
 main=shell.bRepBodies.item(0)
 outside=[]
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  profiles=[disk(shell,'Duto MIC '+label+' / secao externa '+str(i),x,y,z,4.8) for i,(z,y) in enumerate(STATIONS)]
  for i in range(2):outside.append(loft(shell,profiles[i],profiles[i+1],'Duto MIC '+label+' / envoltoria '+str(i)))
  f=c.top(shell,MIC_Y,'Duto MIC '+label+' / curva ate microfone',x,-14,4.8,4.8,1.2,MIC_Y+50,OP.NewBodyFeatureOperation);outside.append(f.bodies.item(0))
  outside.append(m.slab(shell,'Duto MIC '+label+' / camara sobre capsule',6.6,5.8,.6,x,MIC_Y,-11.1,-5.3))
 main=combine(shell,main,outside,OP.JoinFeatureOperation,'MIC / dois dutos incorporados ao corpo')
 assert shell.bRepBodies.count==1 and main.lumps.count==1
 # Cut a continuous inlet, elbow and microphone chamber into each duct.
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  positions=[(-3.02,FRONT_Y),STATIONS[1],STATIONS[2]]
  profiles=[disk(shell,'Duto MIC '+label+' / secao canal '+str(i),x,y,z,2) for i,(z,y) in enumerate(positions)]
  for i in range(2):
   tool=loft(shell,profiles[i],profiles[i+1],'Duto MIC '+label+' / ferramenta canal '+str(i))
   main=combine(shell,main,[tool],OP.CutFeatureOperation,'Duto MIC '+label+' / passagem continua '+str(i))
  c.top(shell,MIC_Y+.1,'Duto MIC '+label+' / canal da curva 2 x 2',x,-14,2,2,.7,MIC_Y+50+.2,OP.CutFeatureOperation)
  m.cut_round(shell,'Duto MIC '+label+' / camara aberta a junta',4.2,3.4,.4,x,MIC_Y,-11,-4.2)
  m.circles(shell,'Duto MIC '+label+' / assento junta frontal',[(x,FRONT_Y,4.1)],-3.02,-.41,OP.CutFeatureOperation)
 assert shell.bRepBodies.count==1 and shell.bRepBodies.item(0).lumps.count==1
 shell.bRepBodies.item(0).appearance=paint
 # Compressed foam references: not ABS printing pieces.
 o,g=m.component(root,'REF / Juntas acusticas / espuma macia / nao imprimir ABS',shell_o.transform2)
 o.transform2=shell_o.transform2
 foam=d.appearances.itemByName('NP2 teclas')
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  s=m.sketch(g,-9.45,'Junta MIC '+label+' / 2 mm livre e 1.65 comprimida')
  m.rounded(s,6.6,5.8,.6,x,MIC_Y);m.rounded(s,4.2,3.4,.4,x,MIC_Y);m.finish(s)
  ring=next(p for p in s.profiles if p.profileLoops.count==2)
  f=g.features.extrudeFeatures.addSimple(ring,m.V.createByString('-1.65 mm'),OP.NewBodyFeatureOperation)
  f.name='Junta traseira MIC '+label;f.bodies.item(0).name=f.name;s.isLightBulbOn=False
  s=m.sketch(g,-3,'Junta entrada '+label+' / 0.5 mm livre e 0.43 comprimida')
  for diam in [4,2]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(x,FRONT_Y),diam/20)
  m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
  f=g.features.extrudeFeatures.addSimple(ring,m.V.createByString('-.43 mm'),OP.NewBodyFeatureOperation)
  f.name='Junta frontal MIC '+label;f.bodies.item(0).name=f.name;s.isLightBulbOn=False
 for b in g.bRepBodies:
  if foam:b.appearance=foam
 # Capture placement after feature creation; Fusion feature edits can roll
 # back an uncaptured occurrence transform.
 o.transform2=shell_o.transform2.copy()
 for o2 in root.occurrences:
  o2.component.isConstructionFolderLightBulbOn=False
  for s in o2.component.sketches:s.isLightBulbOn=False
 if d.snapshots.hasPendingSnapshot:d.snapshots.add()
 data={'front_ports_xy_mm':[[-68,FRONT_Y],[68,FRONT_Y]],'old_right_x_mm':76,'front_diameter_mm':1.6,'channel_diameter_mm':2,'channel_stations_yz_mm':[[y,z] for z,y in STATIONS],'terminal_center_xy_mm':[[-68,MIC_Y],[68,MIC_Y]],'mic_envelope_mm':[3.4,2.6,.998],'terminal_inner_xy_mm':[4.2,3.4],'rear_gasket_outer_inner_xy_mm':[[6.6,5.8],[4.2,3.4]],'rear_gasket_free_compressed_mm':[2,1.65],'front_gasket_outer_inner_diameter_mm':[4,2],'front_gasket_free_compressed_mm':[.5,.43],'gaskets_required':True,'port_inlet_not_detailed_in_STEP':'chamber encloses full mic; verify actual PCB inlet before fabrication','acoustic_measurement':'pending; no gain or isolation performance asserted'}
 d.attributes.add('NP2','mic_ducts_v10',json.dumps(data))
 with open('D:/Projetos/NP2/mechanical/concept-v10/mic-design.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print('Two independent continuous MIC ducts; right front X corrected 76 -> 68; soft gaskets modelled')
