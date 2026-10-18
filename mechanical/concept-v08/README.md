# NP2 V08 — fundo ampliado e acessos completos

Revisão de 2026-10-10. Material ABS; mesa de 350 × 350 mm. A V07 foi preservada.

## Alterações

O fundo do próprio corpo foi baixado 8 mm em relação à eletrônica. Moldura
e tampa acompanham essa extensão, com as mesmas larguras e raios de quina.
Não foram acrescentados pés ou uma base separada. A distância entre vidro,
placa e bases de parafuso permanece igual. A montagem foi reposicionada
para que o novo fundo continue apoiado na mesa.

Os dutos dos microfones receberam duas pequenas ligações às paredes
laterais, pois deixam de tocar a posição antiga do fundo. Os canais de
2 mm foram reabertos nesses apoios; furos frontais de 1,6 mm preservados.

O painel USB possui parede contínua de 1,4 mm, com bordas completas em
volta das três bocas. O rebaixo externo foi reduzido de 58 × 18 mm para
58 × 16,2 mm, preservando espaço para as capas dos plugs de referência.
A extensão inferior afasta os acessos da curva inferior da carcaça.

O rasgo do microSD foi reduzido de 22 × 12 mm para 16 × 4 mm, com raio
de 0,6 mm, alinhado ao corredor do cartão. O cartão fica recuado: levantar
o aparelho para acesso e verificar inserção/ejeção com a placa física.
Pode ser necessária pinça ou ferramenta fina para alcançar o cartão.

## Folgas e validação

São usados envelopes de referência USB A de 22 × 13 mm e USB C/UART de
14 × 10 mm, com pontas metálicas menores nos trechos de encaixe.
Para cada conector inferior foi reservada uma região de plug interno de
32 × 7 × 6,2 mm e outra de 32 × 3 × 8,1 mm para a dobra/saída dos fios.
Esses volumes são hipóteses conservadoras, não modelos dos cabos reais.
O relatório `connector-access-after.json` registra suas interseções.

A [Waveshare identifica os conectores inferiores como PH2.0 de 12 pinos](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B),
enquanto o STEP usa peças chamadas `PITCH1MM-12PIN-SMT-HORIZONTAL`.
Conferir medidas dos conectores e plugs físicos antes de fechar o encaixe.

Os testes de encaixe CAD usam os 1.345 corpos do STEP oficial. Isso não
substitui montagem física, ensaio de cabos, ejeção do microSD e impressão.
O `mesh-check.json` verifica seis malhas fechadas, unidades, faces de
contato com a mesa e posição dentro da placa 3MF de 350 × 350 mm.

## Arquivos e reprodução

- `NP2-case-v08.f3d` e `.step`: montagem com referência oficial.
- `NP2-v08-pecas-impressao.f3d`: seis peças orientadas.
- `NP2-v08-ABS-impressao.zip`: STLs, 3MF, guia, vistas e relatórios.

Com a V07 ativa, executar `tools/cad/create-case-v08.py` pelo MCP do Fusion,
depois `tools/cad/package-print-v08.py` com o Python do runtime.
