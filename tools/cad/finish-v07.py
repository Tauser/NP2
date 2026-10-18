"""Export the V07 assembly and general views."""
def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/finish-v05.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('V05','V07').replace('concept-v05','concept-v07').replace('case-v05','case-v07')
    source=source.replace('hardware oficial / corpo R6','acesso conectores e microSD')
    source=source.replace('V07: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical','V07: acesso microSD inferior para servico; baia USB ampla; IO opcionais internos; sem apoios extras; moldura alinhada ao corpo')
    source=source.replace('CAD verificado contra STEP oficial: zero colisoes.','Placa e pecas CAD verificadas contra STEP oficial; cabos IO internos nao validados. USBs conferidos com envelopes de referencia. microSD exige levantar o aparelho.')
    namespace={};exec(compile(source,path,'exec'),namespace)
    namespace['run'](_context)
