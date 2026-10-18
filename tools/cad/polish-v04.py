import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;OP=v.OP
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    assert 'V04' in adsk.core.Application.get().activeDocument.name
    s=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    main=s.bRepBodies.item(0)
    fs=m.sketch(s,-3.02,'Exterior continuo / acabamento 0.02 mm')
    m.rounded(fs,v.W-.04,108.96,5.98,v.CX,.5);m.finish(fs)
    rs=v.rear_sketch(s,'Traseira exterior continua',.02)
    v.rear_outline(rs,v.REAR_W-.04,v.REAR_H-.04,5.98,v.CX,0);m.finish(rs)
    inp=s.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    inp.loftSections.add(fs.profiles.item(0));inp.loftSections.add(rs.profiles.item(0))
    lf=s.features.loftFeatures.add(inp);lf.name='Superficie exterior continua R6 nominal'
    tools=adsk.core.ObjectCollection.create();tools.add(lf.bodies.item(0))
    ci=s.features.combineFeatures.createInput(main,tools);ci.operation=OP.IntersectFeatureOperation
    cf=s.features.combineFeatures.add(ci);cf.name='Acabamento exterior / sem marcas dos apoios'
    for p in s.sketches:p.isLightBulbOn=False
    s.isConstructionFolderLightBulbOn=False
    print('POLISHED',s.bRepBodies.count,'body')
