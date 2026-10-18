import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    print('ACTIVE',app.activeDocument.name)
    print('DOCUMENTS',[x.name for x in app.documents])
    if d:
        print('COMPONENTS',[(o.component.name,o.isLightBulbOn) for o in d.rootComponent.occurrences])
        print('FEATURES',d.timeline.count)
