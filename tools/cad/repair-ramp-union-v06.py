import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V06' in app.activeDocument.name
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    f=next(f for f in shell.features.combineFeatures if f.name=='Rampas incorporadas ao corpo')
    f.timelineObject.rollTo(True)
    tools=adsk.core.ObjectCollection.create()
    for b in f.toolBodies:
        if b.volume*1000<500000:tools.add(b)
    assert tools.count==10,tools.count
    f.toolBodies=tools
    d.timeline.moveToEnd()
    print('AFTER REPAIR',[(b.name,b.volume*1000) for b in shell.bRepBodies])
    tool=next(b for b in shell.bRepBodies if b.volume*1000>500000)
    removed=shell.features.removeFeatures.add(tool);assert removed
    removed.name='Remover ferramenta / preservar corpo oco'
    assert shell.bRepBodies.count==1 and shell.bRepBodies.item(0).volume*1000<60000
    print('HOLLOW BODY',shell.bRepBodies.item(0).volume*1000,'mm3')
