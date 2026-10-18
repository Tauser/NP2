# NP2 V10 — impressão em ABS

Material solicitado: ABS. Mesa informada: 350 × 350 mm.
O desenho e as orientações foram preparados com bico de 0,4 mm como hipótese
e camada de 0,2 mm como referência. Use o perfil da sua impressora e do seu ABS.

## Peças e posição na mesa

| Arquivo STL | Face apoiada | Suportes previstos |
| --- | --- | --- |
| 01-moldura.stl | Face externa da moldura | Sem suporte como ponto de partida; pequeno alívio traseiro faz ponte de 2 mm |
| 02-corpo.stl | Borda plana traseira | Suporte localizado nas faces superiores internas das aberturas USB, ON/OFF e microSD |
| 03-tampa-berco.stl | Face externa da tampa | Sem suporte como ponto de partida; berço e dedos sobem para cima |
| 04-volume-menos.stl | Face externa da tecla | Sem suporte como ponto de partida |
| 05-volume-mais.stl | Face externa da tecla | Sem suporte como ponto de partida |
| 06-cursor-onoff.stl | Face externa do cursor | Sem suporte como ponto de partida |

Os STL estão em milímetros, separados e com a base em Z=0. O 3MF contém
a distribuição das seis peças na mesa; preserve essas orientações ao abrir
no fatiador. A moldura está girada 90 graus no plano da mesa para caber ao
lado das peças maiores. A distribuição deixa espaço para brim.

O 3MF contém geometria e posições. Configure o perfil da sua impressora,
filamento, temperaturas e suportes no fatiador antes de gerar o arquivo
de impressão. O documento `NP2-v10-pecas-impressao.f3d` mostra a mesma distribuição.
O documento `NP2-case-v10.f3d` contém a montagem do produto e o hardware oficial.

## O que ficou mais simples

- O assento do alto-falante sobe da própria tampa em rampa, substituindo
  a prateleira que começava suspensa a 3 mm da tampa.
- As quatro travas têm rampa inferior e um lábio curto de 0,4 mm. O berço
  continua integrado à tampa e o alto-falante continua preso sem parafusos.
  Folga lateral do alojamento: 2 mm por lado em X e 0,6 mm por lado em Y.
- O corpo tem oito nervuras inclinadas sob os apoios da placa, deixando
  um corredor livre para os parafusos M2.5 originais.
- Uma rampa contínua apoia a plataforma dos tact switches de volume.
  Outra rampa apoia a parte traseira do painel rebaixado dos USBs.
- As teclas e o cursor têm flanges graduais, em vez de ombros largos
  que surgiam depois de uma haste estreita durante a impressão.

A hipótese de rampas até aproximadamente 45 graus segue as orientações
de [modelagem para impressão da Prusa](https://help.prusa3d.com/article/modeling-with-3d-printing-in-mind_164135).
A capacidade real depende do perfil e da impressora. Por isso os acessos
laterais do corpo ainda recebem indicação de suporte localizado.

## Ponto de partida para fatiar

Camada de 0,2 mm e quatro perímetros são um ponto de partida, com preenchimento
de 20% onde houver volume para isso. Nas linguetas e nos botões, confira na
prévia que as regiões estreitas ficam sólidas. As paredes geométricas são
nominalmente de 2,4 mm e as nervuras novas têm 1,6 mm de largura.

Para as peças grandes, começar com brim de 8 mm ajuda a avaliar a aderência.
Use câmara fechada e o perfil correto de ABS para controlar a contração e o
empenamento, conforme a [orientação de ABS da Prusa](https://help.prusa3d.com/article/abs_2058).
Esses valores são sugestões de preparação, não um perfil validado da sua máquina.

Pinte suporte apenas nas faces superiores internas dos acessos laterais
do corpo. Mantenha as fendas das linguetas livres de suporte para permitir
o encaixe e a remoção do alto-falante. A rampa do berço pode ser impressa
com a própria tampa apoiada na mesa.

## Conferência e limites

Os relatórios `fit-check.json`, `mesh-check.json` e `print-manifest.json`
registram, respectivamente, encaixes CAD, topologia/orientação das malhas
e dimensões/distribuição das peças. A análise de faces descendentes é uma
triagem geométrica; superfícies de pontes curtas também aparecem nesse cálculo.
Ela não substitui a prévia de suportes e caminhos do fatiador.

Não houve impressão física ou teste de força das travas. Conferir o encaixe
real do alto-falante, dos cabos e da lingueta ON/OFF no primeiro protótipo.
Os tact switches de volume continuam pendentes da definição dos componentes
físicos. Os dutos estão modelados; vedação e resposta acústica exigem teste real.

## Ajustes da V10

Os rebaixos dos plugs internos foram removidos e o fundo ficou liso. Mantida
a extensão de 4 mm. Os plugs grandes da referência anterior não estão
aprovados nesta versão; comparar os cabos reais antes de montar opcionais.

O corpo agora inclui os dois dutos MIC, com canal de 2 mm e câmaras sobre
os microfones. Há duas juntas frontais macias de 0,5 mm e duas juntas
traseiras de 2 mm, cortadas separadamente conforme a tabela do README.
As referências de juntas não entram nos seis STLs de ABS.

Mantenha os canais acústicos livres de suporte. Confira na prévia as pontes
curtas de 2 mm e paredes das curvas. Use suporte externo localizado apenas
onde o fatiador indicar; não deixe suporte preso dentro do canal. Caso seu
perfil não consiga fazer as pontes curtas, testar o trecho do duto antes de
imprimir o corpo inteiro. Não foram executados fatiamento nem impressão real.

Após imprimir, verificar passagem de luz/ar pelos dois canais e ausência
de detritos sem usar pressão excessiva nos microfones. Instalar as juntas
limpas antes da placa e da moldura, mantendo os orifícios livres de adesivo.
Inserir o conjunto de placa e tela pela frente, com a moldura removida.
Não pressionar diretamente a cápsula do microfone com o duto rígido.

A câmara foi desenhada em torno do envelope completo; confirmar o orifício
acústico real e a compressão da espuma no protótipo. A qualidade do áudio
não foi validada por estes testes CAD.
