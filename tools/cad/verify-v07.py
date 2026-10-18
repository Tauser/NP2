"""Reuse official-hardware fit checks, then insertion-path checks for V07."""
def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/verify-v06.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace("'V06'","'V07'").replace('concept-v06','concept-v07')
    # The V06 wrapper itself substitutes the V05 verifier document guard.
    namespace={};exec(compile(source,path,'exec'),namespace)
    namespace['run'](_context)
