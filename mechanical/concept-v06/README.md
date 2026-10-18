# NP2 V06 — peças simplificadas para ABS

Revisão de 2026-10-09. Material solicitado: ABS; mesa de 350 × 350 mm.
A V05 foi preservada; a V06 foi criada como cópia em documento separado.

O pacote `NP2-v06-ABS-impressao.zip` contém seis STLs em milímetros,
uma placa 3MF com as peças orientadas, o guia e os relatórios de validação.
As peças são moldura, corpo, tampa com berço, duas teclas de volume e cursor
ON/OFF. Os modelos de eletrônica não entram nos arquivos de impressão.

O documento `NP2-case-v06.f3d` e o STEP correspondente contêm a montagem
com o hardware oficial da Waveshare. O documento
`NP2-v06-pecas-impressao.f3d` contém somente as peças orientadas para imprimir.
O retângulo de referência da mesa aparece apenas como sketch.

## Mudanças para imprimir

A tampa continua plana com o berço incorporado, sem parafusos para o
alto-falante de 100 × 45 × 20 mm. O assento periférico é gradual desde
a tampa; as linguetas têm rampa inferior, lábio de 0,4 mm e rampa de entrada.
A folga em Y do alto-falante foi ajustada para 0,6 mm por lado para encurtar
o lábio das travas. A retenção é uma hipótese mecânica para teste em ABS.

O corpo possui oito nervuras inclinadas sob os apoios M2.5, uma rampa
sob a plataforma dos tact switches e outra atrás do painel USB rebaixado.
Os corredores dos parafusos e a folga para os botões foram preservados.
O contato tangente entre o painel USB e o apoio inferior esquerdo recebeu
uma pequena nervura com espessura positiva, eliminando a aresta de espessura zero.
As flanges das teclas e do cursor foram substituídas por transições graduais.

O exterior com quinas arredondadas, traseira vertical, inclinação da tela,
borda assimétrica e ausência de base/bateria da V05 foram conservados.
Correção da moldura: o contorno externo agora coincide com a seção frontal
acabada do corpo (174,22 × 108,94 mm, R5,97, centro X −1,8 / Y +0,5 mm).
O contorno anterior de 110 mm deixava excesso nas duas curvas inferiores,
mesmo depois do nivelamento da aresta de baixo. As quatro quinas agora
usam o mesmo perfil do corpo; foram preservados o arredondamento frontal
de 0,5 mm, a janela do vidro, os dois microfones e o alívio do flex touch.
O corpo ainda tem indicação de suporte localizado nas aberturas laterais;
as demais peças foram preparadas para começar sem suporte.

## Verificação

- 1.345 corpos de hardware oficial considerados; seis superfícies usam
  envelopes conservadores, como na V05.
- 409 testes booleanos: zero colisões acima de 0,01 mm³ e zero falhas.
- Nenhum erro ou aviso de feature; o aviso vazio do grupo importado é
  registrado separadamente no relatório.
- Cada peça CAD tem um único sólido conectado.
- Comparação booleana da moldura com o perfil externo do corpo: zero
  excesso nos quatro cantos, registrado em `bezel-seam-after.json`.
- Os dois botões também foram conferidos com deslocamento de referência
  de 0,5 mm: zero interseção com o corpo.
- Seis STLs com incidência de duas faces por aresta, sem arestas abertas
  ou não manifold, volume positivo e face de contato plana em Z=0.
- As seis malhas da placa 3MF ficam dentro de 350 × 350 mm, inclusive com
  envelopes de brim de 8 mm sem sobreposição.

Não houve impressão física ou fatiamento com perfil específico da máquina.
O guia detalha orientação, suportes e pendências de componentes/encaixe.
As medidas dos conectores continuam derivadas do STEP oficial documentado
em `../reference/waveshare-7b/README.md`.

## Reprodução

Executar `tools/cad/create-case-v06.py` pelo MCP do Fusion com timeout de
1.200 segundos para a revisão geométrica. Depois executar
`tools/cad/package-print-v06.py` com o Python do runtime para conferir as
malhas e montar o ZIP. O primeiro script cria uma cópia nova da V05, valida
os encaixes e exporta os modelos. Não altera firmware.
