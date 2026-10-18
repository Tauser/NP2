"""Replace separate holder by a rear-cover cradle with four integral clips."""
import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP

def cradle(rear,paint):
    s=v.rear_sketch(rear,'Berco INTEGRADO na tampa / 104 x 49',0)
    v.rear_outline(s,108,53,4,0,v.AUDIO_Y);v.rear_outline(s,104,49,2,0,v.AUDIO_Y)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=rear.features.extrudeFeatures.addSimple(ring,m.V.createByString('25 mm'),OP.JoinFeatureOperation)
    f.name='Berco e tampa em uma unica peca';s.isLightBulbOn=False
    s=v.rear_sketch(rear,'Assento acustico / reserva junta 3 mm',3)
    v.rear_outline(s,108,53,4,0,v.AUDIO_Y);v.rear_outline(s,94,39,2,0,v.AUDIO_Y)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=rear.features.extrudeFeatures.addSimple(ring,m.V.createByString('2 mm'),OP.JoinFeatureOperation)
    f.name='Assento periférico para alto-falante';s.isLightBulbOn=False
    s=v.rear_sketch(rear,'Alivios das quatro linguetas flexiveis',10)
    for x in [-32,32]:
        for sign in [-1,1]:
            for dx in [-4.4,4.4]:v.rear_outline(s,.8,3,.35,x+dx,v.AUDIO_Y+sign*25.5)
    m.extrude(rear,s,18,OP.CutFeatureOperation,'Fendas de flexao / comprimento 15 mm')
    s=v.rear_sketch(rear,'Extensao dos dedos de retencao',24.9)
    for x in [-32,32]:
        for sign in [-1,1]:v.rear_outline(s,8,2,.35,x,v.AUDIO_Y+sign*25.5)
    m.extrude(rear,s,2.2,OP.JoinFeatureOperation,'Quatro dedos / flexao para fora')
    for x in [-32,32]:
        for sign in [-1,1]:
            a=v.rear_sketch(rear,'Labio de retencao / folga 0.35 mm',25.35)
            v.rear_outline(a,8,4.2,.35,x,v.AUDIO_Y+sign*24.2);m.finish(a)
            b=v.rear_sketch(rear,'Rampa de entrada por pressao',27)
            v.rear_outline(b,8,1.8,.35,x,v.AUDIO_Y+sign*25.4);m.finish(b)
            inp=rear.features.loftFeatures.createInput(OP.JoinFeatureOperation)
            inp.loftSections.add(a.profiles.item(0));inp.loftSections.add(b.profiles.item(0))
            rear.features.loftFeatures.add(inp).name='Trava elastica integral / rampa de entrada'
    bodies=adsk.core.ObjectCollection.create();main=rear.bRepBodies.item(0)
    for body in rear.bRepBodies:
        if body!=main:bodies.add(body)
    if bodies.count:
        ci=rear.features.combineFeatures.createInput(main,bodies);ci.operation=OP.JoinFeatureOperation
        rear.features.combineFeatures.add(ci).name='Tampa berco e quatro travas / peca unica'
    assert rear.bRepBodies.count==1,'Cradle must be physically continuous with the cover'
    for body in rear.bRepBodies:body.appearance=paint
    rear.name='03 / Tampa traseira / berco e travas integrados sem parafusos'

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    rear=next(o.component for o in root.occurrences if o.component.name.startswith('03 /'))
    assert not rear.features.extrudeFeatures.itemByName('Berco e tampa em uma unica peca')
    old=next(o for o in root.occurrences if o.component.name.startswith('05 /'))
    old.deleteMe()
    cradle(rear,d.appearances.itemByName('NP2 grafite traseiro'))
    # Bridge starts inside the external cap so the power slider is one solid.
    old=next(o for o in root.occurrences if o.component.name.startswith('08 /'))
    transform=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2.copy();old.deleteMe()
    o,p=m.component(root,'08 / Cursor ON OFF / lingueta do modelo oficial',transform)
    y,z=32.7931,-10.45
    c.side(p,v.LEFT+.1,'Tecla externa ON OFF',y,z,8,4.6,1.8,-1.7,OP.NewBodyFeatureOperation)
    c.side(p,v.LEFT+.05,'Ponte ate lingueta',y,z,4,3,.5,5.35,OP.JoinFeatureOperation)
    c.side(p,-86.2,'Flange interna / retencao',y,z-3,14,7,2,.9,OP.JoinFeatureOperation)
    c.side(p,-84.1,'Garfo para lingueta',y,z,4,3,.5,1,OP.JoinFeatureOperation)
    c.side(p,-84.25,'Folga lingueta / envelope STEP + 0.2 mm',y,z,1.7,1.1,.2,1.35,OP.CutFeatureOperation)
    for body in p.bRepBodies:body.appearance=d.appearances.itemByName('NP2 teclas')
    o.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for occ in root.occurrences:
        occ.component.isOriginFolderLightBulbOn=False;occ.component.isConstructionFolderLightBulbOn=False
        for s in occ.component.sketches:s.isLightBulbOn=False
    print('REAR + CRADLE SOLIDS',rear.bRepBodies.count,'POWER SOLIDS',p.bRepBodies.count)
