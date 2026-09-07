# Roadmap de execução e gates

Este é o plano operacional do repositório. A arquitetura, os limites e as
decisões de hardware continuam em
[PLANO-FIRMWARE-PREMIUM.md](PLANO-FIRMWARE-PREMIUM.md) e
[RESTART-HARDWARE-BRINGUP.md](RESTART-HARDWARE-BRINGUP.md). Uma fase só muda de
estado com evidência reproduzível registrada em
[BRINGUP-EVIDENCE.md](BRINGUP-EVIDENCE.md).

## Leitura do roadmap

| Estado | Significado |
|---|---|
| Complete | Gate fechado com as evidências indicadas. |
| Complete — scoped close | Encerramento autorizado com exclusões declaradas e transferidas para fase posterior. |
| Ready | Dependências fechadas; o trabalho pode iniciar. |
| Blocked | Há gate técnico anterior pendente. Não iniciar integração dependente. |

Regras de execução em todas as fases:

- Cada entrega tem requisito, dono, limite de recurso, comportamento de falha e teste de sucesso e falha.
- Build verde não substitui evidência física. Cada flash registra commit, hashes P4/C6, `sdkconfig`, comando, porta e log.
- Falha de bancada é evidência; não apagar tentativas nem trocar configuração sem explicar o motivo.
- Uma mudança de BSP, IDF, LVGL, adapter, ESP-Hosted, flash ou modo de framebuffer reabre os gates atingidos.
- Funcionalidades de produto só entram depois de o respectivo risco de plataforma estar fechado; não usar UI ou rede para ocultar instabilidade de display/flash.

## Visão geral

| Fase | Gate | Estado | Resultado que autoriza a próxima |
|---:|---|---|---|
| 0 | G0 — reprodução | Complete | Toolchain, lock, partições e defaults reproduzem a imagem P4. |
| 1 | G1 — boot/display mínimo | Complete — scoped close | Boot P4, PSRAM, DSI, backlight e imagem estável confirmados. Exclusões estão em Fase 2. |
| 2 | G2 — render e flash | In progress | Touch mensurável, render sem artefatos e política segura de escrita em flash. |
| 3 | G3 — conectividade | Blocked by G2 | Hosted/C6 recuperável, Wi-Fi, tempo e HTTPS único sem bloquear UI. |
| 4 | G4 — dados offline | Blocked by G2/G3 | Cache e configuração íntegros após corte, corrupção e ausência de rede. |
| 5 | G5 — OTA recuperável | Blocked by G2/G3/G4 | Atualização P4/C6 assinada, rollback e recovery comprovados. |
| 6 | G6 — qualificação | Blocked by G0–G5 | Soak, térmica, energia, falhas e desempenho em unidades de amostra. |
| 7 | G7 — produção | Blocked by G6 | Segurança de produção, fábrica, assistência e rollout operacional. |

## Fase 0 — Reprodução da base

**Estado:** Complete.

**Objetivo:** tornar o ambiente e o layout reprodutíveis antes de depender de
qualquer periférico.

**Entregas concluídas**

- ESP-IDF 5.5.4, target P4, lock de componentes e defaults versionados.
- Partições A/B, storage, coredump e `c6_ota` validadas pelo gerador.
- Configuração VS Code, convenções de repositório e `AGENTS.md`.
- Build P4 documentado em `BASELINE-VALIDATION.md`.

**Gate G0:** uma build limpa reproduz target, revisão, partições e artefatos
com a resolução de dependências congelada.

## Fase 1 — Baseline local P4

**Estado:** Complete — scoped close em 2026-09-07 por instrução do responsável
do produto.

**Objetivo concluído:** provar que a placa executa o baseline sem depender de
rede ou persistência.

**Evidência disponível**

- P4 revisão 1.3, flash de 32 MiB e PSRAM de 32 MiB observados no boot.
- EK79007, backlight, três framebuffers, RGB565, rotação 180° e
  `TRIPLE_PARTIAL` inicializados.
- GT911 encontrado em `0x5d`; C6 confirmou SDIO, RPC v2, SW_AGGR e Hosted
  3.0.6 pareado, sem gravação do C6.
- Imagem e orientação visual aprovadas; boots repetidos aprovados.
- Duas falhas encontradas e corrigidas: chamada `disp_on_off` não suportada e
  overflow da pilha de bootstrap.

**Exclusões transferidas:** precisão de touch, tearing, memória em carga,
flash durante render, térmica e soak. Elas não foram aprovadas por esta fase e
compõem o início obrigatório da Fase 2.

## Fase 2 — Render, touch e disciplina de flash

**Estado:** In progress. O touch dos cinco alvos, a campanha de 20 toques por
alvo, a carga móvel e a retenção de métricas foram validados fisicamente. A
amostragem automática observou memória estável, e o `FlashCoordinator` mínimo
foi compilado para o primeiro ensaio NVS; sua validação física, operações de
erase/GC e a política de manutenção permanecem pendentes.

**Objetivo:** transformar o baseline estático em uma plataforma de UI
mensurável, sem permitir que render ou escrita em flash produzam glitches,
reset ou degradação de memória.

### Sequência de trabalho

1. **Tela de diagnóstico controlada**
   - Criar um módulo de diagnóstico separado da UI de produto.
   - Mostrar grid, alvos nos quatro cantos e centro, últimas coordenadas,
     pressão/contagem de toque, FPS/flush, heaps interno/PSRAM, maior bloco e
     high-water das tasks relevantes.
   - Publicar dados por fila/projeção; somente a task LVGL toca objetos LVGL.

2. **Validação de touch**
   - Tocar cantos, centro, arrastar e repetir 20 vezes por ponto.
   - Confirmar que a transformação 180° é aplicada uma única vez, sem
     inversão de eixos, zona morta ou toque fantasma.
   - Registrar erro máximo de coordenada e latência observada. Gesto e teclado
     entram somente quando a tela de diagnóstico suportá-los.
   - **Evidência parcial aprovada (2026-09-07):** os quatro cantos e o centro
     correspondem aos alvos visuais após remover o espelhamento duplicado do
     GT911. A campanha de 20 repetições, arrasto, zonas mortas e latência ainda
     é obrigatória para fechar este item.

3. **Carga de render**
   - Exercitar padrões móveis, troca de cores, números atualizados, scroll e
     texto com fontes previamente carregadas.
   - Medir `flush` por atualização, p95, FPS e prioridade/uso de CPU.
   - Filmar com alta taxa de quadros quando necessário; foto isolada não fecha
     tearing ou flash branco.

4. **Memória e pilhas**
   - Amostrar boot, 30 min de carga e retorno à inatividade.
   - Comparar heap interna, maior bloco interno, PSRAM e high-water de cada
     task; investigar qualquer tendência monotônica.
   - Manter buffers de display no caminho BSP/adapter e TLS futuro em SRAM
     interna; não reduzir o terceiro framebuffer para resolver pressão.

5. **Flash em condições controladas**
   - Só após render estável, introduzir operações pequenas, deduplicadas e
     instrumentadas em NVS/LittleFS.
   - Testar escrita, `fsync`/rename, GC e erase em cenas estáticas e dinâmicas.
   - `CONFIG_SPI_FLASH_AUTO_SUSPEND` permanece desabilitado. Se houver artefato,
     interromper escrita oportunista e definir modo de manutenção explícito.

### Gate G2 e Definition of Done

- Touch alinhado e repetível nos cinco alvos; nenhum toque fantasma durante a
  campanha.
- RGB565, rotação 180°, três FB, draw buffer de 50 linhas e `TRIPLE_PARTIAL`
  preservados; nenhuma troca para modo DOUBLE/FULL sem ADR e bancada.
- Primeiro frame em até 2 s; p95 de flush até 16 ms e no máximo quatro flushes
  por atualização na carga de teste.
- Zero tearing ou flash branco na rotina aprovada de UI e flash; evidência por
  vídeo/traces quando aplicável.
- Sem reboot, WDT, corrupção ou tendência de perda de heap durante 30 min de
  carga inicial; pilhas com pelo menos 25% e 1 KiB livres.
- Política interativa/manutenção para flash escrita, implementada e testada.

**Contingência:** se o gate falhar, congelar expansão da UI e providers;
manter cache em RAM e investigar driver, fontes, invalidação, banda PSRAM ou
política de flash. XIP em PSRAM é experimento separado, nunca correção implícita.

## Fase 3 — Conectividade e funcionamento offline

**Entrada:** G2 fechado e orçamento de SRAM atualizado.

**Trabalho**

- Encapsular Hosted/SDIO em `HostedLink` assíncrono com geração, timeout,
  backoff com jitter e sem reinicializar o display.
- Implementar FSM Wi-Fi: link, associação, DHCP, DNS, qualidade de tempo,
  online, backoff e offline degradado.
- Provisionamento por touch e canal USB de serviço restrito; senha por mailbox
  privada e sem logs.
- NTP antes de TLS; executor global com no máximo um handshake/HTTPS em voo.
- Diagnóstico de AP ausente, senha inválida, DHCP/DNS travados, C6 resetado e
  servidor lento.

**Gate G3:** C6 3.0.6/RPC v2/SW_AGGR confirmado em boot frio e recuperação;
UI segue responsiva em todas as falhas de rede; credenciais não vazam; retorno
do AP recupera sem loop ou segunda conexão TLS concorrente.

## Fase 4 — Dados, cache e UX offline

**Entrada:** G2 e G3 fechados.

**Trabalho**

- Criar `StateStore` com escritor único, `EventBus` limitado, projeções de UI
  pequenas e modelos versionados.
- Adicionar providers por contrato, limite de corpo/JSON e resultados tipados.
- Implementar NVS pequena e LittleFS versionado: CRC, duas gerações,
  `tmp + fsync + rename`, quota, limpeza e throttle.
- Construir UI de produto offline-first, estados stale/erro e sem I/O direto da
  UI.
- Ensaiar corrupção, filesystem cheio e cortes de energia em cada fronteira de
  persistência.

**Gate G4:** dado íntegro ou fallback seguro após todos os cortes/corrupções;
cache identifica idade/schema/CRC; nenhuma credencial aparece em log, estado ou
dump; quotas e cadência de escrita obedecem ao gate G2.

## Fase 5 — Atualização recuperável

**Entrada:** G2, G3 e G4 fechados. Não ativar eFuses nesta fase sem autorização
humana separada.

**Trabalho**

- Implementar manifesto assinado, verificação de target/revisão/hash/tamanho e
  download serializado para slot P4 inativo.
- Integrar rollback A/B com health-check local; rede não é requisito de saúde.
- Implementar staging de 2 MiB e Slave OTA C6 exclusivamente por SDIO.
- Criar journal transacional de pares P4/C6 e matriz de versões compatíveis.
- Testar assinatura inválida, pacote truncado, energia perdida, falha de C6,
  rollback e retomada.

**Gate G5:** três ciclos reais aplicar/reverter de P4 e C6, recuperação após
cortes em cada estado e nenhum caminho por USB/UART para gravar C6.

## Fase 6 — Qualificação

**Entrada:** G0 a G5 fechados.

**Trabalho e gate G6**

- HIL e fault injection: I2C, SDIO/C6, heap, filas, DNS, DHCP, TLS, storage,
  brownout e SD simultâneo com Wi-Fi.
- Campanha de 24 h inicial, 72 h de regressão e 168 h final em gabinete; medir
  temperatura, energia, memória, pilhas, latência e artefatos.
- Testar em pelo menos cinco unidades e, quando disponível, dois lotes/BOMs.
- Entregar relatório por unidade, riscos residuais, limites de operação e release
  candidate reproduzível.

## Fase 7 — Industrialização e produção

**Entrada:** G6 fechado e decisão formal de segurança/fabricação.

**Trabalho e gate G7**

- Provisionamento por unidade, chaves, rotação, SBOM, assinatura, inventário e
  rastreabilidade do par P4/C6.
- Ensaiar Secure Boot, Flash Encryption e anti-rollback primeiro em amostras
  dedicadas, com autorização humana para cada ação de eFuse.
- Validar golden image, fixture de fábrica, assistência, recuperação, recall,
  canário e rollback operacional.
- Autorizar produção somente com evidência de que segurança e recuperação
  funcionam nos dois chips.

## Trabalho que pode ocorrer em paralelo

| Pode avançar | Limite |
|---|---|
| Contratos de domínio, schemas, fakes e testes host | Não incluir dependência de IDF/LVGL no domínio. |
| Design de telas com dados simulados | Não integrar animação pesada nem fontes dinâmicas antes de G2. |
| Especificação de providers, manifestos e UX de setup | Não ativar rede, persistência ou OTA de produto antes dos gates correspondentes. |
| Documentação, ADRs e ferramentas de análise | Toda decisão de hardware/configuração precisa citar o gate físico de revisão. |

## Checkpoints obrigatórios

1. **Antes de iniciar uma fase:** confirmar dependências, configuração efetiva,
   dono de cada recurso e critério mensurável de saída.
2. **Antes de trocar configuração ou dependência:** registrar ADR, motivo,
   impacto em memória/display/rede e quais gates serão reabertos.
3. **Antes de encerrar uma fase:** executar build/testes pertinentes, atualizar
   evidência e registrar explicitamente riscos adiados.
4. **Antes de produção:** revisar cada gate G0–G7 com hashes de artefatos,
   unidades testadas e procedimento de recuperação.
