import adsk.core,adsk.fusion,os
def run(_context: str):
 app=adsk.core.Application.get();os.makedirs('D:/Projetos/NP2/mechanical/concept-v10',exist_ok=True)
 path='D:/Projetos/NP2/mechanical/concept-v09/NP2-case-v09.f3d'
 doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(path));assert doc
 doc.name='NP2 - Mesa V10 / dutos MIC e fundo liso'
 print('V10 copied from V09; previous revision preserved')
