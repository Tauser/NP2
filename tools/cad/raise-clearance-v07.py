"""Two integral side feet keep lower insertion paths above the tabletop.

They are part of the single shell solid, not an extra printed pedestal.
The original body rear plane remains the print bed plane. Front sloped
foot ends grow gradually when printing from that plane.
"""
import adsk.core, adsk.fusion, json
def add_feet(d):
    root=d.rootComponent
    shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
    comp=shell.component;inv=shell.transform2.copy();inv.invert()
    mgr=adsk.fusion.TemporaryBRepManager.get()
    bb=mgr.copy(comp.bRepBodies.item(0).createForAssemblyContext(shell)).boundingBox
    rear_y=bb.maxPoint.y*10
    def local(x,y,z):
        p=adsk.core.Point3D.create(x/10,y/10,z/10);p.transformBy(inv);return p
    for x,label in [(-84,'esquerdo'),(80.4,'direito')]:
        # Native +X is also world +X. Sketch plane normal is +X.
        datum=comp.sketches.add(comp.xYConstructionPlane);datum.name='Plano apoio '+label;datum.is3D=True
        points=[datum.sketchPoints.add(local(*p)) for p in [(x-1.8,0,0),(x-1.8,10,0),(x-1.8,0,10)]]
        pi=comp.constructionPlanes.createInput();pi.setByThreePoints(*points)
        plane=comp.constructionPlanes.add(pi);plane.isLightBulbOn=False;datum.isLightBulbOn=False
        s=comp.sketches.add(plane);s.name='Apoio integral '+label+' / rampa frontal';s.isComputeDeferred=True
        polygon=[(18,0),(rear_y,0),(rear_y,16.5),(8,16.5)]
        pts=[s.modelToSketchSpace(local(x-1.8,y,z)) for y,z in polygon]
        for i,p in enumerate(pts):s.sketchCurves.sketchLines.addByTwoPoints(p,pts[(i+1)%4])
        s.isComputeDeferred=False
        for curve in s.sketchCurves:curve.isFixed=True
        f=comp.features.extrudeFeatures.addSimple(s.profiles.item(0),adsk.core.ValueInput.createByString('3.6 mm'),adsk.fusion.FeatureOperations.JoinFeatureOperation)
        f.name='Apoio '+label+' integrado / 15 mm livres sob corpo';s.isLightBulbOn=False
        assert comp.bRepBodies.count==1 and comp.bRepBodies.item(0).lumps.count==1
    comp.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','table_clearance_v07',json.dumps({'lift_mm':15,'integral_shell_feet':2,'feet_width_mm':3.6,'feet_rear_y_mm':rear_y,'external_lower_cables_assumed':True}))
    print('Two integral 3.6 mm side feet; assembled module lifted 15 mm; shell remains one connected solid')

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    if d.attributes.itemByName('NP2','table_clearance_v07'):
        print('Integral feet already applied');return
    # Change only the world Z position of the full assembled unit.
    # The official hardware keeps its distinct datum rotation/translation.
    for o in d.rootComponent.occurrences:
        t=o.transform2.copy();shift=t.translation;shift.z+=1.5;t.translation=shift
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False;o.transform2=t
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    add_feet(d)
