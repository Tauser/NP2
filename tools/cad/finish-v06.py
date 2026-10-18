"""Export final assembly views and native files after V06 fit verification."""
def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/finish-v05.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('V05','V06').replace('concept-v05','concept-v06').replace('case-v05','case-v06')
    source=source.replace('hardware oficial / corpo R6','ABS / impressao simplificada')
    source=source.replace('V06: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical','V06: ABS 350x350; moldura alinhada ao corpo sem orelhas; rampas nos apoios e botoes; berco integral com labios curtos; STL orientados separados')
    namespace={};exec(compile(source,path,'exec'),namespace)
    namespace['run'](_context)
