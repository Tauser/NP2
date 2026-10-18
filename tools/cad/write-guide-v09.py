from pathlib import Path
out=Path('D:/Projetos/NP2/mechanical/concept-v09')
source=Path('D:/Projetos/NP2/mechanical/concept-v08/GUIA-IMPRESSAO.md').read_text(encoding='utf8')
source=source.replace('V08','V09').replace('v08','v09')
source=source.split('## Ajustes da V09')[0]
source+='''## Ajustes da V09

A extensão inferior foi reduzida de 8 para 4 mm. Corpo, moldura e tampa
ficaram menores juntos, mantendo seis peças e os mesmos raios de quina.
O microSD mantém a fenda de 16 × 4 mm e os USBs mantêm as bordas completas.

Há dois rebaixos rasos no fundo interno para os plugs inferiores; a parede
local fica com aproximadamente 1,2 mm. Confira os perímetros nessa região
e a necessidade de suporte localizado na prévia do fatiador.
Os fios precisam dobrar para cima e seguir para trás da placa, evitando
o trajeto descendente que exigia a carcaça anterior mais alta.

Os envelopes de referência têm folga pequena nesses rebaixos. Teste com os
plugs reais e avalie a contração do ABS antes de imprimir a versão final.
Levante o aparelho para acessar o microSD; conferir inserção e ejeção com
a placa física. Remova integralmente os suportes antes da montagem.
'''
(out/'GUIA-IMPRESSAO.md').write_text(source,encoding='utf8')
