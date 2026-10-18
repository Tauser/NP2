import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get()
    options=app.importManager.createFusionArchiveImportOptions('D:/Projetos/NP2/mechanical/concept-v05/NP2-case-v05.f3d')
    doc=app.importManager.importToNewDocument(options)
    assert doc
    doc.name='NP2 - Mesa V06 / ABS / impressao simplificada'
    d=adsk.fusion.Design.cast(app.activeProduct)
    print('V06 COPY',d.timeline.count,'features; prior V05 preserved')
