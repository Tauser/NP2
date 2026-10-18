# NP2 V11 — dutos removíveis

Revisão mecânica de 2026-10-10. Dois cartuchos acústicos independentes,
separados do corpo. Cada cartucho tem duas metades com canal aberto e
face externa plana para impressão. A carcaça contém apenas os assentos
abertos, os batentes inclinados e os pilotos de fixação.

Mantidos: extensão inferior de 4 mm, fundo sem os rebaixos dos plugs,
quinas arredondadas, traseira reta, acessos USB/microSD, ON/OFF original,
teclas de volume e alto-falante de 100 × 45 × 20 mm no berço da tampa.
O alto-falante continua preso sem parafusos. Não foi adicionada bateria.

## Montagem e remoção

1. Imprimir as quatro metades com a face plana externa na mesa e o canal
   aberto para cima, conforme os STL e o 3MF. Remover resíduos dos canais.
2. Unir A e B de cada lado alinhando os contornos e as duas metades da
   passagem do parafuso. Colar e vedar a junta **fora da placa**,
   mantendo adesivo fora da passagem acústica. O canal precisa ficar limpo.
3. Colocar cada cartucho no seu assento, encostando a aba no batente.
   Fixar com um parafuso pequeno por cartucho. Referência: diâmetro 2 mm,
   comprimento 8 mm; passagem de 2,4 mm e piloto de 1,7 mm no ABS.
   Escolher o parafuso físico e verificar seu encaixe no primeiro protótipo.
   O modelo inclui envelopes de cabeça de 4 × 2 mm; não imprimir esses envelopes.
4. Instalar as quatro juntas macias e o conjunto de tela/placa pela frente.
   A moldura fecha a montagem. Não apoiar ABS diretamente na cápsula MIC.

O cartucho **não é colado ao corpo**. Para removê-lo, retirar moldura e
conjunto de tela/placa, soltar seu parafuso e puxar para a frente. As metades
vedadas permanecem juntas, permitindo substituir o módulo inteiro.

## Microfones e juntas

As entradas corrigidas permanecem em X = −68/+68 mm, Y = −53,5 mm, Ø1,6 mm.
O trajeto contorna o vidro/PCB até as cápsulas oficiais em Y = −45,39999 mm.
A câmara interna de 4,2 × 3,4 mm envolve a face inteira de cada cápsula.
O STEP oficial não detalha o pequeno orifício acústico: conferir a placa real.

| Local | Quantidade | Exterior / interior | Espessura livre / comprimida |
| --- | ---: | --- | --- |
| Duto–moldura | 2 | Ø4 / Ø2 mm | 0,5 / 0,43 mm |
| Câmara–PCB, ao redor do MIC | 2 | 6,6 × 5,8 / 4,2 × 3,4 mm | 2 / 1,65 mm |

As juntas são referências de espuma macia no CAD, não peças de ABS.
Os apoios rígidos limitam a posição axial do cartucho; a espuma sela
ao redor do microfone sem pressionar diretamente sua tampa.
A vedação entre as duas metades também é necessária.

## Arquivos e limites

O conjunto tem seis peças principais e quatro metades opcionais dos dutos,
totalizando dez STL. Os dutos podem ser fabricados depois, sem reimprimir
a carcaça. Se forem omitidos, os furos frontais ficam sem condução vedada
até os MIC; não considerar isso uma solução acústica concluída.

Cada metade dos dutos mede aproximadamente 12,97 × 17,20 × 3,30 mm na
orientação de impressão. As quatro malhas estão fechadas e apoiadas em
face plana, sem faces descendentes acima de aproximadamente 45° fora da
camada inicial na triagem geométrica. As dez peças cabem na mesa informada.

`NP2-case-v11.f3d` contém a montagem e o hardware oficial. O documento
`NP2-v11-pecas-impressao.f3d`, o 3MF e os STL contêm as peças de impressão.
`fit-check.json`, `mic-channel-check.json` e `mesh-check.json` registram
as verificações geométricas efetivamente executadas.

Não houve impressão física, fatiamento específico da impressora, teste
de retenção dos parafusos, medição de força nas juntas ou teste de áudio.
Os tact switches de volume e os plugs opcionais internos ainda dependem
dos componentes reais. Os plugs grandes usados como referência na V09
continuam sem aprovação para o fundo sem rebaixos.

Reprodução pelo MCP: `new-v11.py`, `removable-mics-v11.py`,
`clear-removal-v11.py`, `open-mic-mouths-v11.py`, `verify-v11.py`,
`check-mics-v11.py`, `check-access-v11.py`, `finish-v11.py`,
`render-mics-v11.py`, `export-print-v11.py`. Depois executar
`package-print-v11.py` pelo Python local. V09 e V10 preservadas.
