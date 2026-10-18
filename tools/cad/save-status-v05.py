import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V05' in app.activeDocument.name
    d.attributes.add('NP2','revision','V05: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical')
    d.attributes.add('NP2','fit_status','CAD verificado contra STEP oficial: zero colisoes. Validar em prototipo cabos, curso ON/OFF, travas e tact switches. Dutos acusticos MIC a definir.')
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions('D:/Projetos/NP2/mechanical/concept-v05/NP2-case-v05.f3d'))
    print('FINAL ARCHIVE SAVED')
