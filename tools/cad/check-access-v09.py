def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/check-access-v08.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('Mesa V08','Mesa V09').replace('concept-v08','concept-v09')
 # The smaller enclosure uses an upward wire route. The old downward bend
 # protruded below the new floor; retain the same plug envelopes unchanged.
 source=source.replace("('IO right / wire bend reference', [[23,-53,-22.65],[55,-50,-14.55]])","('IO right / wire exit', [[23,-53,-14.55],[55,-50,-11.55]]), ('IO right / upward wire bend', [[23,-51,-18],[55,-48,-14]]), ('IO right / wire behind board', [[23,-48.5,-22.65],[55,-45.5,-17.7]])")
 source=source.replace("('IO left / wire bend reference', [[-55,-53,-22.65],[-23,-50,-14.55]])","('IO left / wire exit', [[-55,-53,-14.55],[-23,-50,-11.55]]), ('IO left / upward wire bend', [[-55,-51,-18],[-23,-48,-14]]), ('IO left / wire behind board', [[-55,-48.5,-22.65],[-23,-45.5,-17.7]])")
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)
