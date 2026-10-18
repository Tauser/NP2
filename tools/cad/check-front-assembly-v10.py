"""Sample front insertion of major rigid hardware envelopes; bezel removed."""
import adsk.core,adsk.fusion,json
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);assert 'Mesa V10' in app.activeDocument.name
 mgr=adsk.fusion.TemporaryBRepManager.get();root=d.rootComponent
 shell_o=next(o for o in root.occurrences if o.component.name.startswith('02 /'));inv=shell_o.transform2.copy();inv.invert()
 shell=mgr.copy(shell_o.component.bRepBodies.item(0));hardware=[]
 for o in root.allOccurrences:
  if not o.fullPathName.startswith('REF OFICIAL'):continue
  major=any(k in o.fullPathName for k in ['+BOARD:','+7INCH-1024X600-TP:','+7INCH-1024X600-JHD','+MIC1_ASM:','+MIC2_ASM:','+SMTSO-M2_5'])
  if not major:continue
  for b in o.component.bRepBodies:
   t=mgr.copy(b.createForAssemblyContext(o));mgr.transform(t,inv);hardware.append((o.fullPathName,t))
 assert len(hardware)==9,'Expected PCB, two LCD bodies, two mics and four standoffs'
 collisions=[];checks=0
 for distance in [.5,2,5,10,20]:
  shift=adsk.core.Matrix3D.create();shift.translation=adsk.core.Vector3D.create(0,0,distance/10)
  for name,b in hardware:
   t=mgr.copy(b);mgr.transform(t,shift)
   assert mgr.booleanOperation(t,shell,adsk.fusion.BooleanTypes.IntersectionBooleanType)
   checks+=1
   if t.volume*1000>.01:collisions.append({'front_offset_mm':distance,'part':name,'volume_mm3':t.volume*1000})
 result={'method':'sampled translation along +Z in panel coordinates with bezel removed; major PCB, LCD, mic and standoff solids only','offsets_mm':[.5,2,5,10,20],'hardware_bodies':len(hardware),'checks':checks,'collisions':collisions,'scope':'not a complete tolerance/hand assembly or continuous-motion test'}
 with open('D:/Projetos/NP2/mechanical/concept-v10/front-assembly-check.json','w',encoding='utf8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
 print(json.dumps(result,ensure_ascii=False));assert not collisions,'Front insertion of major hardware is blocked'
