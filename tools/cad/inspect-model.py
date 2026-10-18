import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    print('ACTIVE COMMAND',app.userInterface.activeCommand)
    for o in d.rootComponent.occurrences:
        print(o.component.name,o.transform2.asArray())
    for a in d.appearances:
        print('APPEARANCE',a.name)
        for p in a.appearanceProperties:
            if p.objectType=='adsk::core::ColorProperty':
                c=p.value
                print(p.id,p.name,(c.red,c.green,c.blue,c.opacity))
    print('SNAPSHOTS',d.snapshots.count, d.snapshots.hasPendingSnapshot)
    print('ROOTFLAGS',[x for x in dir(d.rootComponent) if 'LightBulb' in x or 'Grid' in x])
    cam=app.activeViewport.camera
    print('CAMERA',cam.eye.asArray(),cam.target.asArray(),cam.upVector.asArray())
