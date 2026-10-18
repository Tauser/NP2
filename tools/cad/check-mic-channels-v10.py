"""Sample the intended airway and wall continuity, including foam references."""
import adsk.core,adsk.fusion,json,math
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V10' in app.activeDocument.name
 root=d.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
 o=next(o for o in root.occurrences if o.component.name.startswith('02 /'));inv=o.transform2.copy();inv.invert()
 parts=[]
 for occ in root.occurrences:
  if not occ.component.name.startswith(('01 /','02 /','REF / Juntas')):continue
  for b in occ.component.bRepBodies:
   temp=mgr.copy(b.createForAssemblyContext(occ));mgr.transform(temp,inv)
   parts.append((occ.component.name+'/'+b.name,temp))
 shell=next(b for name,b in parts if name.startswith('02 /'))
 front=next(b for name,b in parts if name.startswith('01 /'))
 count=0;wall_count=0;junction_skips=0;obstructions=[];wall_failures=[];lengths=[]
 def sphere(point,r):return mgr.createSphere(adsk.core.Point3D.create(*[v/10 for v in point]),r/10)
 def intersection(a,b):
  t=mgr.copy(a);assert mgr.booleanOperation(t,b,adsk.fusion.BooleanTypes.IntersectionBooleanType)
  return t.volume
 def airway(label,p):
  nonlocal count
  ball=sphere(p,.35);count+=1
  for name,b in parts:
   vol=intersection(ball,b)*1000
   if vol>.001:obstructions.append({'duct':label,'point_mm':p,'part':name,'volume_mm3':vol})
 def wall(label,p):
  nonlocal wall_count,junction_skips
  # At a bend, the other leg's designed lumen is an opening, not a wall.
  x=-68 if label=='left' else 68
  bar_open=abs(p[0]-x)<1 and -50.1<p[1]<-45.299988462187 and abs(p[2]+14)<1
  chamber_open=abs(p[0]-x)<2.1 and abs(p[1]+45.399988462187)<1.7 and -15.2<p[2]<-9.45
  if bar_open or chamber_open:
   junction_skips+=1;return
  ball=sphere(p,.1);wall_count+=1
  fraction=intersection(ball,shell)/ball.volume
  if fraction<.98:wall_failures.append({'duct':label,'point_mm':p,'fraction_in_solid':fraction})
 for x,label in [(-68,'left'),(68,'right')]:
  pts=[[x,-53.5,.05],[x,-53.5,-3.03],[x,-53.15,-10.9],[x,-50,-14],[x,-45.399988462187,-14],[x,-45.399988462187,-10.55]]
  length=0
  for a,b in zip(pts,pts[1:]):
   seg=math.dist(a,b);length+=seg
   for i in range(max(2,math.ceil(seg/.4))+1):
    t=i/max(2,math.ceil(seg/.4));p=[a[k]*(1-t)+b[k]*t for k in range(3)];airway(label,p)
  lengths.append({'duct':label,'length_mm':length})
  for a,b in zip(pts[1:3],pts[2:4]):
   for i in range(1,16):
    t=i/16;p=[a[k]*(1-t)+b[k]*t for k in range(3)]
    if p[2]>-3.65:continue
    for sign in [-1,1]:
     wall(label,[x+sign*1.6,p[1],p[2]])
     wall(label,[x,p[1]+sign*1.6,p[2]])
  for y in [-49.6,-49,-48.4,-47.8,-47.4]:
   for sign in [-1,1]:
    wall(label,[x+sign*1.6,y,-14]);wall(label,[x,y,-14+sign*1.6])
  for sign in [-1,1]:
   wall(label,[x+sign*2.7,-45.399988462187,-13.5])
   wall(label,[x,-45.399988462187+sign*2.3,-13.5])
  wall(label,[x,-45.399988462187,-15.8])
 for x in [-68,76]:
  ball=sphere([x,-51,-1.5],.2)
  assert intersection(ball,front)/ball.volume>.98,'Obsolete front hole remains open'
 report={'method':'0.35 mm radius airway probes every <=0.4 mm; 0.1 mm wall probes outside deliberately intersecting passage junctions; case and compressed gasket references only','airway_probes':count,'wall_probes':wall_count,'designed_junction_openings_skipped':junction_skips,'airway_obstructions':obstructions,'wall_probe_failures':wall_failures,'matched_path_lengths':lengths,'old_front_ports_closed':True,'scope':'Geometric continuity, not acoustic response or exact MEMS port validation'}
 with open('D:/Projetos/NP2/mechanical/concept-v10/mic-channel-check.json','w',encoding='utf8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
 print(json.dumps(report,ensure_ascii=False))
 assert not obstructions and not wall_failures,'Repair channel or wall continuity'
 assert abs(lengths[0]['length_mm']-lengths[1]['length_mm'])<.001
