"""Extend the rounded enclosure bottom, leaving hardware in panel coordinates.

Front outlines grow by 8/cos(18) mm; vertical rear outlines grow by 8 mm.
Only the bottom line and two lower corner arcs move. The vertical sides
stretch through their coincident endpoints, preserving the R6 corners.
"""
import adsk.core, adsk.fusion, importlib.util, math, json
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;OP=v.OP

def lower_outline(d,s,amount):
    if s.attributes.itemByName('NP2','lowered_v08'):return
    s.timelineObject.rollTo(False);s.isComputeDeferred=True
    curves=list(s.sketchCurves)
    points=[]
    for curve in curves:
        curve.isFixed=False
        points.extend([curve.startSketchPoint.geometry,curve.endSketchPoint.geometry])
    low=min(p.y for p in points);high=max(p.y for p in points);mid=(low+high)/2
    selected=adsk.core.ObjectCollection.create()
    for curve in curves:
        geom=curve.geometry
        if geom.objectType=='adsk::core::Arc3D' and geom.center.y<mid:selected.add(curve)
        elif geom.objectType=='adsk::core::Line3D':
            if abs(geom.startPoint.y-geom.endPoint.y)<1e-7 and geom.startPoint.y<mid:selected.add(curve)
            elif abs(geom.startPoint.x-geom.endPoint.x)<1e-7:
                selected.add(curve.startSketchPoint if geom.startPoint.y<geom.endPoint.y else curve.endSketchPoint)
    assert selected.count==5,(s.name,selected.count)
    t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(0,-amount/10,0)
    assert s.move(selected,t),s.name
    for curve in curves:curve.isFixed=True
    s.isComputeDeferred=False
    ys=[p.geometry.y for curve in curves for p in [curve.startSketchPoint,curve.endSketchPoint]]
    print('OUTLINE',s.name,'before',low*10,high*10,'after',min(ys)*10,max(ys)*10)
    assert abs(min(ys)-(low-amount/10))<1e-6 and abs(max(ys)-high)<1e-6,'Outline did not stretch as intended'
    assert s.profiles.count==1,(s.name,s.profiles.count)
    s.attributes.add('NP2','lowered_v08',str(amount));s.isLightBulbOn=False
    d.timeline.moveToEnd()

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V08' in app.activeDocument.name
    if d.attributes.itemByName('NP2','lower_floor_v08'):
        print('Lower floor already applied');return
    root=d.rootComponent
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    # Keep the existing acoustic ducts attached when the old floor moves down.
    if not d.attributes.itemByName('NP2','mic_bridges_v08'):
        s=next(s for s in shell.sketches if s.name.startswith('Bocal MIC'))
        s.timelineObject.rollTo(True)
        for cx,w,label in [(-77.8,21.8,'esquerdo'),(80.1,10.1,'direito')]:
            s=m.sketch(shell,-3.04,'Ligacao bocal MIC '+label+' / contorno inferior ampliado')
            m.rounded(s,w,3.6,.4,cx,-51)
            m.extrude(shell,s,-2.9,OP.JoinFeatureOperation,'Apoio curto MIC '+label+' ligado a lateral')
        d.timeline.moveToEnd();d.attributes.add('NP2','mic_bridges_v08','short side bridges preserve ducts after lowering floor')
    delta=8/math.cos(math.radians(18))
    names=[('Contorno frontal do corpo / R6',delta),('Contorno traseiro menor / R6',8),
           ('Exterior continuo / acabamento 0.02 mm',delta),('Traseira exterior continua',8),
           ('Acabamento exterior do painel rebaixado',delta),('Traseira de acabamento do painel',8)]
    for name,amount in names:
        s=shell.sketches.itemByName(name);assert s,name
        lower_outline(d,s,amount)
    front=next(o.component for o in root.occurrences if o.component.name.startswith('01 /'))
    lower_outline(d,front.sketches.itemByName('Moldura alinhada ao corpo / sem orelhas laterais / perfil'),delta)
    cap=next(o.component for o in root.occurrences if o.component.name.startswith('03 /'))
    lower_outline(d,cap.sketches.itemByName('Tampa plana / face externa na mesa'),8)
    # The added MIC joins can consume the original target rib. Rebind the
    # support union to the hollow main body before recomputing downstream cuts.
    f=shell.features.combineFeatures.itemByName('Apoios e suporte volume integrados ao corpo')
    f.timelineObject.rollTo(True)
    bodies=list(shell.bRepBodies);main=max(bodies,key=lambda b:b.volume)
    tools=adsk.core.ObjectCollection.create()
    for body in bodies:
        if body!=main:tools.add(body)
    f.targetBody=main;f.toolBodies=tools
    d.timeline.moveToEnd()
    # Restore the new outer bottom to the tabletop; relative hardware placement
    # and glass/PCB/standoff distances remain unchanged.
    for o in root.occurrences:
        t=o.transform2.copy();p=t.translation;p.z+=.8;t.translation=p;o.transform2=t
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for o in root.occurrences:
        o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    d.attributes.add('NP2','lower_floor_v08',json.dumps({'additional_internal_height_mm':8,'front_profile_extension_mm':delta,'rear_profile_extension_mm':8,'integral_extra_feet':False}))
    print('Case floor lowered by 8 mm; front and rear curves retained; acoustic ducts supported')
