"""FDM revision: rear-down gussets, short latch ledges and tapered buttons."""
import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP

def merge(comp,name):
    main=comp.bRepBodies.item(0);tools=adsk.core.ObjectCollection.create()
    for body in comp.bRepBodies:
        if body!=main:tools.add(body)
    if tools.count:
        ci=comp.features.combineFeatures.createInput(main,tools);ci.operation=OP.JoinFeatureOperation
        comp.features.combineFeatures.add(ci).name=name
    assert comp.bRepBodies.count==1 and comp.bRepBodies.item(0).lumps.count==1,name

def yz_profile(comp,name,offset,w,h,r,x,z):
    s=c.oriented(comp,comp.xZConstructionPlane,offset,name)
    c.outline_at(s,(x,offset,z),w,h,r);m.finish(s);return s

def side_profile(comp,name,x,y,z,w,h,r):
    s=c.oriented(comp,comp.yZConstructionPlane,x,name)
    c.outline_at(s,(x,y,z),h,w,r);m.finish(s);return s

def loft(comp,a,b,op,name):
    i=comp.features.loftFeatures.createInput(op)
    i.loftSections.add(a.profiles.item(0));i.loftSections.add(b.profiles.item(0))
    f=comp.features.loftFeatures.add(i);f.name=name;return f

def triangular_rib(comp,name,y,width,vertices):
    s=c.oriented(comp,comp.xZConstructionPlane,y+width/2,name)
    for i,(x,z) in enumerate(vertices):
        xx,zz=vertices[(i+1)%len(vertices)]
        s.sketchCurves.sketchLines.addByTwoPoints(s.modelToSketchSpace(m.pt(x,y+width/2,z)),s.modelToSketchSpace(m.pt(xx,y+width/2,zz)))
    return m.extrude(comp,s,-width,OP.NewBodyFeatureOperation,name).bodies.item(0)

def roof_ramp(comp):
    s=c.oriented(comp,comp.yZConstructionPlane,-24,'Rampa suporte volume / impressao traseira para baixo')
    vertices=[(55,-22),(43.9,-22),(55,-34)]
    for i,(y,z) in enumerate(vertices):
        yy,zz=vertices[(i+1)%3]
        s.sketchCurves.sketchLines.addByTwoPoints(s.modelToSketchSpace(m.pt(-24,y,z)),s.modelToSketchSpace(m.pt(-24,yy,zz)))
    return m.extrude(comp,s,42,OP.NewBodyFeatureOperation,'Rampa continua sob plataforma de volume').bodies.item(0)

def rear_cap(comp,paint):
    s=v.rear_sketch(comp,'Tampa plana / face externa na mesa',0)
    v.rear_outline(s,v.REAR_W,v.REAR_H,6,v.CX,0)
    m.extrude(comp,s,-2.4,OP.NewBodyFeatureOperation,'Tampa 2.4 mm')
    s=v.rear_sketch(comp,'Grelha e fixacao da tampa',.1)
    cc=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
    for row in range(11):
        for col in range(31):s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+(col-15)*3.2,cc.y*10+v.AUDIO_Y+(row-5)*3.2),.1)
    for x in [-78,78]:
        for y in [-v.REAR_H/2+10,v.REAR_H/2-10]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+x,cc.y*10+y),.16)
    for x in range(-40,41,8):v.rear_outline(s,3,12,1.5,x,30)
    m.extrude(comp,s,-2.7,OP.CutFeatureOperation,'Grelha ventilacao e quatro furos de abertura do case')
    # Tighter Y clearance makes the hook only one extrusion wide.
    s=v.rear_sketch(comp,'Berco integral / folga X 2 e Y 0.6 por lado',0)
    v.rear_outline(s,108,50.2,4,0,v.AUDIO_Y);v.rear_outline(s,104,46.2,2,0,v.AUDIO_Y)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=comp.features.extrudeFeatures.addSimple(ring,m.V.createByString('25 mm'),OP.NewBodyFeatureOperation)
    f.name='Parede vertical do berco';s.isLightBulbOn=False
    s=v.rear_sketch(comp,'Base do assento apoiada na tampa',0)
    v.rear_outline(s,108,50.2,4,0,v.AUDIO_Y)
    m.extrude(comp,s,5,OP.NewBodyFeatureOperation,'Base macica para assento em rampa')
    merge(comp,'Tampa e base unidas')
    a=v.rear_sketch(comp,'Abertura inferior do assento',-.01)
    v.rear_outline(a,104,46.2,7,0,v.AUDIO_Y);m.finish(a)
    b=v.rear_sketch(comp,'Abertura superior do assento',5.01)
    v.rear_outline(b,94,39,2,0,v.AUDIO_Y);m.finish(b)
    tool=loft(comp,a,b,OP.NewBodyFeatureOperation,'Ferramenta assento gradual / maximo 45 graus').bodies.item(0)
    objects=adsk.core.ObjectCollection.create();objects.add(tool)
    ci=comp.features.combineFeatures.createInput(comp.bRepBodies.item(0),objects);ci.operation=OP.CutFeatureOperation
    comp.features.combineFeatures.add(ci).name='Assento sem prateleira suspensa'
    s=v.rear_sketch(comp,'Fendas dos quatro dedos',10)
    for x in [-32,32]:
        for sign in [-1,1]:
            for dx in [-4.4,4.4]:v.rear_outline(s,.8,2.8,.35,x+dx,v.AUDIO_Y+sign*24.1)
    m.extrude(comp,s,18,OP.CutFeatureOperation,'Dedos flexiveis / fendas 0.8 mm')
    s=v.rear_sketch(comp,'Extensao dos dedos verticais',24.9)
    for x in [-32,32]:
        for sign in [-1,1]:v.rear_outline(s,8,2,.35,x,v.AUDIO_Y+sign*24.1)
    m.extrude(comp,s,2.35,OP.NewBodyFeatureOperation,'Quatro dedos sem teto suspenso')
    for x in [-32,32]:
        for sign in [-1,1]:
            a=v.rear_sketch(comp,'Base da rampa inferior da trava',24.55)
            v.rear_outline(a,8,1.8,.35,x,v.AUDIO_Y+sign*24);m.finish(a)
            b=v.rear_sketch(comp,'Topo da rampa inferior da trava',25.15)
            v.rear_outline(b,8,2.4,.35,x,v.AUDIO_Y+sign*23.7);m.finish(b)
            loft(comp,a,b,OP.NewBodyFeatureOperation,'Rampa inferior 45 graus')
            s=v.rear_sketch(comp,'Labio curto / uma linha de 0.4 mm',25.15)
            v.rear_outline(s,8,2.8,.35,x,v.AUDIO_Y+sign*23.5)
            m.extrude(comp,s,.6,OP.NewBodyFeatureOperation,'Labio de retencao 0.4 mm')
            a=v.rear_sketch(comp,'Inicio rampa de encaixe',25.75)
            v.rear_outline(a,8,2.8,.35,x,v.AUDIO_Y+sign*23.5);m.finish(a)
            b=v.rear_sketch(comp,'Final rampa de encaixe',27.15)
            v.rear_outline(b,8,1.4,.35,x,v.AUDIO_Y+sign*24.2);m.finish(b)
            loft(comp,a,b,OP.NewBodyFeatureOperation,'Rampa de entrada 45 graus')
    merge(comp,'Tampa berco e travas / uma peca para ABS')
    for body in comp.bRepBodies:body.appearance=paint

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V06' in app.activeDocument.name
    assert not d.attributes.itemByName('NP2','print_revision'),'Do not apply this migration twice'
    transform=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2.copy()
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    new=[]
    for x in [-78,78]:
        outer,inner,depth=(-88.5,-75,-36) if x<0 else (85,75,-34.5)
        for y in [-44,45]:
            for dy in [-3.5,3.5]:new.append(triangular_rib(shell,'Rampa apoio PCB / corredor livre para parafuso',y+dy,1.6,[(outer,-19.45),(inner,-19.45),(outer,depth)]))
    new.append(roof_ramp(shell))
    new.append(triangular_rib(shell,'Rampa traseira do painel USB',-16.2825,58.765,[(v.LEFT+.05,-21.65),(-82.5,-21.65),(v.LEFT+.05,-29)]))
    # Trim only new gussets to the existing housing envelope.
    fs=m.sketch(shell,-3.04,'Limite das rampas / frente')
    m.rounded(fs,v.W-.08,108.92,5.96,v.CX,.5);m.finish(fs)
    rs=v.rear_sketch(shell,'Limite das rampas / traseira',.04)
    v.rear_outline(rs,v.REAR_W-.08,v.REAR_H-.08,5.96,v.CX,0);m.finish(rs)
    envelope=loft(shell,fs,rs,OP.NewBodyFeatureOperation,'Envelope somente para as rampas').bodies.item(0)
    for rib in new:
        tools=adsk.core.ObjectCollection.create();tools.add(envelope)
        ci=shell.features.combineFeatures.createInput(rib,tools);ci.operation=OP.IntersectFeatureOperation;ci.isKeepToolBodies=True
        shell.features.combineFeatures.add(ci).name='Rampa limitada ao interior do corpo'
    removed=shell.features.removeFeatures.add(envelope);assert removed
    removed.name='Remover ferramenta / preservar corpo oco'
    merge(shell,'Rampas incorporadas ao corpo')
    m.slab(shell,'Uniao positiva painel USB e apoio inferior / evitar aresta tangente',1.2,2.4,.3,-82.4,-44,-15.45,-2.2)
    merge(shell,'Eliminar contato de espessura zero no apoio')
    s=v.rear_sketch(shell,'Reabrir pilotos M3 depois das rampas',-.1)
    cc=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
    for x in [-78,78]:
        for y in [-v.REAR_H/2+10,v.REAR_H/2-10]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+x,cc.y*10+y),.13)
    m.extrude(shell,s,8.2,OP.CutFeatureOperation,'Manter pilotos de abertura do case livres')
    for x in [-12,6]:c.top(shell,52.3,'Folga da flange volume / manter curso',x,-18,12,10,4.9,2.8,OP.CutFeatureOperation)
    c.side(shell,-87.2,'Bolso cursor simetrico / flange em rampa',33.9178,-10.45,12,8,2,2.6,OP.CutFeatureOperation)
    next(o for o in root.occurrences if o.component.name.startswith('03 /')).deleteMe()
    cap_occ,cap=m.component(root,'03 / Tampa ABS / berco e travas integrados',transform)
    rear_cap(cap,d.appearances.itemByName('NP2 grafite traseiro'))
    next(o for o in root.occurrences if o.component.name.startswith('06 /')).deleteMe()
    keys_occ,keys=m.component(root,'06 / Volume + e - / flange em rampa',transform)
    for x,label in [(-12,'Volume menos'),(6,'Volume mais')]:
        c.top(keys,56.4,label,x,-18,9,7,3.4,4,OP.NewBodyFeatureOperation)
        a=yz_profile(keys,'Inicio flange gradual',52.4,9,7,3.4,x,-18)
        b=yz_profile(keys,'Final flange gradual',50.8,11.4,9.4,4.6,x,-18)
        loft(keys,a,b,OP.JoinFeatureOperation,'Flange inclinada / 37 graus')
        c.top(keys,50.9,label+' haste',x,-18,3,3,.5,3.3,OP.JoinFeatureOperation)
        c.top(keys,56.5,label+' sinal horizontal',x,-18,3.5,.6,.25,.35,OP.CutFeatureOperation)
        if x==6:c.top(keys,56.5,label+' sinal vertical',x,-18,.6,3.5,.25,.35,OP.CutFeatureOperation)
    # Join each key's touching sub-bodies independently; never join the two keys.
    for x in [-12,6]:
        matching=[b for b in keys.bRepBodies if b.boundingBox.minPoint.x*10<x<b.boundingBox.maxPoint.x*10]
        if len(matching)>1:
            objects=adsk.core.ObjectCollection.create()
            for b in matching[1:]:objects.add(b)
            ci=keys.features.combineFeatures.createInput(matching[0],objects);ci.operation=OP.JoinFeatureOperation
            keys.features.combineFeatures.add(ci).name='Tecla unica / face externa plana'
    assert keys.bRepBodies.count==2
    for body in keys.bRepBodies:body.appearance=d.appearances.itemByName('NP2 teclas')
    next(o for o in root.occurrences if o.component.name.startswith('08 /')).deleteMe()
    power_occ,power=m.component(root,'08 / Cursor ON OFF / flange gradual',transform)
    y,z=32.7931,-10.45
    c.side(power,v.LEFT+.1,'Face externa plana',y,z,8,4.6,1.8,-1.7,OP.NewBodyFeatureOperation)
    c.side(power,v.LEFT+.05,'Ponte ate lingueta',y,z,4,3,.5,5.35,OP.JoinFeatureOperation)
    a=side_profile(power,'Inicio retencao gradual',v.LEFT+.1,y,z,4,3,.5)
    b=side_profile(power,'Final retencao gradual',-86.2,y,z,8,7,2.5)
    loft(power,a,b,OP.NewBodyFeatureOperation,'Flange em rampa / 38 graus')
    c.side(power,-86.2,'Flange curta de retencao',y,z,8,7,2.5,.9,OP.NewBodyFeatureOperation)
    c.side(power,-84.1,'Garfo lingueta',y,z,4,3,.5,1,OP.NewBodyFeatureOperation)
    merge(power,'Cursor unico / face externa na mesa')
    c.side(power,-84.25,'Folga lingueta oficial',y,z,1.7,1.1,.2,1.35,OP.CutFeatureOperation)
    for body in power.bRepBodies:body.appearance=d.appearances.itemByName('NP2 teclas')
    # Capture final positions after all component history edits.
    for o in root.occurrences:
        if o.component.name.startswith('REF OFICIAL'):continue
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False;o.transform2=transform
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    d.attributes.add('NP2','print_revision','V06 / ABS / nozzle assumption 0.4 / bed 350 x 350; body rear down, cover exterior down, caps exterior down')
    print('V06 PRINT REVISION',d.timeline.count,'features; gussets',len(new),'cap',cap.bRepBodies.count,'keys',keys.bRepBodies.count,'power',power.bRepBodies.count)
