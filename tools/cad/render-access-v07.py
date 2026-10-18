"""Show the underside openings of the closed case; restore the saved camera."""
import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    assert d.activateRootComponent()
    saved=app.activeViewport.camera
    for o in d.rootComponent.occurrences:
        o.isLightBulbOn=not o.component.name.startswith('REF /')
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    app.userInterface.activeSelections.clear()
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(-16,-28,-16);cam.target=adsk.core.Point3D.create(0,3,5)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    assert app.activeViewport.saveAsImageFile('D:/Projetos/NP2/mechanical/concept-v07/np2-underside.png',1600,1200)
    app.activeViewport.camera=saved;adsk.doEvents();app.activeViewport.refresh()
    print('Underside access view saved')
