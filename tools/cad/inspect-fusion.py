import adsk.core, adsk.fusion, json

def run(_context: str):
    app = adsk.core.Application.get()
    print('VERSION', app.version)
    for doc in app.documents:
        print('DOCUMENT', doc.name, 'active', doc == app.activeDocument, 'modified', doc.isModified)
    design = adsk.fusion.Design.cast(app.activeProduct)
    if design:
        print('ROOT', design.rootComponent.name, 'bodies', design.rootComponent.bRepBodies.count,
              'occurrences', design.rootComponent.occurrences.count)
    for lib in app.materialLibraries:
        print('LIBRARY', lib.name)
        names = [a.name for a in lib.appearances if any(s in a.name.lower() for s in ['plastic', 'glass', 'rubber'])]
        print(json.dumps(names[:60]))
