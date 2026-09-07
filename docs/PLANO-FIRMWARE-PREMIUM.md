# Plano de arquitetura e execução — smart panel ESP32-P4/C6

**Estado:** proposta de arquitetura; nenhum firmware implementado por este documento.
**Pesquisa:** 06–07/09/2026, considerando a bancada registrada em 06/09/2026.
**Plataforma:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 revisão de silício v1.3, C6 por SDIO.
**Referência obrigatória, lida integralmente:** [RESTART-HARDWARE-BRINGUP.md](RESTART-HARDWARE-BRINGUP.md).

## 1. Resumo executivo, produto e critérios de evidência

A direção principal é um firmware modular em C++ sobre ESP-IDF 5.5.4, com domínio testável sem hardware, estado central de escritor único, UI LVGL de proprietário único, rede HTTPS inteiramente serializada e persistência coordenada. O C6 permanece dedicado à conectividade ESP-Hosted; domínio, TCP/IP do perfil MCU convencional, TLS, armazenamento, supervisão e UI ficam no P4. Não adotar network split/offload de sockets neste início.

O produto deve iniciar uma interface local utilizável sem internet, preservar os últimos dados íntegros com indicação de idade, responder a toque independentemente da recuperação do rádio e conseguir reverter uma aplicação defeituosa. Clima, mercado, relógio e configuração são domínios iniciais de referência. Áudio, câmera, BLE, Matter, automações e integrações adicionais são extensões futuras com orçamento próprio; não ficam implicitamente habilitados pelo BSP.

**Bloqueadores de produção já identificados:** glitches de render ainda não encerrados; flash sem auto-suspend no caminho validado; recuperação física de SDIO/rede ainda não qualificada; OTA assinada e rollback conjunto P4/C6 não ensaiados; proteção de credenciais em repouso e procedimento irreversível de fabricação ainda pendentes. A arquitetura reduz riscos, mas não transforma essas pendências em fatos resolvidos.

Rótulos usados neste plano:

| Rótulo | Interpretação |
|---|---|
| **B** | Fato registrado na referência de bancada; não repetido nesta atividade de planejamento. |
| **D** | Fato encontrado em documentação oficial ou no projeto mantenedor, com link. Não implica validação física. |
| **R** | Recomendação de arquitetura, orçamento ou critério de aceite proposto. Deve virar requisito rastreável. |
| **H** | Hipótese técnica ou capacidade não comprovada no conjunto exato. Exige experimento antes de uso em produção. |

Salvo indicação B/D, valores novos, interfaces e políticas abaixo são **R**. Métricas históricas descrevem a carga anterior, não a futura UI. Nenhum limite proposto equivale a uma medição realizada.

**Princípios de aceite:** ausência de internet é estado normal; falha recuperável não provoca `abort()`; falhas permanentes não geram reboot infinito; integridade e capacidade de recuperação precedem frequência de atualização; nenhum caminho aceita crescimento ilimitado de memória, filas, retries ou logs.

## 2. Inventário, versões e configuração imutável

### Hardware e restrições práticas

| Item | Especificação e regra |
|---|---|
| P4 | **B:** revisão v1.3, flash 32 MiB, PSRAM 32 MiB. Target `esp32p4`, imagem com revisão mínima v1.0 e seleção de revisões inferiores a v3. Separar revisão de silício, PCB e lote no inventário de fábrica. |
| Flash | **B:** GD JEDEC `0xC84019`; auto-suspend produziu boot loop. Não inferir suporte pelo fabricante ou por outra peça com nome semelhante. Congelar identificação da peça/lote e características elétricas. |
| LCD | **B:** EK79007, MIPI-DSI duas lanes, 1024×600, RGB565, rotação 180° no adapter. Manter timings, clocks, alimentação D-PHY e sequência do BSP usado, sem overclock. |
| Touch | **B:** GT911; I2C SCL GPIO8, SDA GPIO7, sem IRQ neste perfil. Polling e transformação de coordenadas no mesmo pipeline do display. |
| Backlight | **B:** GPIO32; somente após primeiro frame apresentado. BSP já inicializa brilho: não repetir `bsp_display_brightness_init()`. |
| C6 | **B:** flash própria 4 MiB. SDIO 4-bit: CLK18, CMD19, D0=14, D1=15, D2=16, D3=17; reset GPIO54 **ativo baixo** no perfil 3.0.6. Não usar defaults H2/SPI nem GPIO7–12 herdados. |
| I2C compartilhado | **B:** GT911/ES8311. HAL mantém um barramento e lock semântico; áudio futuro não pode interromper polling por operações longas. |
| Cartão SD | **B:** CLK43, CMD44, D0..D3=39..42. É opcional, nunca requisito para boot ou rollback. Concorrência com Hosted depende de teste do controlador/slots, além de pinos diferentes. |
| RTC e bateria | **B:** RTC interno e ML1220 recarregável; nunca CR1220. Retenção, deriva e perda completa de alimentação precisam de caracterização. |
| USB | **B:** USB Serial/JTAG e CH343 chegam ao P4. Console primário USB Serial/JTAG para entrada; console secundário não entrega `stdin`. Nenhuma USB do P4 grava diretamente o C6. |
| Alimentação/térmica | **H:** margem real com backlight, transmissão Wi-Fi, periféricos e gabinete. Medir na alimentação final; não presumir que cabo USB de bancada representa produto. |

A [wiki Waveshare](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B) confirma a família com LCD 1024×600, memórias de 32 MB, C6 e Wi-Fi de 2,4 GHz. Ela também descreve terminal UART do C6; isso **não altera a rota autorizada do produto**, exclusivamente Slave OTA via SDIO. Procedimentos genéricos e defaults do fabricante não substituem as correções B acima.

### Baseline único de desenvolvimento

| Componente | Fixação | Evidência/decisão |
|---|---|---|
| ESP-IDF P4 e C6 | **5.5.4**, tag e submódulos fixados | B: compilação e placa. D: [release oficial](https://github.com/espressif/esp-idf/releases/tag/v5.5.4). |
| BSP Waveshare | **3.0.1** | B: placa. D: [BSP](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.1/readme) usa adapter ~0.6. |
| LVGL | **9.5.0** | B: requerido pelo perfil BSP resolvido. D: [release LVGL](https://github.com/lvgl/lvgl/releases/tag/v9.5.0) confirma versão publicada. |
| esp_lvgl_adapter | **0.6.4** | B: `TRIPLE_PARTIAL`. D: [changelog](https://components.espressif.com/components/espressif/esp_lvgl_adapter/versions/0.6.4/changelog?language=en) corrige contagem de buffers a 180° em DOUBLE_FULL/DOUBLE_DIRECT; não autoriza trocar o modo de bancada. |
| ESP-Hosted P4 e C6 | **3.0.6 / 3.0.6** | B: RPC v2 e SW_AGGR negociados. D: [changelog Hosted](https://components.espressif.com/components/espressif/esp_hosted/versions/3.0.6/changelog?language=en) documenta RPC v2 e requisito de patch para SW_AGGR. |
| esp_wifi_remote | **1.6.4** | B: compilado e conectado; D: [wrapper oficial](https://components.espressif.com/components/espressif/esp_wifi_remote/versions/1.6.4/readme). |
| LittleFS | **joltwallet/littlefs 1.20.4** | B: baseline de cache; D: [componente mantido por terceiro](https://components.espressif.com/components/joltwallet/littlefs/versions/1.20.4/readme). Não chamar esse port de componente oficial Espressif. |
| EK79007, GT911, codec e transitivas | Versões exatas do lock a resolver na fase 0 | O guia não informa todos os números. Não inventá-los; arquivar resolução e hashes antes do primeiro firmware. |

Remover dependência direta de `esp_lvgl_port`. Fixar versões exatas, sem `*`, `^` ou atualização automática no manifesto do produto. Arquivar `dependencies.lock`, toolchain, hashes dos fontes, patch IDF, BSP e configurações efetivas. O diretório atual contém o guia, sem um lock a reutilizar. A compatibilidade relatada em B existe, mas a reprodução local do novo projeto é um gate futuro.

O perfil histórico IDF 5.5.4/BSP 1.0.4/LVGL 9.4.0/adapter 0.5.3/Hosted 2.12.11/remote 1.6.2 fica arquivado para diagnóstico. Ele **não é um rollback automático seguro** com C6 3.0.6. Manter firmware, configuração e imagem C6 de cada conjunto, inclusive a versão 2.12.9 efetivamente anterior à migração relatada.

### Configurações que a CI deverá conferir no resultado efetivo

Carregar em `sdkconfig.defaults` versionado e verificar que os símbolos existem e mantiveram os valores após resolução; `sdkconfig` gerado é evidência de build, não fonte de verdade:

```text
CONFIG_IDF_TARGET="esp32p4"
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=32768
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_DRAW_BUF_ALIGN=64
CONFIG_LV_DRAW_BUF_STRIDE_ALIGN=64
CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y
CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
CONFIG_ESP_CONSOLE_UART_DEFAULT=n
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_ESP_HOSTED=y
CONFIG_ESP_HOST_WIFI_ENABLED=n
CONFIG_ESP_HOSTED_HOST_FEAT_WIFI_EXT_ITWT=n
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW=y
CONFIG_SPI_FLASH_AUTO_SUSPEND=n
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

Validar também transporte 4-bit e todos os GPIOs efetivos; o nome do perfil da placa sozinho não prova pinout. Preservar funções necessárias de flash em IRAM. Não reutilizar `CONFIG_ESP_HOSTED_ENABLED` nem `CONFIG_ESP_HOSTED_ENABLE_ITWT`; foram substituídos. `ESP_HOST_WIFI` junto de Wi-Fi remoto já causou erro de versão do adaptador net80211.

No ambiente normal, usar exportação do IDF; em chamada direta de `idf.py`, assegurar `ESP_IDF_VERSION=5.5`, conforme B, para carregar Kconfig do Wi-Fi Remote. Não confundir essa variável com o pin exato do SDK 5.5.4.

Antes de compilar o **C6**, aplicar `python <esp_hosted>/tools/eh.py patch-idf --idf-path <idf-5.5.4>` e registrar diff: em `components/esp_driver_sdio/src/sdio_slave.c`, apenas substituir a validação `len > 0 && len <= 4092` por `len > 0`, com a mensagem correspondente. Usar instalação IDF controlada; CI falha se patch faltar ou houver alterações adicionais não revisadas. Confirmar no C6:

```text
CONFIG_ESP_HOSTED=y
CONFIG_ESP_HOSTED_CP=y
CONFIG_ESP_HOSTED_CP_FOR_MCU=y
CONFIG_ESP_HOSTED_CP_RPC_V2=y
CONFIG_ESP_HOSTED_CP_FEAT_WIFI=y
CONFIG_EH_TRANSPORT_CP_SDIO=y
CONFIG_EH_TRANSPORT_CP_SDIO_MODE_SW_AGGR=y
```

Novas versões do SDK/componentes abrem uma campanha de compatibilidade; não entram porque o registry diz “Latest”. O changelog Hosted menciona outro SDK default; a receita comprovada continua 5.5.4 com patch. A documentação atual do adapter detecta capacidades/callbacks por símbolos: arquivar os warnings e conferir o caminho de conclusão de framebuffer na build exata, sem remover avisos para fazer o gate passar.

## 3. Arquitetura, interfaces e ownership

### Camadas e dependências

| Camada | Módulos propostos | Responsabilidade e proibição |
|---|---|---|
| Composição | `app_bootstrap` | Criar dependências e ligar serviços explicitamente; sem lógica de negócio nem singletons com trabalho em construtor. |
| Domínio | `models`, `policies`, `reducers` | Tipos limitados, validação, estado e políticas; sem includes ESP-IDF/LVGL. |
| Aplicação | `StateStore`, `EventBus`, serviços de clima/mercado/configuração/tempo | Processar intenções/resultados; decidir refresh, stale e navegação. Não fazer I/O bloqueante. |
| Apresentação | `UiDispatcher`, view-models por tela, componentes LVGL | Consumir projeções pequenas e emitir intenções. Sem acesso direto à rede, NVS ou hardware. |
| Infraestrutura | `RequestOrchestrator`, providers, `PersistenceService`, `UpdateCoordinator`, `CredentialVault` | Implementar portas do domínio, deadlines, limites e tradução de erros. |
| Plataforma | `DisplayHal`, `TouchHal`, `BoardI2c`, `HostedLink`, `WifiHal`, relógio, flash/SD | Encapsular IDF/BSP, ownership de handles, callbacks e diferenças de placa. |
| Supervisão | `HealthSupervisor`, métricas e diagnóstico | Observar progresso, degradar/reiniciar recursos com orçamento. Não contornar limites dos módulos. |

Direção de dependência: apresentação/aplicação usam contratos; infraestrutura implementa contratos; HAL conhece IDF; domínio não conhece HAL. A composição injeta dependências por construtor. Preferir C++ com RAII para handles, recursos de duração explícita e `Result<T>`/`Status`; evitar exceções, RTTI desnecessário, alocação implícita em hot paths e `static std::function` global. Não criar framework de plugins dinâmicos.

### Contratos mínimos a especificar antes de programar

| Porta | Entrada → saída; contrato |
|---|---|
| `IClock` | Tempo monotônico para deadlines e UTC com qualidade/idade/incerteza; mudança de NTP não muda timers. |
| `IConnectivity` | Comandos assíncronos e eventos com geração do enlace; não expor ponteiros do driver. |
| `IRequestScheduler` | Descritor limitado: domínio, prioridade, deadline, idempotência, limite do corpo e cancelamento → resultado tipado. |
| `IProvider<T>` | Bytes limitados → modelo validado; sem objetos JSON atravessando a fronteira. |
| `ICache<T>` | Leitura de última geração válida e proposta de atualização → resultado de persistência independente da atualização em RAM. |
| `ICredentialVault` | Handle opaco/transferência privada → uso transitório pelo provisionador; sem getter para UI/telemetria. |
| `IUpdateService` | Manifesto autenticado e política de compatibilidade → estado de transação e resultado, sem pointer para partição na UI. |
| `IHealthSource` | Contador de progresso e deadline esperado → saudável/degradado/falho, sem alimentação artificial de watchdog. |

Erros distinguem timeout, cancelado, sem memória, payload inválido/excedido, sem hora confiável, falha TLS, indisponibilidade transitória, incompatibilidade e corrupção. Todo callback externo valida dados e retorna rapidamente; serviços não recebem `esp_err_t` cru como regra de negócio.

### Estado e passagem de dados

`app_loop` é o único escritor do `StateStore`. Cada domínio possui `revision`, status, qualidade, `last_success`, idade e código de erro sanitizado. `NetworkState`, `ClockState`, `StorageState`, `UpdateState` e `HealthState` são independentes: ter IP não significa ter DNS, UTC válido ou backend disponível.

Providers publicam resultados com `request_id` e geração da conexão. O app descarta respostas de geração antiga ou configuração substituída. UI recebe somente campos visíveis e revisões; nunca copia o `AppState` completo nem um view-model grande na pilha. Usar projeções de tamanho fixo e buffers prealocados com vida útil explícita. Alteração de estado não precisa equivaler a redesenho imediato.

`EventBus` é entrega assíncrona limitada, não uma cadeia síncrona de callbacks arbitrários. Eventos de progresso podem ser coalescidos; resultados de comandos têm confirmação e identificador. Perda de evento de transporte revoga readiness por estado/geração consultável e flag de falha persistente em RAM. Segredos nunca entram no barramento.

Fluxo normal: toque → intenção → app_loop → mudança de domínio/comando assíncrono → resultado → reducer → revisão → UiDispatcher → objetos LVGL. O ACK visual de toque é local; a conclusão de rede é outro estado, sem bloquear navegação.

## 4. Concorrência, tarefas, filas e supervisão

### Mapa inicial de tarefas do produto

Prioridades FreeRTOS abaixo são **pontos de partida**, não garantias temporais. Números maiores têm precedência. Não rebaixar tasks internas de IDF/Hosted para caber nesta tabela.

| Tarefa | Prioridade | Pilha inicial interna | Responsabilidade |
|---|---:|---:|---|
| `lvgl_task`, criada pelo adapter | 4 | 16 KiB | Única dona dos objetos LVGL, timers e input; nenhuma segunda task chama `lv_timer_handler`. |
| `app_loop` | 3 | 8 KiB | Redutores, comandos, projeções, política de domínio; sem operações bloqueantes. |
| `https_worker` | 3 | 8 KiB | Único proprietário de conexão HTTPS; providers, OTA e uploads de diagnóstico disputam o mesmo executor. |
| `link_worker` | 3 | 4 KiB | Inicialização/recuperação Hosted que pode bloquear ~21 s; não é a lista de todas as tasks internas SDIO. |
| `health_supervisor` | 5 | 4 KiB | Acorda a cada 250 ms, trabalho pequeno e bloqueia; nunca executa recuperação longa diretamente. |
| `storage_worker` | 1 | 6 KiB | Persistência serial e GC aprovado pelo coordenador de flash. |
| `usb_setup` | 2 | 4 KiB | Entrada local limitada, mailbox privada e diagnóstico sanitizado. |
| `update_worker` | 2 | 6 KiB, sob demanda | Orquestra estado OTA/Slave OTA, sem criar outro cliente TLS. |

São 50 KiB permanentes mais 6 KiB transitórios de pilhas de aplicação. Contabilizar separadamente tarefas de timer, event loop, TCP/IP, drivers, Hosted RX/TX/RPC, idle e draw workers eventualmente criados. Obter o mapa real da build; não assumir que o BSP cria apenas uma task.

Recomendação inicial de afinidade: LVGL no core 1; controle de enlace/rede no core 0; app/supervisor/armazenamento sem afinidade quando seguro. Driver e callbacks seguem requisitos do componente. Comparar com scheduler livre antes de congelar. Nenhuma divisão de core elimina a contenção MSPI ou as interrupções de flash. Desabilitar paralelismo de draw adicional no início; habilitar apenas com ganho medido e orçamento de pilhas.

### Filas e backpressure

| Canal | Capacidade inicial | Saturação |
|---|---:|---|
| Intenções UI → app | 32, nunca <16 | Coalescer slider/refresh repetido; comando discreto recusado retorna ocupado; não sumir silenciosamente. |
| Eventos/resultados → app | 32 | Payload ≤128 B ou handle de pool; completions têm reserva própria/ACK. Overflow incrementa métrica e força ressincronização. |
| Projeções app → UI | Uma versão pendente por domínio | Substituir pela mais recente; UI não precisa desenhar todos os estados intermediários. |
| Requests | 16, máximo um em voo global | Dedupe por domínio/chave; remover vencidos; reservar vaga para operação de sistema. |
| Persistência | 8, proposta por domínio | Latest-wins; credenciais/transação OTA têm ACK e reserva, sem descarte silencioso. |
| Credenciais | Mailbox privada de 1 item | Rejeitar novo envio enquanto ocupado, limpar buffers no consumo/cancelamento. |

Nenhuma fila carrega corpo HTTP, senha ou view-model inteiro. Handles usam pool limitado, dono único e geração; timeout/cancelamento também libera recursos. Timers fazem apenas notificação. IRQs usam apenas APIs FromISR e dados permitidos; nunca parse, filesystem, logs formatados extensos ou LVGL de aplicação.

### Mutexes e deadlines

UI é single-owner, dispensando locks de aplicação para objetos LVGL. Respeitar locks internos do adapter na inicialização; depois de iniciar a task, agendar toda mudança por seu contexto. StateStore usa publicação de projeção protegida por lock curto ou troca de buffers, sem retenção durante render.

I2C possui mutex com herança de prioridade: apenas transação/sequência indivisível, timeout inicial 20 ms e fatias que preservem polling; medir/reduzir. Falha de touch não bloqueia estado ou rede. Drivers SDIO/SDMMC preservam sincronização oficial; não sobrepor mutex genérico ao transporte.

O coordenador de flash concede permissão de operação por mensagem. Não segurar mutex LVGL, I2C ou StateStore enquanto espera flash, TLS ou SDIO. Não aninhar locks entre domínios. Caso excepcional requer ordem documentada e teste de deadlock. Se um lock curto exceder 1 ms sob carga, investigar; não aumentar prazo sem medir.

Timeout de request é end-to-end e inclui DNS/conexão/TLS/corpo. Cancelamento precisa atingir a operação bloqueada por timeout do cliente; invalidar um token sozinho não interrompe um socket preso. Threads órfãs não são solução para timeout.

### Watchdogs e recuperação

Manter IWDT e TWDT do IDF; validar conforme [watchdogs P4 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/wdts.html). Começar TWDT em 10 s para caminhos cooperativos, sem inserir o worker que pode bloquear 21 s em uma exigência impossível. O supervisor observa esse worker por deadline próprio inicial de 30 s; refatorar ou ajustar conforme comportamento real. Não desabilitar IWDT nem alimentar TWDT de outra task para ocultar travamento.

Heartbeats significam trabalho/progresso ou espera legítima com prazo. Supervisor detecta app parado por 1 s e UI parada por 500 ms fora de manutenção, coleta evidência em RAM e aciona recuperação controlada; são metas a aferir, distintas do TWDT. Espera legítima de rede não invalida saúde local.

Escalonamento: cancelar operação → recriar cliente/recurso afetado → recuperar enlace com backoff → modo offline persistente. Limite recomendado: até três ciclos completos de reset C6 por dez minutos; depois pausa de cinco minutos ou tentativa explícita do usuário. Reboot P4 fica para falha local irrecuperável, nunca apenas AP ausente. Três boots defeituosos em dez minutos levam a modo seguro, usando informação RTC quando íntegra e evidência persistente limitada quando necessário; não gravar contador em flash a cada loop.

## 5. Memória, DMA e orçamento

### Regras de alocação

**B:** cada framebuffer RGB565 tem 1.228.800 B. Três totalizam **3.686.400 B = 3,515625 MiB**, sem contar metadados, alinhamento e buffers auxiliares. O draw buffer de 50 linhas usa **102.400 B = 100 KiB** em SRAM interna. Stride de 2048 B e buffers devem respeitar alinhamento de 64 B e requisitos do driver.

Alocar framebuffers pelo caminho BSP/adapter apropriado, não via `malloc` genérico nem pool de UI. A política `MALLOC_ALWAYSINTERNAL=32768` não garante SRAM para um corpo HTTP de 48 KiB: TLS, corpo, parser e pilhas exigem alocação explícita com capabilities internas. Falha retorna erro; não faz fallback oculto para PSRAM.

Descritores DMA/ISR, sincronização, pilhas críticas e dados usados com cache indisponível ficam internos conforme requisitos do periférico. Framebuffers PSRAM acessados pelo DMA do display são exceção específica suportada pelo caminho do driver, não evidência de que qualquer DMA aceite qualquer ponteiro PSRAM. Driver/adapter são donos da coerência de cache e de quando um framebuffer pode ser reutilizado. Callback de cópia concluída não equivale automaticamente a framebuffer liberado pelo scanout.

TLS e parsing JSON ficam internos, com arenas limitadas. Parsing de tokens/campos relevantes é preferível a árvore JSON completa. Limitar profundidade inicial a 8, quantidade de itens por schema, strings e números; não aceitar compressão HTTP na primeira versão. Cancelar corpo chunked que ultrapasse 49.152 B; não confiar apenas em Content-Length. Um byte extra deve produzir erro, não dado truncado.

### Orçamento inicial a fechar em medição

| Recurso | Reserva/teto inicial R | Observação |
|---|---:|---|
| FB PSRAM | 3,516 MiB + overhead medido | Obrigatório, não otimizar removendo terceiro buffer. |
| Assets/fontes quentes PSRAM | Até 2 MiB | Subsets carregados previamente; medir contenção MSPI. |
| Objetos/cache gráfico PSRAM | Até 2 MiB | Sem decodificação ilimitada ou imagens de resolução externa arbitrária. |
| Cache normalizado em RAM PSRAM | Até 512 KiB | Sem credenciais; não manter todos os históricos do filesystem. |
| Reserva PSRAM livre | ≥8 MiB | Meta que não substitui disponibilidade interna/contígua. |
| Draw interno | 100 KiB | Confirmar que não existe segundo draw buffer oculto de igual tamanho. |
| Pilhas de aplicação internas | 50 KiB + 6 KiB OTA | Além das pilhas de sistema. |
| Filas, estado e projeções internas | ≤24 KiB | Mensagens pequenas; limites verificáveis. |
| Corpo HTTP interno | ≤48 KiB | Pico temporário, contabilizado junto de TLS e parser. |
| Arena JSON interna | ≤24 KiB | Excesso falha de forma recuperável. |
| TLS interno | Envelope inicial ≤96 KiB | Hipótese de orçamento, depende de certificado, cipher e SDK; medir, não forçar fragmentos incompatíveis. |
| lwIP/Hosted/pilhas sistema/metadados | Medição obrigatória | Congelar configuração de pools; não usar toda memória restante automaticamente. |
| Heap interno livre | **B: >80 KiB em regime; R: ≥96 KiB também no pior pico** | Mínimo histórico serializado foi 179 KiB na carga anterior. |

Esta tabela **não comprova que o firmware cabe**. SRAM anunciada inclui usos estáticos, IRAM/cache e reservas; não é heap livre. Na fase 1 construir equação a partir de linker map e heap por capabilities: estáticos + pilhas + pools + draw + pico TLS/corpo/parser + margem. Não somar pools contabilizados duas vezes. Amostrar boot, primeira conexão, maior cadeia TLS, navegação, GC, download OTA e reconexão.

Admissão de HTTPS exige heap livre suficiente para pico remanescente medido mais 96 KiB e maior bloco contíguo suficiente para a maior alocação prevista. Se não houver margem, adiar request, liberar caches opcionais e manter UI offline. Nunca resolver insuficiência baixando silenciosamente o piso de 80 KiB. OTA trabalha em streaming com chunk inicial de 4 KiB, podendo haver sobreposição temporária documentada durante gravação; não alocar a imagem de 8 MiB em SRAM.

Pilha: aferir high-water mark em **bytes**, respeitando unidades da API IDF usada. Gate inicial: livre ≥25% e ≥1 KiB em cada pilha, incluindo caminhos de erro. Ativar stack overflow checks, heap poisoning em diagnóstico e inspeção de integridade em pontos seguros. Sem cópias grandes na pilha de render, recursão de JSON ou VLA.

Banda de leitura pura do display é `1.228.800 × Hz` B/s: **73,728 MB/s se 60 Hz**, hipótese de cálculo, não taxa medida. PPA, reparo de regiões, CPU e PSRAM somam tráfego. Fixar clocks do baseline, medir taxa real e underrun; capacidade PSRAM de 32 MiB não é garantia de banda.

## 6. UI premium, tearing e coordenação de flash

Criar painel com `bsp_display_new_with_handles()`, registrar no `esp_lvgl_adapter` como MIPI, selecionar `ESP_LV_ADAPTER_ROTATE_180` e `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL`, manter três FB, RGB565 e draw parcial de 50 linhas. Registrar GT911 antes da task LVGL. Aplicar exatamente uma transformação `x=width-1-x`, `y=height-1-y`; confirmar que nenhuma camada duplica a rotação do touch. Ligar backlight após conclusão real do primeiro frame. Não usar MADCTL, mirror ou swap_xy como substitutos.

Preservar callbacks padrão do adapter; qualquer externalização exige provar notificação de framebuffer completo e liberação sem corrida. O [driver DSI do IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html) documenta o caminho do periférico; a integração efetiva deve seguir a versão fixada do adapter.

Navegação principal persistente, hierarquia curta e comportamento uniforme de voltar. Telas: início/resumo, detalhes por domínio, conectividade, configuração e atualização/diagnóstico. Estado offline não abre modal repetitivo. Ações mostram feedback imediato e resultado posterior. Campos e teclado virtual devem continuar operáveis com C6 ausente.

Design system com tokens de espaço, cor, tipografia, estados de erro e foco. Alvos de toque inicialmente ≥48×48 pixels, espaçamento adequado e contraste de texto ≥4,5:1 como critério de design; validar tamanho físico com usuários no painel real. Não depender só de cor. Modos de brilho, texto ampliado dentro do orçamento e movimento reduzido. Não prometer leitor de tela sem suporte definido.

Reutilizar widgets, fontes bitmap subsetadas incluindo português, dígitos e símbolos previstos; não gerar glyph por FreeType durante atualização frequente. Um label por dígito quando necessário para limitar invalidação, sem quebrar representação semântica do valor. Formatar strings somente quando mudarem; atualizar relógio a 1 Hz e dados externos na cadência do domínio. Não redesenhar tela por evento irrelevante.

Animações curtas, tipicamente 120–180 ms, em regiões pequenas; sem `shadow_width`, `transform_*` ou render FULL nos widgets dinâmicos. Grandes transições usam atualização simples até aprovação de banda; degradar para sem animação se p95 exceder orçamento. Primeiro frame ≤2 s, flush p95 ≤16 ms, flush/update ≤4 são gates B a repetir; p95 histórico 9,25 ms e espera próxima de 5 µs não são SLA da UI nova. Meta adicional R: toque → feedback visível p95 ≤50 ms, p99 ≤100 ms fora de manutenção.

### Flash não pode ser tratada apenas com prioridade baixa

**D:** o IDF documenta impacto de operações SPI1 no cache e que auto-suspend depende da peça; descreve XIP a partir de PSRAM como alternativa de configuração. **B:** auto-suspend falhou nesta flash, e erase/glyphs/render já produziram artefatos. Portanto, um mutex ou aguardar VSYNC não garante segurança de uma operação que atravessa vários frames. [Concorrência de flash no P4](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/spi_flash/spi_flash_concurrency.html).

Direção principal: reduzir acessos quentes à flash, três FB, invalidação mínima e **FlashCoordinator** cobrindo NVS, cache, GC, staging C6 e OTA P4. A tarefa que executa pode ser distinta, mas toda escrita normal precisa de uma concessão. Auditar escritas implícitas de componentes, inclusive persistência Wi-Fi; panic/coredump são exceção de emergência.

Dois estados operacionais:

1. **Interativo:** somente escritas pequenas cuja combinação com o pipeline tenha passado o ensaio gráfico e de latência. Dados voláteis podem ficar pendentes; nenhuma escrita é iniciada no callback de toque. Fonte quente é carregada antes do render frequente. Aguardar ociosidade de UI ajuda, mas não é prova de ausência de underrun.
2. **Manutenção:** OTA e operações longas suspendem providers/animações e adotam transição visual explícita. Se scanout estável durante flash não tiver sido comprovado, apagar backlight antes da região crítica e restaurar somente após primeiro frame íntegro. Tela estática sozinha também não é garantia. Ensaiar pausa/retomada sem reinit concorrente e limites de duração.

**H a investigar na fase 2:** `CONFIG_SPIRAM_XIP_FROM_PSRAM` no conjunto exato, consumo `.text/.rodata`, cache, ISR, PPA e banda. Isso não é auto-suspend e não está proibido pelo guia, mas não entra no baseline sem comparação física. Manter funções/constantes críticas corretamente localizadas enquanto o caminho alternativo não for aprovado.

**Gate de produto:** rotina normal de NVS/cache deve passar sem flashes brancos ou tearing. Manutenção pode ter apagamento anunciado e limitado; essa exceção não justifica apagamentos periódicos a cada cache. Se nenhuma configuração segura passar, a contingência é cache somente em RAM enquanto se resolve o gate, ou revisão de hardware/armazenamento independente aprovada. Não lançar o produto premium alegando que “acontece só a cada 30 minutos”.

## 7. Rede, provisionamento e funcionamento offline

Boot: display → primeiro frame → backlight → início assíncrono Hosted → Wi-Fi → DHCP/IP → hora utilizável/NTP → HTTPS serializada → providers. Carga de cache é paralela ao boot quando possível e não atrasa primeiro frame. Criar event loop/netif uma única vez; recuperar instâncias conforme ciclo de vida suportado, nunca recriar em loop.

Máquina de estados: `DISABLED`, `LINK_STARTING`, `LINK_UP`, `ASSOCIATING`, `WAIT_IP`, `TIME_PENDING`, `ONLINE`, `BACKOFF`, `OFFLINE_DEGRADED`, `MAINTENANCE`. `TRANSPORT_DOWN/FAILURE` revoga readiness imediatamente, incrementa geração, cancela requests e deixa UI ativa.

Backoff do enlace **B**: 2, 4, 8, 16 e 30 s; adicionar jitter ±20% e respeitar cooldown de resets. Conexão Wi-Fi e DNS têm contadores separados. Após duas falhas DNS consecutivas, reassociar para renovar DHCP/DNS conforme receita, sem repetir a ação mais de uma vez por minuto. Contar falhas de resolução genuínas; 404 ou erro de certificado não são DNS.

Provisionamento principal no próprio touch: scan limitado, seleção de rede 2,4 GHz, entrada mascarada e tentativa cancelável. Setup USB privado serve bring-up e assistência local; produção exige modo físico de manutenção com timeout e comandos reduzidos. Não implementar portal SoftAP aberto nem BLE no início. APIs/radio do C6 operam com storage em RAM; conferir por inspeção e teste que não persistem senha também no C6.

Credenciais atravessam mailbox privada e são gravadas no P4 apenas após **30 s contínuos com IP**, por serviço de persistência e fora do callback de toque. Queda reinicia o temporizador. Não condicionar esse sucesso a um servidor externo, mas informar se internet/hora continuam indisponíveis. Conservar credencial anterior até commit íntegro da nova.

DHCP timeout inicial 20 s, associação 15 s; resolver e conectar com prazos medidos, sem bloquear app. Para requests externos: DNS 5 s, conexão/TLS até 10 s, leitura ociosa 5 s e prazo total 20 s como parâmetros iniciais; verificar o que o cliente permite separar e garantir um deadline total por relógio monotônico.

**Uma conexão HTTPS global e um handshake por vez**, incluindo OTA, telemetria e provisionamento remoto futuro. Preferir fila/executor a semáforo distribuído que cada consumidor pode esquecer. Reutilizar conexão sequencialmente quando servidor permitir; fechar e liberar antes de trocar destino. OTA recebe exclusividade de manutenção e suspende polling. Nenhum upload de logs pode criar segundo TLS.

Requests de leitura têm no máximo duas novas tentativas dentro do deadline; full jitter, respeito a `Retry-After` limitado e cotas de provider. Falhas TLS de autenticidade, 401/403 e schema inválido não geram retry rápido. Circuit breaker por origem abre após cinco falhas transitórias, espera 60 s, permite uma sonda e pode crescer até 15 min. Renovação periódica de um domínio não pode monopolizar a fila; round-robin entre domínios elegíveis com prioridade para ação visível do usuário e anti-starvation.

Hora possui estados `INVALID`, `RTC_HOLDOVER`, `SYNCED` e `SUSPECT`. RTC plausível permite exibir relógio com qualidade explícita. No arranque novo, NTP precede providers HTTPS conforme B; sem sincronismo e sem confiança temporal suficiente, manter offline. Após sincronização válida, holdover com incerteza limitada pode sustentar TLS em reconexões; janela máxima inicial 24 h é R a ajustar pela deriva real. NTP comum não autentica a hora: rejeitar saltos implausíveis, comparar fontes e nunca desabilitar validação temporal/hostname para contornar problema. Bootstrap de hora após reset total precisa de teste com atacante/rede hostil; até lá falha fechada para HTTPS.

Cache mantém último valor válido, idade e proveniência. TTL inicial R: clima 30 min, mercado 5 min; offline até 24 h com stale explícito, após isso estado expirado e dado somente como histórico. Domínio pode refinar TTL contratualmente. Sem relógio confiável após cold boot, idade é desconhecida e o cache nunca é apresentado como fresco. Não substituir dado válido por zero quando endpoint falha.

## 8. Persistência, partições e integridade

### Layout físico proposto para congelamento na fase 0

Todos os tamanhos obrigatórios do guia são preservados. Offsets abaixo são **R**, não uma tabela já gravada. Reservar bootloader abaixo de `0x10000`, tabela em `0x10000`, para folga de futuro bootloader assinado; verificar tamanho real e suporte antes do primeiro flash de produto.

| Região | Offset | Tamanho | Final exclusivo |
|---|---:|---:|---:|
| Bootloader/reserva inicial | 0x000000 | 0x010000 | 0x010000 |
| Tabela de partições | 0x010000 | 0x001000 | 0x011000 |
| `nvs` | 0x011000 | 0x008000 — 32 KiB | 0x019000 |
| `nvs_keys` | 0x019000 | 0x001000 — 4 KiB | 0x01A000 |
| `phy_init` | 0x01A000 | 0x001000 — 4 KiB | 0x01B000 |
| `otadata` | 0x01B000 | 0x002000 — 8 KiB | 0x01D000 |
| Alinhamento | 0x01D000 | 0x003000 | 0x020000 |
| `ota_0` | 0x020000 | 0x800000 — 8 MiB | 0x820000 |
| `ota_1` | 0x820000 | 0x800000 — 8 MiB | 0x1020000 |
| `storage` | 0x1020000 | 0x900000 — 9 MiB | 0x1920000 |
| `coredump` | 0x1920000 | 0x100000 — 1 MiB | 0x1A20000 |
| `c6_ota` | 0x1A20000 | 0x200000 — 2 MiB | 0x1C20000 |
| Reserva não alocada | 0x1C20000 | 0x3E0000 — 3,875 MiB | 0x2000000 |

Definir `CONFIG_PARTITION_TABLE_OFFSET=0x10000` somente ao materializar esse layout. Apps alinhadas a 64 KiB; dados a 4 KiB. `nvs_keys`, `phy`, `ota`, `coredump` usam subtypes próprios; `storage` usa subtype suportado pelo port LittleFS, e `c6_ota` dado bruto com subtype customizado registrado. Não marcar `nvs` genericamente encrypted como substituto de NVS Encryption. Validar flags de storage/coredump/staging com Flash Encryption em bancada dedicada.

Não há app `factory` dedicada: ambos os slots incluem modo seguro. Isso preserva A/B e evita presumir compatibilidade futura de factory com anti-rollback. A reserva final não é um terceiro slot de 8 MiB. Se ambos apps/bootloader estiverem inválidos, recuperação é procedimento local autenticado de assistência; não prometer recuperação remota de bootloader corrompido.

### Política de gravação

NVS guarda configuração pequena, credenciais pelo vault, schema e journal mínimo de atualização; cache volumoso vai para LittleFS. Deduplicar valores, agrupar preferências e aguardar pelo menos 5 s sem alterações, com commit máximo uma vez por minuto para preferências comuns. Critérios de 30 s com IP aplicam-se às credenciais; OTA e comandos explícitos de recuperação têm regras transacionais próprias.

Cache: **no máximo uma gravação por domínio a cada 30 min**, somente se houve mudança. Impedir flush simultâneo de todos os domínios e GC junto de TLS/render pesado. Nenhuma escrita síncrona por toque. Leituras de assets/cache também são limitadas e retiradas do caminho de render frequente.

Envelope persistido: magic, schema, domínio, geração, tamanho limitado, timestamp/qualidade, versão de produtor e CRC32 do header relevante + payload. CRC detecta corrupção acidental, **não autentica conteúdo**. Cache não contém segredos e nunca executa comandos recebidos de arquivos.

Gravação: serializar → arquivo temporário no mesmo filesystem → escrever completamente → `fsync` → fechar → validar leitura/CRC → `rename` → confirmar metadados conforme garantias reais do port. Manter duas gerações nomeadas e selecionar maior geração íntegra no boot, para não depender exclusivamente de rename ou múltiplas operações atômicas. Remover geração velha só após comprovar a nova. Testar todas as fronteiras com corte de energia: suporte a `fsync` e semântica de rename não devem ser presumidos pelo nome POSIX.

NVS não oferece transação automática entre várias chaves de aplicação. Configuração relacionada deve ser um registro versionado limitado ou dois slots lógicos com geração/CRC e ponteiro de seleção. Conservar leitura da versão anterior durante a janela de rollback. Não migrar destrutivamente schema no primeiro boot de firmware ainda pendente.

Filesystem: usar máximo 70% de 9 MiB para dados comprometidos; manter ≥30% para gerações temporárias, metadados e GC. Limite inicial 64 arquivos, quotas por domínio, remover temporários órfãos e históricos expirados em manutenção. Falha de montagem: não autoformatar. Tentar leitura/recuperação conforme API suportada; se falhar, desabilitar cache, preservar evidência e seguir com UI/defaults. Reformatação é operação explícita que afeta somente cache.

NVS corrompida não deve provocar o padrão indiscriminado `erase + init`: entrar em setup seguro, sem apagar identidade/chaves. “Restaurar configurações” apaga dados do usuário de maneira separada das chaves de fábrica/eFuses. Prever que apagar storage criptografado e apagar identidade são operações diferentes. A [documentação NVS](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/storage/nvs_flash.html) sustenta uso para valores pequenos e recuperação de interrupções; o protocolo de múltiplos registros acima é responsabilidade do produto.

Desgaste: registrar contagem/volume de gravações e estimar ciclos por setor com write amplification medido e endurance da peça exata. Calcular vida para pelo menos cinco anos 24/7, com margem de 2× de carga; não deduzir vida útil apenas da cadência lógica. Não persistir telemetria de cada erase no mesmo flash que se deseja poupar.

## 9. Segurança e etapas para produção

### Ameaças e fronteiras

| Ameaça | Controle principal | Limite restante |
|---|---|---|
| AP/DNS/backend hostil | TLS com hostname/CA/tempo, parser limitado, URLs fixadas por política | DoS continua possível; sistema fica offline utilizável. |
| Imagem OTA adulterada/replay | Manifesto e apps assinados, hash, target/revisão/schema/compatibilidade, política de versão | Sem Secure Boot, acesso físico ainda pode substituir firmware/verificador. |
| Leitura da flash/USB de manutenção | NVS Encryption + proteção da chave + Flash Encryption e fechamento de debug na produção | Não impede ataques físicos invasivos; procedimento de assistência muda. |
| C6 comprometido/SDIO observado | Validar RPC/eventos/comprimentos, sem confiança em payload; TLS termina no P4 | Rádio conhece credencial Wi-Fi em uso; protegê-lo também na fabricação. |
| Logs/coredump contendo senha/token | Minimização, redaction, zeroização e exportação controlada | Dump de memória bruta pode conter cópias TLS/driver; não tratá-lo como log sanitizado. |
| Dependência/build comprometidos | Pins/hashes, SBOM, CI isolada, revisão do patch e assinatura fora da build comum | Exige operação contínua de resposta a vulnerabilidades. |

Credenciais não aparecem em StateStore, eventos, view-models, URLs, logs ou mensagens de erro. O campo de senha da UI é buffer privado temporário: remover/zeroizar ao terminar, cancelar ou sair da tela; documentar e minimizar cópias do widget e driver. Nunca imprimir bodies de autenticação ou headers Authorization. Dados como SSID, localização e identificadores também exigem minimização, mesmo quando não são senha.

Proteger NVS explicitamente. Direção de produção: chaves NVS por dispositivo, armazenadas em `nvs_keys` sob proteção de Flash Encryption, com configuração de NVS Encryption própria do IDF. Não embutir chave simétrica universal no app. **Antes dos eFuses, não existe proteção física equivalente**: usar rede/credenciais de teste no desenvolvimento; não declarar o protótipo protegido contra extração. A [Flash Encryption do P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/security/flash-encryption.html) e NVS têm mecanismos diferentes, que devem ser exercitados juntos.

Bundle de CAs completo conforme baseline; atualização do bundle via release assinada. Certificados de servidor precisam verificar cadeia, SAN/hostname e validade; proibir `skip_common_name_check`, TLS inseguro e aceitar qualquer CA. Não fazer pin exclusivo de certificado leaf que expira sem plano de rotação. Allowlist de origem/redirect impede OTA/provider ir a destino arbitrário; limitar headers e redirects a zero inicialmente.

Recomendar **RSA-PSS/Secure Boot V2 com RSA-3072** para P4. A documentação [v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/security/secure-boot-v2.html) descreve os esquemas; a documentação [atual release-v5.5](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32p4/security/secure-boot-v2.html) alerta sobre ECDSA e certos vetores no P4. Essa diferença exige revisão de errata por revisão física, e reforça a escolha RSA. Não extrapolar o alerta como ensaio feito na nossa placa.

Chaves privadas de assinatura ficam fora de dispositivo, repositório, logs e runners comuns; serviço de assinatura protegido/HSM, papéis separados e trilha auditável. Definir key ID, rotação e revogação conservadora antes da fabricação. Manifesto assinado usa formato canônico especificado ou bytes exatos com assinatura destacada; rejeitar campos duplicados e representações ambíguas. Hash sem assinatura não é autenticidade.

Etapas:

1. Desenvolvimento: eFuses/Secure Boot/Flash Encryption/anti-rollback desligados; implementar verificação de OTA assinada em software e testes negativos. Sem garantias contra substituição física.
2. Pré-produção dedicada: fechar OTA/recovery, mapa de chaves e suporte do C6; ensaiar Secure Boot RSA, Flash Encryption, NVS Encryption, dumps protegidos e interfaces de manutenção em placas destinadas ao teste irreversível.
3. Produção: após **autorização humana antes de eFuses**, conforme guia, provisionar identidade/chaves por unidade, verificar leitura de estado e desligar debug/download não necessários de acordo com o procedimento de assistência aprovado. Não executar comandos de queima neste planejamento.
4. Anti-rollback: habilitar somente depois de provar atualização e fallback na mesma security version; elevar security version apenas quando janela de recuperação estiver fechada. Não gastar contador eFuse a cada release funcional. C6 e P4 têm políticas próprias, ambas verificadas na transação.

Coredump de 1 MiB é reserva, não garantia de comportar qualquer conjunto de tarefas. Validar tamanho/formato e ausência de truncamento. Desenvolvimento usa dump restrito de laboratório; produção exige proteção em repouso e exportação autenticada com retenção limitada, ou desabilita captura de áreas sensíveis. Redaction de log não sanitiza dump automaticamente.

## 10. OTA P4, Slave OTA C6 e recuperação transacional

### Artefato de release

Pacote inclui manifest ID, produto/PCB/revisão SoC, versão app, security version, tamanho e hash da imagem P4 **assinada com padding**, hash/imagem C6 quando aplicável, ESP-Hosted/RPC/transporte/features, formatos de persistência suportados, versões mínimas/máximas de bootloader e pares permitidos. Incluir assinatura, key ID, toolchain, lock, SBOM, ELF/map para diagnóstico e evidência de gates. Rejeitar pacote que dependa de reparticionar ou trocar bootloader em campo no fluxo inicial.

### P4 A/B

Usar APIs oficiais `esp_https_ota`/`app_update` por trás do executor único. Baixar incrementalmente ao slot inativo com tamanho conhecido e verificado, sem apagar slot ativo. Não fazer bulk erase como otimização inicial. Validar assinatura, hash, target, revisão mínima, tamanho e política antes de ativar; falha preserva app atual. Commit de seleção somente após `end` válido; falha elétrica antes disso não deve selecionar download parcial.

**B:** rollback habilitado; `PENDING_VERIFY` marca válido após display, primeiro frame, serviços e app_loop saudáveis por 15 s contínuos. Prazo máximo 60 s; sem saúde, marcar inválido e reiniciar se houver alternativa bootável. Rede não é pré-requisito. Crash antes de confirmar deve produzir rollback pelo bootloader. **R:** prever falta de slot válido e retornar ao modo de recuperação sem loop. [OTA P4 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/ota.html).

Confirmação local de boot e aceitação do **pacote combinado** são diferentes: a primeira não depende de internet, a segunda verifica protocolo/versão do C6. Não iniciar outra OTA com app pendente. Depois de confirmar, manter slot anterior durante campanha de observação; regressões tardias exigem política explícita de retorno compatível, pois `VALID` encerra o rollback automático daquele boot.

### C6: única rota permitida

`c6_ota` é staging bruto no P4, **não um slot bootável do P4 nem o filesystem do C6**. A referência registra 1.108.272 B transmitidos e 1.108.256 B em outra descrição da imagem; a diferença de 16 B deve ser resolvida pelo artefato e hash reais, não ajustada à mão. Ambos excedem 1 MiB; reservar 2 MiB permanece obrigatório.

Baixar e verificar integralmente staging antes de alterar o C6. Quando não houver rede, assistência pode carregar pacote assinado no **P4**, que valida e o transfere via SDIO; isso não é gravar C6 pela USB. O recurso local de carga é R a implementar e testar.

Transferir somente imagem de aplicação destinada ao slot OTA remoto: `begin → write → end → activate`, verificar retorno em cada etapa, reiniciar C6, confirmar versão **3.0.6**, RPC v2 e SW_AGGR. Não transferir bootloader, tabela de partições nem imagem merged como se fossem app. Validar layout A/B de 4 MiB real do C6, capacidade do slot, assinatura/rollback remoto e comportamento de pending verify; o guia não prova esses detalhes.

O [ESP-Hosted 3.0.6](https://components.espressif.com/components/espressif/esp_hosted/versions/3.0.6/readme?language=en) lista o exemplo oficial `examples/ota/coprocessor_ota`. Usar esse exemplo da versão fixada como referência de API, não endpoints/nomes adivinhados. O detalhe completo do rollback remoto não foi confirmado nas páginas acessíveis desta pesquisa: é gate explícito de fonte e bancada.

Migração especial **já descrita em B**: build `examples/ota/coprocessor_ota/cp`, target C6, SDK com patch; gravar no P4 o migrador temporário `OTA_METHOD_PARTITION`, transmitir por SDIO e confirmar o novo enlace; somente então gravar firmware final P4. Na travessia 2.12.x → 3.0.6, desabilitar `OTA_VERSION_CHECK_HOST_SLAVE` e `OTA_VERSION_CHECK_SLAVEFW_SLAVE` **apenas no migrador temporário**, pois a checagem relatada pulava a transferência. Firmware final preserva verificações estritas. A ordem especial é **C6 primeiro, P4 final depois**.

### Atualização conjunta não é atômica

| Situação | Política permitida |
|---|---|
| Atualizar só app P4 mantendo Hosted 3.0.6 | Preferencial; C6 não muda. Ambos slots P4 devem entender C6 3.0.6. |
| Atualizar C6 mantendo protocolo compatível com P4 atual e candidato | C6 primeiro, provar saúde; P4 depois. Testar fallback P4 com C6 novo. |
| Nova linha incompatível | Exigir firmware-ponte explicitamente compatível com estados intermediários, ou migração assistida equivalente à receita. Proibida OTA remota comum sem ponte comprovada. |
| Reverter P4 para baseline Hosted 2.x após C6 3.x | Proibido presumir funcionamento. Recuperação exige conjunto/migrador compatível. |
| C6 sem firmware funcional para SDIO | Slave OTA deixa de existir. Necessário rollback autônomo remoto comprovado; sem isso, unidade vai à assistência/substituição. Não oferecer UART C6 como caminho de produto. |

Journal versionado em NVS: `IDLE → STAGED → C6_TRANSFER → C6_VERIFIED → P4_STAGED → P4_PENDING → ACCEPTED`, com hash/transaction ID/versões e status de verificação. Gravar nas transições significativas, nunca em cada chunk. Após reboot reconciliar journal com slot P4, hash staging e versão/estado C6 reais; não confiar apenas no último marcador. Operações devem ser idempotentes. Retentativa não ativa imagem sem `end` válido.

Se a versão atual do C6 não possuir rollback autônomo e a partição necessária, app Slave OTA pode não conseguir corrigir bootloader/layout. Resolver no gate de industrialização antes de prometer atualização recuperável exclusivamente SDIO. P4 verificar assinatura do binário C6 protege o canal de atualização, mas somente cadeia de boot segura no próprio C6 impede substituição física do rádio.

Matriz de falhas obrigatória: perda de energia durante staging, erase/write P4, metadata de seleção, primeiro boot, cada fase Slave OTA, ativação/reboot C6, mismatch de versão, journal corrompido, assinatura inválida e schema incompatível. Resultado deve ser app anterior funcional ou modo seguro recuperável, nunca boot loop ou aceitação de pacote parcial.

## 11. Observabilidade e plano de testes

### Instrumentação

Registro estruturado em ring buffer RAM limitado, com tempo monotônico, build ID, severidade, domínio, código de erro e correlation ID. Sem corpos HTTP, senhas, tokens ou URLs com query sensível. Amostrar estado a baixa frequência; agregado de contadores, não log por pixel/pacote. Exportação manual autenticada ou em janela do executor de rede.

Métricas: boot/primeiro frame, input-to-photon, histograma de flush, frames perdidos/underrun quando observável, fila máxima/drop, tempo de lock, heap interno/PSRAM livre e maior bloco, mínimo de heap, stack high-water, uso CPU por core, geração/restarts C6, versão/RPC/features, tentativas DNS/TLS, writes/erase/GC, reset reason e transações OTA. Ausência de contador do hardware exige método externo; não chamar “zero underrun” à falta de instrumentação.

### Campanhas

| Nível | Casos e método | Resultado exigido |
|---|---|---|
| Host | Reducers, FSM de boot/rede/OTA, jitter com seed, prioridade/fairness, fila cheia, TTL com RTC inválido, parser malformado/limites, CRC, schemas N/N−1 | Determinísticos, sem IDF no domínio; sanitizers e fuzz do parser/manifesto; nenhuma fuga/overflow em corpus. |
| Integração | Drivers substituídos por fakes: respostas atrasadas, duplicadas, fora de ordem, cancelamento, falha de alocação em cada ponto | Sem deadlock, resultado único, pool retornando ao baseline, estado antigo descartado. |
| Build | P4/C6 limpos com pins, patch diff, Kconfig/partições/ELF/assinatura | Target/revisão corretos, sem warning não explicado, binários cabem com padding/margem. |
| HIL básico | Boot frio/quente, reset54 com analisador lógico, SDIO/RPC/SW_AGGR, touch centro/bordas/gestos/teclado | 100 boots consecutivos por unidade de amostra sem falha; primeiro frame ≤2 s. |
| Render | Padrões móveis, dígitos, fonte maior permitida, tela mais pesada, toque/scroll, fetch frio de glyph | Filmagem de alta velocidade controlada + marcadores temporais e traces; zero tearing/flash branco na rotina. Foto isolada não vale. |
| Rede | AP desligado, senha errada, DHCP silencioso, DNS NXDOMAIN/timeout, TLS expirado/hostname errado, captive portal, servidor lento, chunked >48 KiB | UI responsiva, nenhum segundo TLS, retry limitado, stale correto, recuperação quando falha removida. |
| Armazenamento | Power cut em tmp/fsync/rename, NVS commit, GC, filesystem cheio, dados aleatórios/corrompidos, SD removido | Seleciona geração íntegra/defaults, sem perda de identidade, sem autoformat destrutivo. |
| SD + Wi-Fi | Leitura/escrita cartão enquanto Hosted trafega e recupera | Sem corrupção/reset colateral; latência UI dentro do gate. Remover SD da release se não qualificado. |
| Memória | TLS de maior cadeia, maior JSON, teclado/telas, 10 mil requests e mil ciclos navegação | Piso interno preservado, sem fragmentação crescente, pilhas com margem, nenhuma alocação ilimitada. |
| Fault injection | Timeout I2C, C6 reset/travado, fila cheia, heap fail, worker sem progresso, evento perdido | Degradação/recovery local limitado, sem reboot por AP ausente, evidência clara. |
| Energia | Fonte/cabo finais, brownout/rampas, Wi-Fi TX + brilho máximo + I/O, cortes aleatórios | Sem corrupção permanente; reset cause coerente, retorno previsível. Não desabilitar brownout para passar. |
| OTA | Assinaturas boas/ruins, pacote truncado, rollback, anti-rollback em amostra dedicada, todos os cortes listados | Pelo menos 3 ciclos reais assinados aplicar/reverter, recuperação conjunta comprovada. |
| Soak/térmico | 24 h inicial; 72 h de regressão; 168 h final em gabinete com reconexões, cache e UI pesada | Zero panic/WDT inesperado, vazamento ou artefato; limite térmico e latência atendidos. |

Qualificação final recomendada: ≥5 unidades, preferencialmente ≥2 lotes/revisões de BOM equivalentes, executando 168 h cada; não é demonstração estatística de MTBF de anos. Definir campanha adicional de confiabilidade e homologação conforme ambiente comercial. Cortes de energia: ≥100 distribuídos por fluxo de persistência/OTA, com fases determinísticas cobertas antes da aleatoriedade.

Térmica: **B: gate ≤70 °C** do roteiro anterior. **R:** medir ponto quente P4/placa com instrumento externo, temperatura ambiente e interior do gabinete; respeitar também limites individuais e toque humano do gabinete, que podem ser mais restritos. Registrar método de sensor, tolerância e duty cycle. Faixa comercial de ambiente ainda precisa ser definida; ensaiar seus extremos antes de release, sem inventar especificação industrial.

Memória em soak: comparar estados equivalentes após aquecimento de 30 min; perda final ≤4 KiB interna e sem tendência monotônica, PSRAM sem crescimento não explicado, maior bloco ≥80% do valor inicial equivalente e suficiente para maior alocação admitida. Leaks conhecidos de qualquer tamanho bloqueiam; tolerância de medição não é licença para vazamento.

Rede recuperada após retorno de AP/serviços: meta ≤90 s para IP e ≤120 s para dado válido, com tempo confiável disponível e backend saudável; medir separado de backoff longo após falha persistente. C6 pode levar ~21 s para falhar: UI deve permanecer dentro das metas durante esse intervalo.

## 12. Critérios objetivos e Definition of Done

DoD comum: requisito rastreado, implementação futura revisada, testes de sucesso/falha executados, logs sem segredos, limites de recursos documentados, configuração efetiva/artefatos identificados por hash, instrução de reprodução, risco residual explicitado e nenhuma pendência crítica escondida como “conhecido”. Build verde não fecha gate físico. Resultado de bancada exige data, unidade/lote, commit, firmware/hash C6, SDK/patch, configuração, comandos e evidência bruta.

| Gate | Critério de encerramento |
|---|---|
| G0 Reprodutibilidade | P4/C6 e transitivas fixados; patch auditado; layout validado pelo gerador IDF e imagem assinada de teste; BOM/pinout/revisão identificados. |
| G1 Boot/recursos | 100 boots por unidade sem panic; PSRAM/flash reconhecidas; primeiro frame ≤2 s; backlight posterior; orçamento estático/dinâmico fecha com margens. |
| G2 Display/flash | 3 FB e draw alinhados; flush p95 ≤16 ms e ≤4/update na carga máxima; toque p95 ≤50 ms/p99 ≤100 ms; zero artefato em rotina com NVS/cache/GC. Caminho de manutenção aprovado separadamente. |
| G3 Rede | C6 3.0.6/RPC v2/SW_AGGR comprovados; um HTTPS global; DNS/AP/C6 down não travam UI; recuperação limitada e credenciais sem vazamento. |
| G4 Dados/offline | Cortes não produzem configuração parcial; cache com idade/CRC/schema; corrupção mantém UI/setup e chaves; quotas e throttle verificados. |
| G5 OTA | Três ciclos assinados aplicar/reverter; estados intermediários P4/C6 compatíveis; power cuts e recovery remoto/local documentados; sem depender de UART C6. |
| G6 Qualificação | 168 h nas amostras, térmica e alimentação finais, sem falhas críticas, margens de memória/pilha/render, SD concomitante aprovado ou fora da release. |
| G7 Produção | Segurança nos dois chips, fabricação/assistência ensaiadas, autorização de eFuses registrada, chaves/rotação/SBOM/recall, canário e rollback operacional aprovados. |

## 13. Roadmap executável por fases

Estimativa de capacidade R para equipe com firmware, UI/QA e acesso contínuo à bancada; não é prazo contratado. Gates podem exigir revisão de hardware e ampliar duração.

| Fase | Dependências; trabalho | Entregáveis e gate | Esforço indicativo |
|---|---|---|---|
| 0 — Congelar base | Guia e pesquisa; inventariar unidade/C6, errata, dependências, patch, partições, instrumentos | BOM e recipe reproduzível, lock P4/C6, mapa de memória, ADRs iniciais; **G0** | 1–2 semanas |
| 1 — Núcleo mínimo | G0; composição, estado/eventos, supervisor, console e boot offline, display mínimo | Firmware mínimo e testes host, recursos medidos, procedimentos de boot; **G1** | 1–2 semanas |
| 2 — Render e flash | G1; tela máxima de teste, fontes, tearing, cache frio, erase/write/GC e hipótese XIP | Relatório comparativo, configuração display congelada, política interativo/manutenção; **G2** | 2–3 semanas |
| 3 — Conectividade | G1 e orçamento G2; Hosted, Wi-Fi, tempo, worker HTTPS, provisionamento | FSM/recovery HIL, limites por provider, segredo privado; **G3** | 2–3 semanas |
| 4 — Domínios e persistência | G2/G3; providers, cache, NVS, quotas, schemas e UI final | UX offline completa e ensaios de corte/corrupção; **G4** | 2–3 semanas |
| 5 — Atualização recuperável | G2/G3/G4; assinatura, A/B, staging e Slave OTA, matriz conjunta | Pacote assinado, journal, recovery e testes negativos; **G5** | 2–4 semanas |
| 6 — Qualificação | G0–G5; fault injection, soak, gabinete, energia, SD e desempenho | Relatório por unidade, riscos residuais e release candidate; **G6** | 2–3 semanas + repetição se falhar |
| 7 — Industrialização | G6; chaves, security por chip, eFuses em amostras, fábrica, assistência, rollout canário | Golden package, runbooks, assinatura operacional, decisão formal de release; **G7** | 2–4 semanas |

Testes host, UX com dados simulados e especificação de pacotes podem evoluir enquanto outra fase mede hardware, mas nenhum avanço de implementação fecha um gate anterior por suposição. As fases 2 e 5 são caminho crítico. Se display/flash falhar, suspender expansão visual e fechar causa antes de multiplicar serviços.

Rollout: laboratório → equipe interna → canário limitado → expansão gradual. Interromper promoção diante de boot loops, corrupção, falha de assinatura/OTA, aumento de resets ou piora de latência/memória. Telemetria agregada e opcional respeita o executor único e privacidade. Suporte precisa poder identificar o par P4/C6 e pacote exato sem acesso aos segredos.

## 14. ADRs propostos e comparação de alternativas

ADRs são **propostas**, não decisões já aprovadas. Criar registro com contexto, opções, decisão, consequências, hipótese física e gate de revisão.

| ADR | Direção principal | Alternativas e custo/risco |
|---|---|---|
| 001 — Baseline fixo | IDF 5.5.4 e conjunto atualizado B | SDK mais novo pode corrigir defeitos, mas exige nova compatibilidade, memória e bancada. Menor custo inicial fica com conjunto reproduzido. |
| 002 — Arquitetura | C++ modular, estado de escritor único e contratos pequenos | Monólito reduz arquivos mas amplia acoplamento/efeitos colaterais; framework genérico amplia RAM e operação sem benefício inicial. |
| 003 — UI ownership | Uma task LVGL, projeções/diffs limitados | LVGL com locks em todas as tasks permite mais pontos de bloqueio e inversão; ganho de paralelismo não comprovado. |
| 004 — Render | RGB565, 180°, TRIPLE_PARTIAL, três FB | DOUBLE economiza ~1,172 MiB, mas muda pipeline de risco; FULL aumenta banda/invalidação. RGB888 sobe FB em 50%, sem benefício necessário. |
| 005 — Rede | Hosted convencional, TCP/IP/TLS no P4 e HTTPS único | TLS paralelo reduz tempo agregado mas já esgotou SRAM; network split exige outra arquitetura e qualificação, não elimina orçamento da UI automaticamente. |
| 006 — Persistência | NVS pequena + LittleFS versionado, escritor coordenado | NVS para tudo aumenta RAM/GC; cartão removível reduz previsibilidade; storage adicional muda BOM/operação. |
| 007 — Flash/render | Sem auto-suspend; política de escrita medida e manutenção | Prioridade/VSYNC apenas são insuficientes; XIP PSRAM usa memória/banda e é experimento; flash independente/revisão da peça são contingências físicas. |
| 008 — OTA | A/B P4 e Slave OTA C6 com matriz/journal | “Atualizar dois binários e reiniciar” pode deixar par incompatível; UART C6 fora da rota; terceiro app de resgate de 8 MiB não cabe na reserva proposta. |
| 009 — Saúde | Boot local válido sem internet, supervisor por progresso | Exigir internet gera rollback de app saudável em falta de AP; reiniciar toda a placa por DNS piora UX e desgaste. |
| 010 — Segurança | RSA-PSS, cadeia assinada, chaves por unidade e hardening progressivo | ECDSA menor tem alerta atual P4; assinatura só no transporte não protege boot; chave universal reduz custo operacional mas amplia impacto de vazamento. |
| 011 — Recursos | Alocação explícita por capability, limites e admissão | PSRAM genérica oculta requisitos DMA/cache; buffers sem teto favorecem falha por fragmentação. |
| 012 — Evolução | Schema N/N−1, slots anteriores preservados e pacotes pareados | Migração destrutiva simplifica código novo mas impede rollback e recuperação offline. |
| 013 — Provisionamento | Touch local e USB de serviço restrita | BLE/SoftAP aumentam superfície, RAM e testes; habilitar só com necessidade comercial e gates próprios. |
| 014 — Desempenho/energia | Clocks validados, iTWT desligado, sem sleeps agressivos | Power save pode reduzir consumo, mas adiciona jitter/recuperação. Priorizar previsibilidade; medir watts/temperatura antes de otimizar. |

## 15. Riscos priorizados, mitigação e contingência

P0 bloqueia liberação; P1 bloqueia a funcionalidade afetada ou a qualificação; P2 exige orçamento/monitoramento. Probabilidade é avaliação inicial, não estatística de campo.

| Risco | Prioridade / chance | Mitigação e sinal | Contingência e dono |
|---|---|---|---|
| Flash causa underrun/glitch durante escrita | P0 / alta | G2 com NVS/GC/OTA e fontes frias, três FB e coordenação; vídeo/latência | Suspender escrita oportunista, manutenção explícita e avaliar XIP/revisão hardware; plataforma/display. |
| C6 perde SDIO após atualização, sem fallback | P0 / média | Confirmar layout/bootloader/rollback do C6 e cortes durante ativação | Não habilitar OTA C6 de campo sem recovery; assistência/substituição se coprocessador não inicia; atualização/hardware. |
| Rollback P4 incompatível com C6 novo | P0 / alta se não modelado | Matriz de pares e pacote com firmware-ponte | Bloquear promoção; migrador assistido comprovado; arquitetura/OTA. |
| SRAM/fragmentação sob TLS + UI | P0 / alta | Executor global, arenas, maior bloco e admissão | Adiar requests/liberar opcionais, reduzir payload/UI; nunca retirar terceiro FB; plataforma/rede. |
| Queima de eFuse impede boot/assistência | P0 / média | Amostras dedicadas, mapa de chaves, autorização e leitura de verificação | Interromper lote; não há reversão de eFuse; segurança/fabricação. |
| Credenciais em log/dump/C6 flash | P0 / média | Storage RAM no rádio, vault, testes de extração e controles de dump | Bloquear release, revogar/rotacionar credenciais expostas; segurança. |
| Recuperação de rede bloqueia UI ou entra em loop | P1 / alta | Link assíncrono, deadlines, eventos/geração e cooldown | Offline persistente sem reboot por AP; rede. |
| Callback/ownership de FB incorreto | P1 / média | Adapter único, caminho completion verificado, nenhuma mutação do FB em scanout | Voltar ao pipeline B, não improvisar flush_ready; display. |
| NVS/cache perde consistência com energia | P1 / média | Gerações/CRC/schema e cortes em todas as fronteiras | Última geração válida/defaults; reset de cache explícito; storage. |
| Energia/temperatura no gabinete | P1 / média | Fonte final, Wi-Fi TX/brilho/soak, limites por componente | Reduzir carga/brilho de modo controlado ou rever alimentação/térmica; hardware. |
| SDMMC interfere no Hosted | P1 / média | Teste conjunto, ciclo de vida e slots fixados | Excluir SD da release; plataforma. |
| Relógio inválido impede TLS ou permite tempo hostil | P1 / média | Qualidade temporal, bounds e falha fechada | Modo offline e recuperação de hora autenticável a especificar; segurança/rede. |
| Mudança transitiva silenciosa | P1 / média | Lock/hashes/patch/CI reproduzível | Reverter conjunto de software e par C6 compatível; build/release. |
| Wear/logs/GC crescentes | P2 / média | Quotas, throttle, write amplification e vida útil medida | Desabilitar histórico opcional, ajustar cadência; storage. |

## 16. Confirmações físicas obrigatórias antes de produção

- Identificação real P4 v1.3, revisão de PCB, BOM/lote, flash JEDEC/capacidade e PSRAM 32 MiB; cruzar errata atual com revisão, especialmente segurança, PPA/MSPI e clocks.
- Reprodução do baseline atualizado a partir de ambiente limpo, hashes das imagens P4/C6, patch exato e transitivas resolvidas; nenhum resíduo de configuração H2/SPI.
- Reset54 ativo baixo, largura de pulso/temporização e SDIO 4-bit; versão C6/RPC v2/SW_AGGR após boot frio, warm reset e falha induzida.
- Layout físico e bootloader do C6 4 MiB, tamanho dos slots, assinatura, pending verify, rollback autônomo e capacidade de atualizar por SDIO após falhas. Sem comprovação, OTA C6 recuperável permanece bloqueada.
- Primeiro frame em ≤2 s, rotação/touch nas bordas e teclado, backlight posterior, ausência de inicialização duplicada do brilho.
- Três FB, 50 linhas internas, alinhamento real, caminho de completion correto, p95/latência na UI máxima e nenhuma reutilização de FB ainda em scanout.
- Flash sem auto-suspend; comportamento de NVS, cache, GC, staging e OTA com render. Ensaiar XIP PSRAM separadamente se necessário; não promover com base em inferência documental.
- Heap interno mínimo, maior bloco, TLS com certificados reais, parser/corpo máximos, pools Hosted/lwIP, pilhas de todas as tasks e integridade em erro/reconexão.
- AP/DHCP/DNS/C6 down, captura de deadlines e recuperação; nenhum handshake sobreposto, netif duplicado, segredo em log ou persistência Wi-Fi não autorizada no C6.
- Retenção/deriva RTC com ML1220, perda completa de energia, NTP hostil/indisponível e comportamento TLS sem hora confiável.
- Power cuts em NVS/LittleFS/OTA/journal; integridade de configuração, preservação de chaves, cache stale com idade desconhecida e ausência de autoformat destrutivo.
- Três ciclos assinados aplicar/reverter P4 e atualização/recovery do C6; todos os pares intermediários e fallback após app P4 confirmado; assistência local com pacote assinado entregue ao P4.
- SD e Wi-Fi simultâneos, ou exclusão explícita do SD da release; I2C compartilhado se áudio for habilitado.
- 168 h de soak final, múltiplas unidades, temperatura em gabinete, fonte/cabo finais, brownouts e uso máximo; ausência de glitch documentada por método além de foto.
- Secure Boot RSA/Flash Encryption/NVS Encryption/coredump e política anti-rollback em unidades dedicadas; fechamento de interfaces sem destruir a assistência; autorização humana antes de eFuses.
- Golden image, chaves/SBOM, processo de fábrica, teste por unidade, rotação/revogação, canário, rollback operacional e procedimento para unidade cujo C6 não oferece mais SDIO.

**Decisão de início:** pode-se iniciar a fase 0 com esta direção. **Decisão de produção:** permanece condicionada a G0–G7, especialmente G2 e G5. Nenhum resultado isolado de build, uma conexão HTTPS ou uma foto do display substitui esses gates.

## Apêndice — fontes e limites desta pesquisa

As fontes oficiais foram consultadas antes da proposta. Referências próximas dos fatos no texto sustentam apenas as afirmações documentais; políticas, orçamentos, filas e metas novas são recomendações próprias. O guia de bancada é a fonte dos resultados físicos, não a pesquisa web.

- As páginas do registry para BSP, adapter, Hosted e Wi-Fi Remote e as releases de IDF/LVGL confirmaram as versões publicadas. A página de dependências detalhadas do BSP não ficou acessível nesta consulta; a exigência LVGL 9.5.x é evidência do guia/resolução anterior e será reproduzida no G0.
- A documentação web de threading do LVGL apresentou erro/redirect e o fonte bruto retornou limitação de acesso. A regra de dono único é mandatória no guia e mantida como decisão conservadora; não se atribui a uma página que não foi lida.
- O exemplo completo de OTA C6 não pôde ser lido pelos URLs tentados. Sua existência consta no README oficial Hosted; sequência prática e migração especial vêm do guia. APIs, layout e rollback remoto devem ser verificados nos fontes do componente fixado, sem inventar garantias.
- A documentação exata 5.5.4 de Secure Boot e a linha atual release-v5.5 diferem no alerta ECDSA. Usar RSA e auditar errata/BOM antes da produção, preservando a distinção entre documentação histórica e correção atual.
- Não foram executados build, flash, testes físicos, gravação de credenciais ou alteração de eFuses nesta atividade. Não há resultados novos de bancada.
