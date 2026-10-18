"""Volume caps and sliding actuator aligned approximately to original ON/OFF."""
import adsk.core, adsk.fusion, math, sys
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
from importlib import util
spec=util.spec_from_file_location('np2case','D:/Projetos/NP2/tools/cad/build-case.py')
m=util.module_from_spec(spec);spec.loader.exec_module(m)
P=m.P;V=m.V;OP=m.OP

def oriented(comp,base,offset,name):
    pi=comp.constructionPlanes.createInput()
    pi.setByOffset(base,V.createByString(f'{offset} mm'))
    p=comp.constructionPlanes.add(pi);p.isLightBulbOn=False
    s=comp.sketches.add(p);s.name=name;s.isComputeDeferred=True
    return s

def outline_at(s,point,w,h,r):
    c=s.modelToSketchSpace(m.pt(*point))
    m.rounded(s,w,h,r,c.x*10,c.y*10)

def top(comp,y,name,x,z,w,h,r,depth,op):
    s=oriented(comp,comp.xZConstructionPlane,y,name+' / perfil')
    outline_at(s,(x,y,z),w,h,r)
    return m.extrude(comp,s,-depth,op,name)

def side(comp,x,name,y,z,w,h,r,depth,op):
    s=oriented(comp,comp.yZConstructionPlane,x,name+' / perfil')
    # Fusion YZ sketches use local -Z as their X axis and local Y as Y axis.
    outline_at(s,(x,y,z),h,w,r)
    return m.extrude(comp,s,depth,op,name)

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V02' in app.activeDocument.name
    shell_occ=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
    shell=shell_occ.component;transform=shell_occ.transform2
    mount=next(o.component for o in root.occurrences if o.component.name.startswith('04 /'))
    assert not any(o.component.name.startswith('08 /') for o in root.occurrences),'Controls already created'
    paint=d.appearances.itemByName('NP2 / cinza claro botoes')
    dark=d.appearances.itemByName('NP2 / grafite fosco')
    for name,expr,comment in [('volume_travel_ref','0.6 mm','Curso previsto; escolher tact switch'),
                              ('switch_y_ref','47.82 mm','58.5 - 10.68; referencia foto'),
                              ('onoff_slide_ref','3 mm','Curso provisório; medir chave original')]:
        d.userParameters.add(name,V.createByString(expr),'mm',comment)
    # XZ plane normal points -Y: positive extrusion passes down through top wall.
    for x,label in [(39,'VOL -'),(57,'VOL +')]:
        top(shell,72.1,'Janela superior '+label,x,-18,9.6,7.6,3.6,2.7,OP.CutFeatureOperation)
    occ,caps=m.component(root,'08 / Botoes volume + e - / curso provisório',transform)
    for x,label in [(39,'Volume menos'),(57,'Volume mais')]:
        f=top(caps,73.4,label+' / tecla',x,-18,9,7,3.4,3.1,OP.NewBodyFeatureOperation)
        body=f.bodies.item(0);body.name=label;body.appearance=paint
        top(caps,69.9,label+' / flange retencao',x,-18,12,10,2,1.2,OP.JoinFeatureOperation)
        top(caps,69,label+' / haste',x,-18,3,3,.5,5.4,OP.JoinFeatureOperation)
        # Engraved signs on the cap, cut as separate features.
        top(caps,73.5,label+' / sinal menos',x,-18,3.5,.6,.25,.35,OP.CutFeatureOperation)
        if x==57:
            top(caps,73.5,label+' / sinal mais',x,-18,.6,3.5,.25,.35,OP.CutFeatureOperation)
    occ.transform2=transform
    occ,bracket=m.component(root,'09 / Suporte 2 tact switches / eletrica pendente',transform)
    # Holder underneath the keys, separate from PCB and screen.
    f=top(bracket,62.8,'Plataforma tact switches',48,-18,34,16,2.5,2.4,OP.NewBodyFeatureOperation)
    f.bodies.item(0).appearance=dark
    for x in [39,57]:
        top(bracket,62.9,'Nicho tact switch 6.4 x 6.4',x,-18,6.4,6.4,.4,1.1,OP.CutFeatureOperation)
    occ.transform2=transform
    # Original switch: approx. Y from supplied dimension, Z and travel must be measured.
    side(shell,92.1,'Acesso chave ON OFF da placa',47.82,-9,16,7.2,2.4,-2.7,OP.CutFeatureOperation)
    m.cut_round(mount,'Alivio trilho junto chave original',12,18,2,84,47.82,-7.8,-3.6)
    occ,power=m.component(root,'10 / Cursor ON OFF / usa chave original / MEDIR CURSO',transform)
    f=side(power,92.2,'Cursor ON OFF externo',47.82,-9,10,6.6,2.2,1.7,OP.NewBodyFeatureOperation)
    body=f.bodies.item(0);body.appearance=paint
    side(power,89,'Flange de retencao cursor',47.82,-9,18,10,2,1.2,OP.NewBodyFeatureOperation)
    side(power,89,'Ponte ate chave original',47.82,-9,5,4,.5,4,OP.NewBodyFeatureOperation)
    side(power,82.8,'Garfo acoplamento chave / hipotese',47.82,-9,5,4,.5,3.7,OP.NewBodyFeatureOperation)
    side(power,82.7,'Rasgo para lingueta / dimensao provisória',47.82,-9,2,2,.3,3.9,OP.CutFeatureOperation)
    for b in power.bRepBodies:b.appearance=paint
    occ.transform2=transform
    occ,ref=m.component(root,'REF / Chave ON OFF original / envelope provisório',transform)
    f=side(ref,80.5,'Chave na borda direita da placa',47.82,-9,9,5,.5,3,OP.NewBodyFeatureOperation)
    f.bodies.item(0).appearance=dark;occ.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for o in root.occurrences:
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    root.isOriginFolderLightBulbOn=False;root.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','power_switch','Original ON/OFF on right edge. XY based on supplied photo. Engagement, Z and travel provisional.')
    print('CONTROLS CREATED: 2 volume caps + original ON/OFF sliding actuator')
    print('VOLUME: mechanical only; tact-switch part, wiring and firmware mapping remain unspecified')
