"""Preserve the active corrected V06, then create a separate V07."""
import adsk.core, adsk.fusion, os
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V06' in app.activeDocument.name
    path='D:/Projetos/NP2/mechanical/concept-v07'
    os.makedirs(path,exist_ok=True)
    snapshot=path+'/source-v06.f3d'
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(snapshot))
    options=app.importManager.createFusionArchiveImportOptions(snapshot)
    doc=app.importManager.importToNewDocument(options)
    assert doc
    doc.name='NP2 - Mesa V07 / acesso conectores e microSD'
    print('V07 COPY of current V06; previous document preserved')
