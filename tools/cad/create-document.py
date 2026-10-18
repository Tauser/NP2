import adsk.core, adsk.fusion, json

def run(_context: str):
    app = adsk.core.Application.get()
    doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name = 'NP2 - Mesa / conceito V01 sem bateria'
    design = adsk.fusion.Design.cast(app.activeProduct)
    print('DOC', doc.name, 'TYPE', design.designType)
    print('EXTRUDE', [x for x in dir(design.rootComponent.features.extrudeFeatures) if not x.startswith('_')])
    print('EXPORT', [x for x in dir(design.exportManager) if not x.startswith('_')])
    for lib in app.materialLibraries:
        print('LIB', lib.name, lib.appearances.count)
        print(json.dumps([(a.name,a.id) for a in lib.appearances][:25], ensure_ascii=False))
