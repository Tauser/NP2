import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V05' in app.activeDocument.name
    rear=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('03 /'))
    bodies=adsk.core.ObjectCollection.create();main=rear.bRepBodies.item(0)
    for body in rear.bRepBodies:
        if body!=main:bodies.add(body)
    ci=rear.features.combineFeatures.createInput(main,bodies);ci.operation=adsk.fusion.FeatureOperations.JoinFeatureOperation
    rear.features.combineFeatures.add(ci).name='Tampa berco e quatro travas / peca unica'
    print('REAR SOLIDS',rear.bRepBodies.count)
    assert rear.bRepBodies.count==1
