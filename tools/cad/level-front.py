import adsk.core, adsk.fusion, sys, importlib.util, math
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('case','D:/Projetos/NP2/tools/cad/build-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
spec=importlib.util.spec_from_file_location('controls','D:/Projetos/NP2/tools/cad/add-controls.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    comp=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('01 /'))
    if comp.features.extrudeFeatures.itemByName('Nivelar moldura no plano da mesa'):
        print('Already level');return
    s=c.oriented(comp,comp.yZConstructionPlane,-90,'Recorte inferior / nivel da mesa')
    pts=[(-58,.2),(-55-.2*math.tan(math.radians(18)),.2),
         (-55+3.2*math.tan(math.radians(18)),-3.2),(-58,-3.2)]
    for i,(y,z) in enumerate(pts):
        yy,zz=pts[(i+1)%4]
        s.sketchCurves.sketchLines.addByTwoPoints(s.modelToSketchSpace(m.pt(-90,y,z)),s.modelToSketchSpace(m.pt(-90,yy,zz)))
    m.extrude(comp,s,180,m.OP.CutFeatureOperation,'Nivelar moldura no plano da mesa')
    s.isLightBulbOn=False;comp.isConstructionFolderLightBulbOn=False
    print('FRONT lower edge flush to Z=0 tabletop')
