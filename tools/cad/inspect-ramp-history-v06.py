import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V06' in app.activeDocument.name
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for f in shell.features.combineFeatures:
        if f.name=='Rampas incorporadas ao corpo':
            f.timelineObject.rollTo(True)
            print('BEFORE MERGE',[(b.name,b.volume*1000) for b in shell.bRepBodies])
            d.timeline.moveToEnd();break
    for f in shell.features.loftFeatures:
        if f.name=='Envelope somente para as rampas':
            f.timelineObject.rollTo(False)
            print('AFTER ENVELOPE',[(b.name,b.volume*1000) for b in shell.bRepBodies])
            print('LOFT BODIES',[(b.name,b.volume*1000) for b in f.bodies])
            d.timeline.moveToEnd();break
    print('FINAL',[(b.name,b.volume*1000) for b in shell.bRepBodies])
