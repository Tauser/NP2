# NP2 — case de mesa, conceito V03

Atualizado em 09/10/2026 no Fusion 360 2705.1.30 via MCP local
`http://127.0.0.1:27182/mcp`. Este é um conceito de encaixe e aparência,
com operações editáveis. Não é uma liberação de fabricação.

## Configuração solicitada

- Sem bateria e sem base separada: o corpo em cunha apoia diretamente na mesa.
- Tela em paisagem, inclinada 18° em relação à vertical; mesa no plano Z=0.
- Frente nominal 176 × 110 mm, raio 6 mm. Moldura com 3 mm e parede com 2,4 mm.
- Vidro 164,28 × 99,17 mm, janela com 0,30 mm de folga por lado.
- PCB 164 × 97 mm; entre centros dos furos: 156 × 89 mm.
  Os 89 mm da foto são o pitch vertical, não a altura total da placa.
- Microfones com portas frontais de 1,6 mm na borda inferior.
- Duas teclas de volume no topo; cursor lateral para a chave ON/OFF original.
- Alto-falante traseiro: **100 × 45 × 20 mm**, medidas informadas pelo usuário.
  Alojamento de 104 × 49 × 24 mm, assento periférico e abas de fixação à tampa.
  As abas do berço não representam os furos do alto-falante original.
- Grelha traseira com 341 furos de 2 mm; ventilação acima e abaixo.
- Envelope montado aproximado: 178 × 85 × 106 mm, incluindo cursor e teclas.

## Arquivos

- `NP2-case-v03.f3d`: modelo nativo, componentes, histórico e referências.
- `NP2-case-v03.step`: geometria de intercâmbio, incluindo envelopes de referência.
- `np2-front.png`, `np2-rear.png`: montagem exterior.
- `np2-inside.png`: vista traseira sem tampa para conferir o alto-falante.

Os componentes `REF /` são envelopes para estudo de montagem. Eles não são
peças para impressão. Os sketches de contorno têm geometria fixa; profundidades
e espessuras selecionadas usam parâmetros. Não anunciar parametrização integral.

## Reprodução

No repositório, com o Fusion e o MCP local ativos:

```powershell
./tools/cad/invoke-fusion.ps1 -ScriptPath tools/cad/create-case-v03.py
```

Isso abre um novo documento e exporta em `mechanical/concept-v03/`.
Os scripts atuais usam caminhos absolutos deste workspace. O script de ligação
usa o protocolo MCP e não instala ou altera a configuração global do Codex.

## Verificação executada e pendências

O Fusion concluiu as operações sem erros ou warnings na timeline. Foram
conferidas as vistas frontal/traseira, os componentes e o apoio inferior em Z=0.
F3D e STEP foram exportados. Não houve fabricação, ensaio térmico ou acústico.

Antes de imprimir: confirmar altura real do conjunto LCD/PCB/componentes,
diâmetros e fixações, envelopes dos conectores, folgas da montagem frontal,
curso e lingueta da ON/OFF, switches de volume, retenção/espuma do alto-falante,
abas e furos da peça real. Medir a posição dos microfones e completar os dutos
até as portas frontais. Ajustar trilhos e parafusos para retirar interferências
de montagem; ainda não foi executado um gate completo de interferências.

A interface elétrica e o firmware dos botões de volume não foram alterados.
O encaixe do alto-falante usa as medidas do usuário, sem um desenho oficial
confirmado do modelo específico. As restrições de bateria RTC do projeto
permanecem independentes deste case, que não inclui bateria de alimentação.
