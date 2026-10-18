# Gabinete NP2 — índice e estado atual

Consolidado em 18/10/2026. A revisão atual é a **V11**, criada em 10/10 e
versionada em `57b3433`, com dois cartuchos de microfone removíveis.

## Arquivos para retomar ou fabricar o protótipo

| Uso | Referência |
|---|---|
| Decisões da sessão, histórico e checkpoints Fusion | [RETOMAR-CASE.md](RETOMAR-CASE.md) |
| Montagem, dutos, juntas e limites V11 | [README V11](concept-v11/README.md) |
| Orientação das peças e preparação ABS | [Guia de impressão V11](concept-v11/GUIA-IMPRESSAO.md) |
| Montagem editável com hardware oficial | [NP2-case-v11.f3d](concept-v11/NP2-case-v11.f3d) |
| Intercâmbio CAD | [NP2-case-v11.step](concept-v11/NP2-case-v11.step) |
| Pacote de impressão | [NP2-v11-ABS-impressao.zip](concept-v11/NP2-v11-ABS-impressao.zip) |
| Distribuição de peças para mesa 350 × 350 mm | [NP2-v11-ABS-350x350.3mf](concept-v11/NP2-v11-ABS-350x350.3mf) |
| Referência oficial e alinhamento da placa | [Waveshare 7B](reference/waveshare-7b/README.md) |
| Hashes dos checkpoints e da entrega | [Manifesto SHA-256](session-2026-10-10/integrity-sha256.json) |

`concept-v11/STL/` contém seis peças principais e quatro metades dos cartuchos
MIC. Os dutos podem ser fabricados depois; sem eles o caminho acústico vedado
até as cápsulas permanece incompleto. O 3MF contém geometria e posições; o
perfil de máquina, material e suportes deve ser preparado no fatiador.

`concept-v01/` e as revisões `concept-v03/` a `concept-v10/` preservam a evolução.
`session-2026-10-10/` guarda os 27 checkpoints locais Fusion e seus manifestos,
inclusive estudos que não são peças de impressão. Scripts de criação, inspeção
e exportação ficam em [tools/cad](../tools/cad/); a sequência V11 está no README
da revisão. Scripts com `adsk` dependem do ambiente Fusion.

## Evidência e pendências

Em 18/10, os 199 arquivos do manifesto foram conferidos por presença, tamanho
e SHA-256, sem divergências. Os relatórios V11 registram inspeções CAD e malhas
fechadas; os resultados detalhados e suas limitações permanecem na revisão.

Faltam fatiamento específico, impressão física, montagem e testes de retenção,
vedação e áudio. Conferir as regiões de 0,6 mm dos dutos, os parafusos reais,
as juntas macias, os tact switches e os conectores opcionais. Térmica e energia
com gabinete final pertencem à qualificação G6 do firmware. A integridade dos
arquivos e a triagem geométrica não encerram esses ensaios.
