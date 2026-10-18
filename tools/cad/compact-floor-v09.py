import adsk.core,adsk.fusion,math,json
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V09' in app.activeDocument.name
 if d.attributes.itemByName('NP2','compact_floor_v09'):return
 path='D:/Projetos/NP2/tools/cad/lower-floor-v08.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('lowered_v08','reduced_v09')
 ns={};exec(compile(source,path,'exec'),ns);move=ns['lower_outline']
 root=d.rootComponent
 shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
 delta=-4/math.cos(math.radians(18))
 for name,amount in [('Contorno frontal do corpo / R6',delta),('Contorno traseiro menor / R6',-4),('Exterior continuo / acabamento 0.02 mm',delta),('Traseira exterior continua',-4),('Acabamento exterior do painel rebaixado',delta),('Traseira de acabamento do painel',-4)]:
  move(d,shell.sketches.itemByName(name),amount)
 front=next(o.component for o in root.occurrences if o.component.name.startswith('01 /'))
 move(d,front.sketches.itemByName('Moldura alinhada ao corpo / sem orelhas laterais / perfil'),delta)
 cap=next(o.component for o in root.occurrences if o.component.name.startswith('03 /'))
 move(d,cap.sketches.itemByName('Tampa plana / face externa na mesa'),-4)
 for o in root.occurrences:
  t=o.transform2.copy();p=t.translation;p.z-=.4;t.translation=p;o.transform2=t
 if d.snapshots.hasPendingSnapshot:d.snapshots.add()
 d.attributes.add('NP2','compact_floor_v09',json.dumps({'additional_internal_height_mm':4,'reduced_from_mm':8}))
 print('Floor extension reduced from 8 to 4 mm; bezel and rear cover shortened together')
