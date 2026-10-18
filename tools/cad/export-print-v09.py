def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/export-print-v08.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V08','V09').replace('v08','v09')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
