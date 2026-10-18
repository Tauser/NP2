import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    for o in d.rootComponent.occurrences:
        print('COMP',o.component.name)
        for b in o.component.bRepBodies:
            bb=b.boundingBox
            print('BODY',b.name,bb.minPoint.asArray(),bb.maxPoint.asArray())
        for p in o.component.constructionPlanes:
            if 'tampa' in p.name.lower():print('REARPLANE',p.geometry.origin.asArray(),p.geometry.normal.asArray())
    print('PREFS', [x for x in dir(app.preferences.generalPreferences) if any(y in x.lower() for y in ['grid','ground','color','orientation'])])
    print('COMMANDS',[(cd.id,cd.name) for cd in app.userInterface.commandDefinitions if any(x in cd.id.lower() for x in ['grid','ground','colorcycle'])])
