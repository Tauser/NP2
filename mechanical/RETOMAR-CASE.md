# Encerramento da sessão — 10/10/2026

Revisão atual: **V11, dutos dos microfones removíveis**.

Também salvo no **Fusion, hub ti3d, projeto NP2**. Envio e processamento
dos 27 documentos confirmados, todos na versão 1:

- Na raiz: `NP2 - Mesa V11 - dutos MIC removiveis`.
- Na raiz: `NP2 - V11 - pecas para ABS - mesa 350x350`.
- Revisões anteriores: pasta `Historico da sessao - 2026-10-10` (23 documentos).
- Vistas de revisão: pasta `Estudos V11` (2 documentos).

Os identificadores dos arquivos e a confirmação de processamento ficam em
`session-2026-10-10/cloud-check.json`.

Montagem editável: `concept-v11/NP2-case-v11.f3d`.
STEP: `concept-v11/NP2-case-v11.step`.
Pacote de impressão: `concept-v11/NP2-v11-ABS-impressao.zip`.
Mesa com peças orientadas: `concept-v11/NP2-v11-ABS-350x350.3mf`.
As dez peças individuais estão em `concept-v11/STL/`.

O diretório `session-2026-10-10/` contém arquivos F3D locais dos documentos
NP2 e da referência oficial abertos no Fusion, inclusive revisões anteriores
e documentos de estudo. `save-manifest.json` registra cada checkpoint.
Esses arquivos de estudo não substituem os STL preparados para impressão.

## Decisões mantidas

- Sem bateria, sem base separada. Traseira reta e quinas arredondadas.
- Placa/display Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, modelo oficial.
  Referências medidas: vidro–PCB 9 mm; vidro–bases dos parafusos 15 mm.
- Alto-falante traseiro 100 × 45 × 20 mm. Berço e travas na tampa,
  sem parafusos para o alto-falante.
- Acesso à chave ON/OFF original, duas teclas de volume e acessos USB/microSD.
- Extensão inferior adicional de 4 mm. Removidos os rebaixos dos plugs.
  Os dois conectores inferiores destinam-se a componentes internos opcionais.
- ABS, mesa de 350 × 350 mm; bico de 0,4 mm e camada de 0,2 mm como hipóteses.
- Dois cartuchos MIC independentes. Cada um é impresso em duas metades,
  com canal aberto para cima, unido e vedado fora da placa. Um parafuso
  pequeno retém cada cartucho; o cartucho não é colado à carcaça.
  As quatro metades são opcionais na primeira impressão do case.
- Quatro juntas macias necessárias: duas entre dutos e moldura e duas
  ao redor dos MIC, apoiadas na PCB. Não imprimir essas juntas em ABS.

## Evidências e próximos cuidados

V11: 1.345 corpos oficiais, 455 verificações booleanas, sem colisões
detectadas nem erros de recursos CAD. Canais livres, metades abertas e
retirada dos cartuchos pela frente amostrados em CAD. USB e microSD livres
para os envelopes de referência. Dez malhas fechadas, com contato plano
na mesa; distribuição de 350 × 350 mm com espaço para brim de 8 mm.

Não houve fatiamento específico, impressão física ou teste de áudio.
No protótipo conferir as paredes locais de 0,6 mm dos dutos, o parafuso
escolhido, compressão das juntas, vedação das metades e entrada acústica
real dos MIC (o STEP contém somente envelopes das cápsulas).
Tact switches de volume e plugs internos ainda dependem das peças reais.
Os envelopes grandes de plugs da V09 não cabem no fundo sem rebaixos.

Para montagem, remoção, dimensões de juntas e scripts de reprodução,
consultar `concept-v11/README.md` e `concept-v11/GUIA-IMPRESSAO.md`.
Nenhuma alteração de firmware foi feita nesta atividade mecânica.
