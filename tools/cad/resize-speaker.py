import adsk.core, adsk.fusion, sys, importlib.util
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('v03','D:/Projetos/NP2/tools/cad/build-v03.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V03' in app.activeDocument.name
    oldspeaker=next(o for o in root.occurrences if o.component.name.startswith('REF / Alto-falante'))
    oldholder=next(o for o in root.occurrences if o.component.name.startswith('05 /'))
    transform=oldspeaker.transform2
    oldspeaker.deleteMe();oldholder.deleteMe()
    rear=next(o.component for o in root.occurrences if o.component.name.startswith('03 /'))
    oldgrill=rear.features.extrudeFeatures.itemByName('138 perfuracoes de audio traseiras')
    if oldgrill:oldgrill.deleteMe()
    s=v.rear_sketch(rear,'Grelha audio / alto-falante 100 x 45',.1)
    cc=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
    for row in range(11):
        for col in range(31):
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+(col-15)*3.2,cc.y*10+(row-5)*3.2),.1)
    m.extrude(rear,s,-2.7,m.OP.CutFeatureOperation,'341 perfuracoes de audio traseiras')
    for name,expr,comment in [('speaker_width_ref','100 mm','Medido pelo usuario'),
                              ('speaker_height_ref','45 mm','Medido pelo usuario'),
                              ('speaker_depth','20 mm','Usuario informou 2 cm'),
                              ('speaker_clearance_ref','2 mm','Folga por lado do berco')]:
        d.userParameters.add(name,m.V.createByString(expr),'mm',comment)
    occ,speaker=m.component(root,'REF / Alto-falante TRASEIRO / 100 x 45 x 20 medido pelo usuario',transform)
    s=v.rear_sketch(speaker,'Envelope medido do alto-falante',5)
    v.rear_outline(s,100,45,3)
    f=m.extrude(speaker,s,'speaker_depth',m.OP.NewBodyFeatureOperation,'Alto-falante 100 x 45 x 20')
    f.bodies.item(0).appearance=d.appearances.itemByName('NP2 vidro preto');occ.transform2=transform
    occ,holder=m.component(root,'05 / Berco traseiro / alojamento 104 x 49 x 24',transform)
    s=v.rear_sketch(holder,'Berco / folga 2 mm por lado',3)
    v.rear_outline(s,108,53,4);v.rear_outline(s,104,49,2)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,m.V.createByString('24 mm'),m.OP.NewBodyFeatureOperation)
    f.name='Berco para alto-falante medido';f.bodies.item(0).appearance=d.appearances.itemByName('NP2 grafite');s.isLightBulbOn=False
    # Rear-side seat retains the enclosure perimeter; foam and retention strap remain fit items.
    s=v.rear_sketch(holder,'Apoio periférico para alto-falante',3)
    v.rear_outline(s,108,53,4);v.rear_outline(s,94,39,2)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,m.V.createByString('2 mm'),m.OP.JoinFeatureOperation)
    f.name='Assento periférico / vedacao compressível';s.isLightBulbOn=False
    occ.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for o in root.occurrences:
        o.component.isConstructionFolderLightBulbOn=False;o.component.isOriginFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    d.attributes.add('NP2','speaker_measured','User supplied 100 x 45 x 20 mm on 2026-10-09. Mounting hole pattern unknown.')
    print('SPEAKER UPDATED 100 x 45 x 20 mm; cavity 104 x 49 x 24 mm; 341 grill holes')
