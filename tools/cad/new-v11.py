import adsk.core,adsk.fusion,os

def run(_context: str):
 app=adsk.core.Application.get();os.makedirs('D:/Projetos/NP2/mechanical/concept-v11',exist_ok=True)
 path='D:/Projetos/NP2/mechanical/concept-v09/NP2-case-v09.f3d'
 doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(path));assert doc
 doc.name='NP2 - Mesa V11 / dutos MIC removiveis'
 print('V11 copied from V09; archived V10 preserved')
