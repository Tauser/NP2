def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/finish-v05.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V05','V10').replace('v05','v10')
 # Keep the helper module dependency at the real source revision.
 source=source.replace("tools/cad/build-v10.py","tools/cad/build-v05.py")
 source=source.replace('hardware oficial / corpo R6','dutos MIC e fundo liso')
 source=source.replace('V10: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical','V10: fundo liso sem rebaixos; folga extra 4 mm; portas MIC X +/-68 e Y -53.5; dois dutos independentes integrados ao corpo; juntas macias obrigatorias')
 source=source.replace('Validar em prototipo cabos, curso ON/OFF, travas e tact switches. Dutos acusticos MIC a definir.','Validar em prototipo cabos, curso ON/OFF, travas, tact switches, juntas e resposta acustica dos dutos. Plugs IO internos dependem das medidas reais; os envelopes grandes da V09 nao cabem no fundo liso.')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
