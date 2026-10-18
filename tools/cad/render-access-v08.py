def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/render-access-v07.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('V07','V08').replace('v07','v08')
    namespace={};exec(compile(source,path,'exec'),namespace);namespace['run'](_context)
