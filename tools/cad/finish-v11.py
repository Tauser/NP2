def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/finish-v05.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V05','V11').replace('v05','v11').replace('tools/cad/build-v11.py','tools/cad/build-v05.py')
 source=source.replace('hardware oficial / corpo R6','dutos MIC removiveis')
 source=source.replace('V11: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical','V11: dutos MIC separados em quatro metades abertas; dois cartuchos removiveis por parafusos; corpo sem tuneis acusticos; fundo sem rebaixos IO e extensao inferior 4 mm')
 source=source.replace('Validar em prototipo cabos, curso ON/OFF, travas e tact switches. Dutos acusticos MIC a definir.','Validar em prototipo cabos, ON/OFF, tact switches, parafusos dos cartuchos, juntas e audio. Plugs IO reais pendentes.')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
