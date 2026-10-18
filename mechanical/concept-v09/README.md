# NP2 V09 — folga inferior compacta

Revisão de 2026-10-10. A extensão inferior foi reduzida de 8 para **4 mm**,
com corpo, moldura e tampa encurtados juntos. As larguras, quinas
arredondadas, alinhamento da placa e distâncias entre vidro/placa/bases
originais foram preservados. Não há pés ou base separada.

O microSD continua com rasgo de 16 × 4 mm e os três USBs mantêm as bordas
completas da V08. O menor recuo inferior também aproxima o cartão da abertura.
Levantar o aparelho para usar o microSD e verificar a ejeção física.

Dois rebaixos internos rasos de 34 × 16 mm acomodam os plugs inferiores
sem aumentar a carcaça por fora. A espessura local do fundo fica em
aproximadamente 1,2 mm; o restante mantém a parede nominal de 2,4 mm.
Os fios devem dobrar para cima e seguir para trás da placa, conforme os
envelopes do relatório `connector-access-after.json`. A hipótese anterior
de fios descendo abaixo do plug não cabe nesta versão compacta.

Os plugs internos de referência continuam com 32 × 7 × 6,2 mm. Os USBs usam
capas de referência de 22 × 13 mm (A) e 14 × 10 mm (C/UART). Não são modelos
dos cabos reais. A folga local é pequena e deve ser conferida com os plugs
escolhidos e com a contração da primeira impressão em ABS.

O encaixe é conferido contra os 1.345 corpos do STEP oficial Waveshare.
Os relatórios registram colisões CAD, integridade dos sólidos e das seis
malhas, unidades e orientação na mesa de 350 × 350 mm. Não houve impressão
ou ensaio físico do produto.

Arquivos: `NP2-case-v09.f3d`, `.step`, seis STLs, 3MF, documento de peças
orientadas e `NP2-v09-ABS-impressao.zip`. A V08 foi preservada.

Reprodução: executar `new-v09.py`, `compact-floor-v09.py`,
`relieve-internal-io-v09.py`, `check-access-v09.py`, `verify-v09.py`,
`finish-v09.py`, `render-access-v09.py` e `export-print-v09.py` pelo MCP do
Fusion. Depois executar `package-print-v09.py` com o Python do runtime.
