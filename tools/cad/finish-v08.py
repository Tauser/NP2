def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/finish-v05.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('V05','V08').replace('concept-v05','concept-v08').replace('case-v05','case-v08')
    source=source.replace('hardware oficial / corpo R6','folga inferior e acessos completos')
    source=source.replace('V08: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical','V08: fundo 8 mm mais baixo; moldura e tampa acompanhando corpo; microSD 16 x 4; painel USB continuo 1.4 mm; berco integrado')
    source=source.replace('CAD verificado contra STEP oficial: zero colisoes.','Placa e pecas CAD verificadas contra STEP oficial. Plugs USB e IO internos conferidos com envelopes de referencia; confirmar dimensoes reais e acesso fisico microSD no prototipo.')
    namespace={};exec(compile(source,path,'exec'),namespace);namespace['run'](_context)
