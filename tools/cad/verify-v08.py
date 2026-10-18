def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/verify-v06.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace("'V06'","'V08'").replace('concept-v06','concept-v08')
    namespace={};exec(compile(source,path,'exec'),namespace);namespace['run'](_context)
