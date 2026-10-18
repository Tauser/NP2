"""Independent MIC cartridges. Both halves print on flat backs, channels up.

Native XY is the display plane, -Z rear. Split at native X = mic centre.
Case contains open seats and inclined supports, never the long airway.
One small screw holds each sealed cartridge; the speaker still uses clips.
"""
import adsk.core,adsk.fusion,importlib.util,json
spec=importlib.util.spec_from_file_location('mic10','D:/Projetos/NP2/tools/cad/mic-ducts-v10.py')
old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
m=old.m;c=old.c;OP=old.OP
MIC_Y=old.MIC_Y;FRONT_Y=old.FRONT_Y
STATIONS=[(-3.43,FRONT_Y),(-10.9,-52.45),(-14,-50)]
WIDTH=6.6
MOUNT_Y=-40.3

def section(comp,name,x,y,z,w,h):
 s=m.sketch(comp,z,name);m.rounded(s,w,h,.1,x,y);m.finish(s);s.isLightBulbOn=False;return s

def outside(comp,x,label,clearance=0):
 a=clearance
 stations=[(z+(a if i==0 else 0),y) for i,(z,y) in enumerate(STATIONS)]
 profiles=[section(comp,label+' / perfil '+str(i),x,y,z,WIDTH+2*a,(4.4 if i==0 else 3.2)+2*a) for i,(z,y) in enumerate(stations)]
 tools=[old.loft(comp,profiles[i],profiles[i+1],label+' / envoltoria '+str(i)) for i in range(2)]
 tools.append(c.top(comp,MIC_Y+a,label+' / curva aberta depois de dividir',x,-14,WIDTH+2*a,3.2+2*a,.1,MIC_Y+50+2*a,OP.NewBodyFeatureOperation).bodies.item(0))
 tools.append(m.slab(comp,label+' / camara',6.6+2*a,5.8+2*a,.3,x,MIC_Y,-11.1+a,-5.3-2*a))
 # Upper mounting ear avoids both the airway and the front seal.
 tools.append(m.slab(comp,label+' / aba fixacao',6.6+2*a,5.2+2*a,.3,x,-41.1,-13.1+a,-3.3-2*a))
 tools.append(c.side(comp,x-3.3-a,label+' / guia das metades',-45.4,-17.4,2.8+2*a,2.8+2*a,.3,6.6+2*a,OP.NewBodyFeatureOperation).bodies.item(0))
 return old.combine(comp,tools[0],tools[1:],OP.JoinFeatureOperation,label+' / solido unico')

def floor_support(comp,x,transform):
 # +Z face is a cartridge stop. The other end follows the horizontal floor
 # at world Z=2.3 (0.1 mm overlap); surfaces grow in print from rear to front.
 points=[]
 for y,z in [(-43.7,-16.4),(-38.5,-16.4)]:points.append((y,z))
 a=transform.getCell(2,1);b=transform.getCell(2,2);t=transform.getCell(2,3)*10
 for y in [-38.5,-43.7]:points.append((y,(2.3-t-a*y)/b))
 s=c.oriented(comp,comp.yZConstructionPlane,x-3.3,'MIC / apoio inclinado / piso ao batente')
 for i,(y,z) in enumerate(points):
  yy,zz=points[(i+1)%len(points)]
  s.sketchCurves.sketchLines.addByTwoPoints(s.modelToSketchSpace(m.pt(x-3.3,y,z)),s.modelToSketchSpace(m.pt(x-3.3,yy,zz)))
 return m.extrude(comp,s,6.6,OP.NewBodyFeatureOperation,'MIC / apoio sem prateleira suspensa').bodies.item(0)

def add_halves(comp,whole,x,label):
 mgr=adsk.fusion.TemporaryBRepManager.get();halves=[]
 for letter,sign in [('A',-1),('B',1)]:
  # Flat constant-X outside face is the print bed; the split face opens up.
  clip=mgr.createBox(adsk.core.OrientedBoundingBox3D.create(m.pt(x+sign*5,-47,-12),adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),1,4,4))
  temp=mgr.copy(whole);assert mgr.booleanOperation(temp,clip,adsk.fusion.BooleanTypes.IntersectionBooleanType)
  assert temp.isSolid and temp.lumps.count==1
  bf=comp.features.baseFeatures.add();bf.name='MIC '+label+' '+letter+' / metade aberta'
  assert bf.startEdit();body=comp.bRepBodies.add(temp,bf);bf.finishEdit()
  body=bf.bodies.item(0);body.name='MIC '+label+' '+letter+' / canal aberto';halves.append(body)
 comp.features.removeFeatures.add(whole).name='MIC '+label+' / substituir cartucho por metades'
 # B receives a blind socket, opening towards the joint / print +Z.
 s=c.oriented(comp,comp.yZConstructionPlane,x-.01,'MIC '+label+' / guia diam 2.1')
 s.sketchCurves.sketchCircles.addByCenterRadius(s.modelToSketchSpace(m.pt(x,-45.4,-17.4)),.105)
 m.finish(s);inp=comp.features.extrudeFeatures.createInput(s.profiles.item(0),OP.CutFeatureOperation)
 inp.setDistanceExtent(False,m.V.createByString('1.3 mm'));inp.participantBodies=[halves[1]]
 comp.features.extrudeFeatures.add(inp).name='MIC '+label+' B / encaixe guia';s.isLightBulbOn=False
 s=c.oriented(comp,comp.yZConstructionPlane,x-.02,'MIC '+label+' / pino diam 1.8')
 s.sketchCurves.sketchCircles.addByCenterRadius(s.modelToSketchSpace(m.pt(x,-45.4,-17.4)),.09)
 pin=m.extrude(comp,s,1.02,OP.NewBodyFeatureOperation,'MIC '+label+' A / pino de alinhamento').bodies.item(0)
 halves[0]=old.combine(comp,halves[0],[pin],OP.JoinFeatureOperation,'MIC '+label+' A / guia incorporada')
 halves[0].name='MIC '+label+' A / canal aberto';halves[1].name='MIC '+label+' B / canal aberto'
 return halves

def gaskets(root,d,transform):
 o,g=m.component(root,'REF / Juntas acusticas / espuma macia / nao imprimir ABS',transform)
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  s=m.sketch(g,-9.45,'Junta MIC '+label+' / 2 livre e 1.65 comprimida')
  m.rounded(s,6.6,5.8,.6,x,MIC_Y);m.rounded(s,4.2,3.4,.4,x,MIC_Y);m.finish(s)
  ring=next(p for p in s.profiles if p.profileLoops.count==2)
  f=g.features.extrudeFeatures.addSimple(ring,m.V.createByString('-1.65 mm'),OP.NewBodyFeatureOperation)
  f.bodies.item(0).name='Junta traseira '+label;s.isLightBulbOn=False
  s=m.sketch(g,-3,'Junta frontal '+label+' / .5 livre e .43 comprimida')
  for diam in [4,2]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(x,FRONT_Y),diam/20)
  m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
  f=g.features.extrudeFeatures.addSimple(ring,m.V.createByString('-.43 mm'),OP.NewBodyFeatureOperation)
  f.bodies.item(0).name='Junta frontal '+label;s.isLightBulbOn=False
  
 for b in g.bRepBodies:b.appearance=d.appearances.itemByName('NP2 teclas')
 return o

def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
 assert 'Mesa V11' in app.activeDocument.name
 assert not d.attributes.itemByName('NP2','removable_mics_v11')
 shell_o=next(o for o in root.occurrences if o.component.name.startswith('02 /'));shell=shell_o.component
 transform=shell_o.transform2.copy();paint=shell.bRepBodies.item(0).appearance
 # Use the V09 4 mm height; remove only the two unwanted header recesses.
 for label in ['esquerdo','direito']:
  f=shell.features.combineFeatures.itemByName('Alojamento raso plug interno '+label+' / exterior preservado');assert f and f.deleteMe()
  f=shell.features.baseFeatures.itemByName('Ferramenta rebaixo interno '+label+' / piso 1.2 mm');assert f and f.deleteMe()
 front=next(o.component for o in root.occurrences if o.component.name.startswith('01 /'))
 m.circles(front,'MIC / fechar furos antigos',[(x,-51,1.65) for x in [-68,76]],0,-3,OP.JoinFeatureOperation)
 for x,w,label in [(-75.4,20.8,'esquerdo'),(77.5,9.4,'direito')]:
  m.cut_round(shell,'MIC / remover bocal antigo '+label,w,4.2,.2,x,-51,-3.02,-3.2)
 m.circles(front,'MIC / portas X -68 e +68 diam 1.6',[(x,FRONT_Y,1.6) for x in [-68,68]],.1,-3.3,OP.CutFeatureOperation)
 assert shell.bRepBodies.count==1
 main=shell.bRepBodies.item(0)
 duct_o,duct=m.component(root,'09 / Dutos MIC removiveis / 4 metades para ABS',transform)
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  # Shallow inlet socket and an inclined screw support stay in the case.
  socket=m.slab(shell,'MIC '+label+' / assento frontal aberto',8,5.2,.2,x,FRONT_Y,-3.03,-2.9)
  ramp=floor_support(shell,x,transform)
  main=old.combine(shell,main,[socket,ramp],OP.JoinFeatureOperation,'MIC '+label+' / assentos unidos ao corpo')
  clearance=outside(shell,x,'MIC '+label+' / ferramenta folga',.2)
  main=old.combine(shell,main,[clearance],OP.CutFeatureOperation,'MIC '+label+' / assento aberto com folga .2')
  # Add back a flat positive stop: clearance tool went 0.2 mm past its plane.
  pad=m.slab(shell,'MIC '+label+' / face do batente',6.6,5.2,.3,x,-41.1,-16.4,-.4)
  main=old.combine(shell,main,[pad],OP.JoinFeatureOperation,'MIC '+label+' / batente sem folga axial')
  m.circles(shell,'MIC '+label+' / piloto parafuso pequeno',[(x,MOUNT_Y,1.7)],-16.39,-4.9,OP.CutFeatureOperation)
  m.circles(shell,'MIC '+label+' / assento espuma frontal',[(x,FRONT_Y,4.1)],-3.02,-.63,OP.CutFeatureOperation)
  main=shell.bRepBodies.item(0)
  whole=outside(duct,x,'MIC '+label+' / cartucho')
  profiles=[old.disk(duct,'MIC '+label+' / canal '+str(i),x,y,z,2) for i,(z,y) in enumerate(STATIONS)]
  for i in range(2):
   tool=old.loft(duct,profiles[i],profiles[i+1],'MIC '+label+' / ferramenta canal '+str(i))
   whole=old.combine(duct,whole,[tool],OP.CutFeatureOperation,'MIC '+label+' / canal '+str(i))
  c.top(duct,MIC_Y+.1,'MIC '+label+' / canal curva',x,-14,2,2,.7,MIC_Y+50+.2,OP.CutFeatureOperation)
  m.cut_round(duct,'MIC '+label+' / camara capsule',4.2,3.4,.4,x,MIC_Y,-11,-4.2)
  m.circles(duct,'MIC '+label+' / passagem parafuso 2.4',[(x,MOUNT_Y,2.4)],-13,-3.5,OP.CutFeatureOperation)
  whole=next(b for b in duct.bRepBodies if b.boundingBox.minPoint.x*10<x<b.boundingBox.maxPoint.x*10)
  halves=add_halves(duct,whole,x,label)
  for b in halves:b.appearance=d.appearances.itemByName('NP2 grafite traseiro')
 assert duct.bRepBodies.count==4 and shell.bRepBodies.count==1
 shell.bRepBodies.item(0).appearance=paint
 foam_o=gaskets(root,d,transform)
 # Conservative screw heads only: the shaft intentionally engages the pilot.
 screw_o,screws=m.component(root,'REF / Parafusos dutos / 2 x diam 2 x 8 / nao imprimir',transform)
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  m.circles(screws,'Cabeca parafuso MIC '+label,[(x,MOUNT_Y,4)],-11.1,-2,OP.NewBodyFeatureOperation)
 for o in [duct_o,foam_o,screw_o]:o.transform2=transform.copy()
 for o in root.occurrences:
  o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
  for s in o.component.sketches:s.isLightBulbOn=False
 if d.snapshots.hasPendingSnapshot:d.snapshots.add()
 data={'removable':True,'printed_duct_parts':4,'modules':2,'split_axis':'native X at -68 and +68 mm','print_orientation':'constant-X outer face down; split face and airway up','mounting':'one screw per cartridge, approximately diameter 2 x 8 mm; pilot 1.7 mm, clearance 2.4 mm; select physical screw and prototype thread','halves_joint':'1.8 mm guide pin, 2.1 mm blind socket; bond and seal halves away from PCB; do not glue cartridge to case','case_clearance_mm':.2,'body_airway_integrated':False,'floor_nominal_mm':2.4,'header_reliefs_removed':True,'bottom_extension_mm':4,'channel_stations_yz_mm':[[y,z] for z,y in STATIONS],'front_ports_xy_mm':[[-68,FRONT_Y],[68,FRONT_Y]],'front_diameter_mm':1.6,'channel_diameter_mm':2,'gaskets_required':True,'rear_gasket_free_compressed_mm':[2,1.65],'front_gasket_free_compressed_mm':[.5,.43],'local_wall_at_glass_bypass_mm':.6,'terminal_wall_minimum_mm':1.2,'hardware_inlet':'official STEP has only capsule envelope; physically verify inlet','acoustic_performance':'not measured'}
 d.attributes.add('NP2','removable_mics_v11',json.dumps(data))
 with open('D:/Projetos/NP2/mechanical/concept-v11/mic-design.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print('V11: open seats in case, four flat backed duct halves, two removable screw-mounted cartridges')
