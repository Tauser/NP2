import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get()
    opts=app.importManager.createSTEPImportOptions('D:/Projetos/NP2/mechanical/reference/waveshare-7b/official-7b.stp')
    doc=app.importManager.importToNewDocument(opts)
    doc.name='REF OFICIAL / Waveshare 7B / 3D 20251108'
    d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    print('ROOT BODIES',[(b.name,b.boundingBox.minPoint.asArray(),b.boundingBox.maxPoint.asArray()) for b in root.bRepBodies])
    print('OCCS',[(o.component.name,o.component.bRepBodies.count,o.childOccurrences.count) for o in root.occurrences])
    print('ROOT BB',root.boundingBox.minPoint.asArray(),root.boundingBox.maxPoint.asArray())
