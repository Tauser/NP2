import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    power=next(o for o in root.occurrences if o.component.name.startswith('08 /'))
    power.isGrounded=False
    if hasattr(power,'isGroundToParent'):power.isGroundToParent=False
    power.transform2=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2.copy()
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    print('POWER TRANSFORM',power.transform2.asArray())
