def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/verify-v08.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V08','V09').replace('concept-v08','concept-v09')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
