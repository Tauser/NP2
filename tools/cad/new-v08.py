import adsk.core, adsk.fusion, os
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    out='D:/Projetos/NP2/mechanical/concept-v08';os.makedirs(out,exist_ok=True)
    path=out+'/source-v07.f3d'
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(path))
    doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(path));assert doc
    doc.name='NP2 - Mesa V08 / folga inferior e acessos completos'
    print('V08 copy; V07 preserved')
