import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    first=root.occurrences.item(0)
    print('GROUND PROPS',[(p,getattr(first,p)) for p in dir(first) if 'ground' in p.lower() and not p.startswith('_')])
    first.isGrounded=False
    if hasattr(first,'isGroundToParent'):first.isGroundToParent=False
    first.transform2=root.occurrences.item(1).transform2
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    print('FRONT TRANSFORM',first.transform2.asArray())
    grid=app.userInterface.commandDefinitions.itemById('ViewLayoutGridOnCommand')
    lc=adsk.core.ListControlDefinition.cast(grid.controlDefinition)
    print('GRID ITEMS',[(x.name,x.isSelected) for x in lc.listItems])
    if lc.listItems.count==2:
        lc.listItems.item(1).isSelected=True
        grid.execute()
    adsk.doEvents()
