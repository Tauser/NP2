def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/finish-v08.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V08','V09').replace('v08','v09').replace('fundo 8 mm','fundo 4 mm').replace('folga inferior e acessos completos','folga inferior compacta')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
