"""Assembled airway, split print opening and extraction without PCB/bezel."""
import adsk.core,adsk.fusion,json,math

def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V11' in app.activeDocument.name
 root=d.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
 shell_o=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
 inv=shell_o.transform2.copy();inv.invert();parts=[];ducts=[]
 for o in root.occurrences:
  if not o.component.name.startswith(('01 /','02 /','09 /','REF / Juntas')):continue
  for b in o.component.bRepBodies:
   t=mgr.copy(b.createForAssemblyContext(o));mgr.transform(t,inv)
   parts.append((o.component.name+'/'+b.name,t))
   if o.component.name.startswith('09 /'):ducts.append((b.name,t))
 shell=next(b for name,b in parts if name.startswith('02 /'));front=next(b for name,b in parts if name.startswith('01 /'))
 def intersection(a,b):
  t=mgr.copy(a);assert mgr.booleanOperation(t,b,adsk.fusion.BooleanTypes.IntersectionBooleanType);return t.volume
 def sphere(p,r):return mgr.createSphere(adsk.core.Point3D.create(*[v/10 for v in p]),r/10)
 obstructions=[];wall_failures=[];extraction=[];counts=0;walls=0;lengths=[];open_checks=0;opening_failures=[]
 for x,label in [(-68,'esquerdo'),(68,'direito')]:
  points=[[x,-53.5,.05],[x,-53.5,-3.43],[x,-52.45,-10.9],[x,-50,-14],[x,-45.399988462187,-14],[x,-45.399988462187,-10.55]]
  lengths.append({'mic':label,'length_mm':sum(math.dist(a,b) for a,b in zip(points,points[1:]))})
  for a,b in zip(points,points[1:]):
   n=max(2,math.ceil(math.dist(a,b)/.4))
   for i in range(n+1):
    p=[a[k]*(1-i/n)+b[k]*i/n for k in range(3)];ball=sphere(p,.3);counts+=1
    for name,body in parts:
     vol=intersection(ball,body)*1000
     if vol>.001:obstructions.append({'point':p,'part':name,'volume_mm3':vol})
  for a,b in zip(points[1:3],points[2:4]):
   for i in range(2,14):
    p=[a[k]*(1-i/16)+b[k]*i/16 for k in range(3)]
    if p[2]>-3.8:continue
    for delta in [(2.5,0,0),(-2.5,0,0),(0,1.35,0),(0,-1.35,0)]:
     q=[p[k]+delta[k] for k in range(3)]
     # End of elbow is also the deliberately open horizontal leg.
     if abs(q[0]-x)<1 and -50.1<q[1]<-45.2999 and abs(q[2]+14)<1:continue
     ball=sphere(q,.08);walls+=1
     fraction=sum(intersection(ball,body) for name,body in ducts)/ball.volume
     if fraction<.98:wall_failures.append({'point':q,'fraction':fraction})
  # Every duct airway sample touches its open split face before joining.
  for name,body in ducts:
   if label not in name:continue
   sign=-1 if ' A /' in name else 1
   for a,b in zip(points[1:4],points[2:5]):
    for i in range(1,8):
     p=[a[k]*(1-i/8)+b[k]*i/8 for k in range(3)];p[0]=x+sign*.15
     ball=sphere(p,.15);open_checks+=1
     if intersection(ball,body)*1000>.001:opening_failures.append({'part':name,'point':p})
   # Module can come out towards the front after screen and screws removed.
   poses=[(0,z) for z in [.2,.5,1,2,5,10,20]]
   for lift,distance in poses:
    t=mgr.copy(body);shift=adsk.core.Matrix3D.create();shift.translation=adsk.core.Vector3D.create(0,lift/10,distance/10);mgr.transform(t,shift)
    vol=intersection(t,shell)*1000
    if vol>.01:extraction.append({'part':name,'offset_mm':distance,'lift_y_mm':lift,'volume_mm3':vol})
 for x in [-68,76]:
  ball=sphere([x,-51,-1.5],.2);assert intersection(ball,front)/ball.volume>.98
 data={'airway_probes':counts,'airway_obstructions':obstructions,'wall_probes':walls,'wall_failures':wall_failures,'split_open_airway_probes':open_checks,'split_open_failures':opening_failures,'extraction_lift_y_offset_z_mm':poses,'extraction_collisions':extraction,'matched_path_lengths':lengths,'scope':'CAD samples, screen/bezel/screws removed for extraction; not acoustic or printer testing'}
 with open('D:/Projetos/NP2/mechanical/concept-v11/mic-channel-check.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print(json.dumps(data,ensure_ascii=False))
 assert not obstructions and not wall_failures and not opening_failures and not extraction
