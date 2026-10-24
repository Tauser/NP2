# Agenda Google — margem de SRAM e pilha

Campanha de 19–20/10/2026, America/Sao_Paulo. Unidade de bancada Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3 reportado como revisão 103, flash/PSRAM
32 MiB. A revisão de PCB/lote não foi reinspecionada nesta campanha. Base
`777abde7069c91cfb6aeb915891cf00bc249f9df`, com alterações no working tree.
Decisão: [ADR-095](DECISIONS.md#adr-095--recuperar-sram-para-https-da-agenda-sem-reduzir-o-piso).

## Problema e alteração

A conta já estava vinculada, mas o sync era recusado com `ESP_ERR_NO_MEM`:
aproximadamente 47.839 B internos livres e maior bloco de 23.552 B. A admissão
exigia 80 KiB de piso + 64 KiB de envelope TLS + workspace interno + formulário
OAuth de 4 KiB, e bloco contíguo de pelo menos 32 KiB.

O pool builtin LVGL de 64 KiB foi posicionado em BSS externa por fragmento de
linker; a extensão PSRAM de 128 KiB continua igual. Capacidade útil medida:
194.596 B antes/depois, com 188.168 B livres após inicialização. Draw interno,
três framebuffers, RGB565, rotação 180° e TRIPLE_PARTIAL foram preservados.

Projeções públicas, estado visual, resultados de scan, metadados de ícones e
buffers de diagnóstico ficam explicitamente em BSS externa. Credenciais,
locks, filas e pilhas permanecem internos. Corpos de provider e chunks OTA de
4 KiB são internos e transitórios, pertencem ao único worker HTTPS e são
apagados/liberados tanto em sucesso como em erro. O arquivo
`tools/check_agenda_memory_layout.py` confere a localização no ELF e verifica
que o vault OAuth permanece interno.

Somente duas opções efetivas mudaram em relação ao build anterior:
`CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` e sua opção derivada
`CONFIG_ESP_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`. Defaults versionados; nenhum
componente/lock, partição, clock ou opção TLS foi atualizado. XiP existente
permanece igual. A PSRAM extra compete por banda; isso exige qualificação de
render/latência separada e não encerra G2.

## Primeira gravação — diagnóstico da pilha

Build limpo IDF 5.5.4, target explícito `esp32p4`, em
`firmware/build/agenda-memory-20261019`, SDKCONFIG próprio. ELF confirmou
130.352 B a menos de dados estáticos internos e 122.288 B de BSS externa.
`app-flash` gravou somente a aplicação em `0x20000`, pela COM8 a 460800 baud;
hash verificado. A conta foi restaurada do NVS após esse reboot.

| Medida | Resultado |
|---|---:|
| SRAM antes da admissão Google | 176.887 B |
| Maior bloco antes do Google | 77.824 B |
| Admissão exigida | 172.936 B |
| HTTP token / Tasks / Calendar | 200 / 200 / 200 |
| Mínimo interno durante sync | 125.899 B |
| Heap após sync | 176.127 B |
| Integridade de heap | 1 |
| Itens importados | 1 |
| App loop mínimo | 2.280 B / 8 KiB |
| Rede mínimo — **insuficiente** | 112 B / 8 KiB |

O mínimo é medido pelo monitor local do heap do IDF durante a operação toda,
incluindo TLS, e não apenas por amostras antes/depois. Não há corpos, títulos,
identificadores de conta, tokens ou headers nos logs exportados. Leituras
isoladas não qualificam o pico de todos os certificados futuros.

Essa primeira imagem demonstrou a leitura real e apontou margem de pilha
inaceitável. Foi substituída por revisão com worker interno de 12 KiB e corpo
de escrita de 1 KiB no workspace interno transitório. BSS pública adicional
compensa parte desse custo; a admissão e o piso permanecem ativos.
O log inicial está em `boot-google-initial-sanitized.log` no diretório do build.

## Revisão de pilha — leitura real, anterior à calibração

O ELF dessa revisão tem 63.752 B de dados estáticos internos e 125.640 B de BSS
externa: redução interna de 133.696 B em relação à imagem anterior da agenda.
Worker de rede interno: 12 KiB; payload de escrita Google: 1 KiB transitório.
Sem warnings de compilação. A nova gravação P4 foi verificada por hash.

| Medida | Resultado |
|---|---:|
| Firmware C6 consultado via RPC | 3.0.6 |
| SRAM antes da admissão | 175.919 B |
| Maior bloco antes/depois | 83.968 B |
| Admissão exigida | 173.960 B |
| Token / refresh conhecido / Tasks / Calendar | HTTP 200 em todos |
| Mínimo interno durante sync | 123.963 B (121,1 KiB) |
| Heap após sync | 174.963 B |
| Pilha de rede mínima | 4.276 B / 12.288 B (34,8%) |
| Integridade de heap | 1 |

A leitura real preservou o piso de 80 KiB e a margem de pilha de 25%/1 KiB.
O registro é `boot-google-stack-fixed-sanitized.log`; seu conteúdo é filtrado na
captura, não um dump bruto. O teste de criação/exclusão pela UI é registrado
separadamente; não inferir esse resultado de uma leitura HTTP 200.

| Artefato anterior à calibração | SHA256 |
|---|---|
| P4 bin (3.505.360 B; 58% livre no slot de 8 MiB) | `DBDCEFBFCCF4EDEEDC84EC6A31B8AD4CEDA90EDCCC138AFF84FD1E32A636EE94` |
| ELF | `4066BF9AF7F33E3F62677F49304F76744CB38B36A8340E38228D340495AFFCA0` |
| sdkconfig efetivo | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| dependencies.lock (preservado) | `6F605BE935C36425D451A65EC2A700AEF5878D9DAB7EA1000C2721199C552707` |

## Revisão final — reserva medida e ida/volta Google

Uma tentativa posterior foi recusada pelo envelope provisório de 64 KiB,
apesar do mínimo real acima de 120 KiB. Com o pico medido, reservar 48 KiB
para TLS: 51.956 B de pico total menos 22.408 B de workspace = 29.548 B
restantes observados, incluindo atividade concorrente. O envelope de 49.152 B
fica mais de 19 KiB acima disso, além do orçamento separado de formulário.
Piso obrigatório de 81.920 B e maior bloco mínimo de 32.768 B preservados.
A admissão final exige **157.576 B**; testes host conferem imediatamente
abaixo/acima do limiar e a fronteira de bloco contíguo sem iniciar HTTP quando
há insuficiência. Essa calibração vale para a campanha de laboratório, sem
qualificar toda cadeia/cipher futura ou concorrência máxima.

A aplicação final foi gravada e verificada por hash, restaurando dois eventos
do filesystem e a mesma conta. O segundo evento foi criado pelo responsável
na UI com o título de teste combinado. Após reboot, houve GET individual
HTTP 200 dos dois IDs conhecidos e listagem Calendar com os dois itens:
isso confirma a ida/volta do evento criado no painel. A janela do POST original
não foi capturada; não se atribui um status de POST que não foi observado.

O responsável selecionou somente o evento de teste, excluiu, confirmou e
sincronizou na UI. A captura contínua registrou DELETE Calendar **HTTP 204**,
resultado de sync OK, sem corpo nem identificador de evento nos logs.
Em 214,1 s de captura, a agenda voltou a um item, com `pending=0`, `linked=1`,
`sync_busy=0` e `sync_result=0`: a limpeza final foi salva localmente.
O leitor serial foi encerrado após a verificação, liberando COM8.

| Medida final | Leitura | Exclusão |
|---|---:|---:|
| Heap antes da admissão | 176.087 B | 175.691 B |
| Mínimo interno | 124.207 B | 123.767 B |
| Heap após sync | 174.919 B | 174.931 B |
| Maior bloco após sync | 100.352 B | 100.352 B |
| Pilha de rede mínima | 4.216 B (34,3%) | 4.216 B (34,3%) |
| Integridade | 1 | 1 |
| Resultado | 0 | 0 |

O app_loop ficou com 2.284 B de margem (27,9% de 8 KiB). C6 respondeu versão
3.0.6 via RPC. O ELF manteve a redução estática de 133.696 B (130,6 KiB) e
o vault interno. O arquivo `boot-google-sanitized.log` contém a captura final;
`write-google-sanitized.log`/`roundtrip-google-sanitized.log` preservam a
tentativa recusada anterior. Não houve alteração de firmware após essa gravação.

| Artefato final calibrado | SHA256 |
|---|---|
| P4 bin, 3.505.360 B / 58% livre | `3648E14AAA4C98E8C3D4EE659D8CEB3E06816CC92F88669F550647F780F00E45` |
| ELF | `1C9444876EEE5C822C2B330B14D9958062EA789159EB8AB389EA09828EAC8896` |
| sdkconfig efetivo | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Fontes públicos, `sources.sha256` (48 arquivos) | `A1832AB4398B6CA552B282D68599A03D891F50E03C8942E8385C3EADF4619DC3` |

O manifesto contém somente fontes públicos alterados/novos; exclui o diretório
local de credenciais e artefatos. O binário existente contém configuração
privada de desenvolvimento e não deve ser publicado. Os hashes são evidência
local, não uma declaração de release. Nenhum segredo foi exportado.

## Testes host e limite conhecido

Passaram: domínio/codec da agenda, barramento (incluindo 8.000 snapshots
concorrentes), sync Google com HTTP simulado, vault, persistência da agenda
com fsync/rename/CRC/falhas, lifecycle LVGL de 100 ciclos e teste dos buffers
transitórios internos (sucesso, erros, falha de alocação e apagamento).

O teste antigo de filesystem cheio precisou acompanhar o schema atual e os
tipos públicos para voltar a compilar. Ele passa preenchimento normal e
repetição, mas falha no caso de snapshot de domínio com assert de LittleFS
`pcache->block == LFS_BLOCK_NULL`, em `lfs.c:263`. A mesma falha foi reproduzida
com `flash_coordinator.c` extraído do HEAD anterior, usando o mesmo harness
atualizado, portanto não é atribuída ao posicionamento BSS. Continua aberta;
não desabilitar asserts nem declarar esse gate concluído. Nenhum ensaio de
filesystem cheio, corrupção ou corte de energia foi executado na placa.

## Reprodução

```powershell
./tools/run_agenda_host_test.ps1
./tools/run_app_event_bus_host_test.ps1
./tools/run_google_agenda_sync_host_test.ps1
./tools/run_agenda_ui_host_test.ps1
./tools/run_network_transient_buffers_host_test.ps1
python tools/check_agenda_memory_layout.py firmware/build/agenda-memory-20261019/np2_p4.elf --baseline firmware/build/agenda-20261019/np2_p4.elf
```

Build: `idf.py -DIDF_TARGET=esp32p4
-DSDKCONFIG=D:/Projetos/NP2/firmware/build/agenda-memory-20261019/sdkconfig
-B build/agenda-memory-20261019 build`, em `firmware/`, com o ambiente IDF
5.5.4 exportado. Para gravar usar o mesmo comando com
`-p COM8 -b 460800 app-flash`. Conferir a tabela antes:
SHA256 `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
Captura limitada: `tools/capture_agenda_diagnostics.py`, sem reset, com
whitelist de campos fixos. Não exportar logs brutos/dumps.

C6 não foi gravado; a USB continua exclusivamente no P4. Hash da imagem
instalada no C6 não foi obtido nesta campanha e não é inferido do lock ou de
um staging antigo. Gates de par exato, soak, energia e produção continuam
abertos. Também ficam pendentes matriz física completa de alarmes/som e
interação/render sob carga; build e duas conexões não substituem esses gates.

## 20/10/2026 — confirmação de salvamento no editor

Após a gravação durável, a lista recebia `persistence_pending=0`, mas o editor
mantinha o texto definido no ACK do comando. O editor agora acompanha a mesma
projeção: mantém pendência durante a gravação, mostra falha/retry quando houver
erro e exibe “Alteração salva no painel.” somente com pendência encerrada e
`storage_result=ESP_OK`. Sem confirmação de storage, não declara sucesso.
Uma nova interação assume a mensagem para preservar validação, confirmação
de exclusão e instruções USB durante atualizações posteriores de estado.

`tools/run_agenda_ui_host_test.ps1` passou com regressão de pendência repetida,
falha, retry, resultado ainda desconhecido e confirmação; inclui preservação
da confirmação de exclusão e 100 ciclos LVGL sem perda após warm-up. Essa
verificação exercita os rótulos reais com LVGL no host. Não se atribui uma
inspeção visual física dos novos rótulos à captura serial.

Build incremental sobre o build limpo desta campanha, IDF 5.5.4 e target
`esp32p4`; sem warnings novos. App-flash COM8/460800 gravou 3.505.472 B em
`0x20000`, hash verificado. Boot restaurou um item e a conta vinculada, com
`ready=1 pending=0 sync_busy=0 sync_result=0`. Configuração e partições mantêm
os hashes da revisão anterior. O layout ELF continua com 63.752 B internos,
125.640 B de BSS externa e redução estática de 133.696 B.

Na captura desta imagem, o C6 respondeu 3.0.6. A sincronização automática
retornou HTTP 200 para token, Calendar e Tasks, com resultado final 0,
`min_internal=124043`, maior bloco 102.400 B, pilha de rede mínima de 4.208 B
e `integrity=1`. Não houve criação/exclusão de outro evento para testar texto.
A captura encerrou normalmente após 100 s, liberando COM8. Em 93,9 s,
`pending=0 linked=1 sync_busy=0 sync_result=0`; nenhum fault foi capturado.

| Artefato após correção do feedback | SHA256 |
|---|---|
| P4 bin, 3.505.472 B / 58% livre | `245F3200E987BA0F86C2139B2184F69E25D077D4E1B5A5942E8C333FFC7FC61E` |
| ELF | `27712ABF2B5F2CC77119D0FD83BDCC7D6F0C1C248C64D51339C22CF4AD0E08F9` |
| Fontes públicos, `save-feedback-sources.sha256` (48 arquivos) | `4A73594AEBF8EDC1AD8C4C6715065E442CABC60078334FFE7ECD8EF62464399D` |

Artefatos locais ignorados no mesmo diretório: `app-flash-save-feedback.log`,
`boot-save-feedback-sanitized.log` e o novo manifesto; bin/ELF anteriores
preservados com sufixo `.before-save-feedback`. A tabela histórica acima
continua identificando a imagem usada no roundtrip e DELETE reais. O C6 não
foi regravado; gates e falha preexistente de filesystem cheio continuam abertos.
