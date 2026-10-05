# Diagnóstico de atualizações de dados — 04/10/2026

## Sintoma e conclusão parcial

O operador relatou que vários dados deixam de atualizar ao mesmo tempo. Não
informou ainda duração, telas exatas ou relação com uma reconexão. A revisão
do código foi seguida por captura passiva da COM8, sem comandos, flash ou
reinício solicitado.

Foi confirmada falha real e repetida na consulta de Bitcoin/altcoins:
o agendador executa o domínio 0, a verificação DNS do servidor de hora passa,
o serviço declara hora confiável, mas a abertura da conexão HTTPS expira.
Não foi observada resposta HTTP nesses casos. Portanto, esses registros não
demonstram erro de API key, quota HTTP, parser ou redesenho da tela.

Isso ainda não identifica a causa comum de todos os dados: não houve
conclusão de outros domínios nas janelas capturadas. Uma falha do provedor,
da rota/rede, do transporte C6 ou do estabelecimento TLS continua exigindo
diferenciação. Não foi identificado o hash do app em execução nesta captura;
a árvore analisada é `cf335d9` mais alterações locais.

## Evidência nova

| Uptime da placa | Evidência |
|---|---|
| 222.836 ms | `product refresh scheduled domain=0` |
| 234.016 ms | `dns=ESP_OK ntp=ESP_OK https=ESP_ERR_HTTP_CONNECT duration=11183ms` |
| 485.436 ms | Nova execução `domain=0`; linha serial intercalada com log Sonoff |
| 496.626 ms | `esp-tls: Failed to open new connection in specified timeout` |
| 496.626 ms | `transport_base: Failed to open a new connection` |
| 496.626 ms | `HTTP_CLIENT: Connection failed, sock < 0` |
| 496.626 ms | `dns=ESP_OK ntp=ESP_OK https=ESP_ERR_HTTP_CONNECT duration=11192ms` |
| 616.726 ms | Nova execução automática `domain=0` |
| 627.936 ms | Mesmo timeout de conexão, `https=ESP_ERR_HTTP_CONNECT duration=11212ms` |

Foram três tentativas com falha confirmadas nas janelas. Entre as duas
últimas, o novo agendamento ocorreu aproximadamente 120 s após a conclusão
anterior, coerente com o retry de Bitcoin. O executor voltou a operar sem
reinício solicitado; o problema observado é a conexão repetidamente falha,
não uma paralisação permanente do scheduler nessa amostra.

As capturas foram filtradas para agendamento/resultados/erros, sem salvar
corpos, credenciais ou headers. Uma janela intermediária de 75 s não teve
linhas correspondentes ao filtro; isso não prova que a placa estava parada.
Arquivos locais de evidência, fora do Git:

- `%TEMP%/np2-refresh-audit-live-20261004.log`
- `%TEMP%/np2-refresh-audit-detail-20261004.log`
- `%TEMP%/np2-refresh-audit-final-20261004.log`

## Caminhos comuns encontrados no código

1. **IP não comprova acesso aos provedores.** `connectivity.online` é ativado
   ao receber DHCP. O agendador depende desse estado, mas ele não representa
   DNS/TCP/TLS/backend saudáveis.
2. **Precondição DNS global.** Todas as consultas começam resolvendo
   `time.cloudflare.com`. Somente esse resultado é reportado à política de
   recuperação DNS. O DNS do hostname do provedor é feito dentro do cliente
   HTTP e pode falhar sem alimentar o mesmo contador. `dns=ESP_OK` no log
   agregado não comprova a resolução do provedor.
3. **Precondição de tempo global.** Uma renovação NTP que falha limpa a
   qualidade `trusted` e pode impedir todos os HTTPS. Não há holdover
   qualificado nesse caminho; qualquer revisão precisa preservar validade
   temporal e verificação dos certificados.
4. **Executor compartilhado.** Uma operação eWeLink sem deadline total pode
   atrasar todos os domínios. Nas execuções capturadas, o domínio 0 terminou;
   não foi demonstrado esse travamento.
5. **Refresh completo com orçamento único.** `refresh_all_product_domains()`
   divide 20 s entre cinco domínios e encerra ao esgotar o prazo. Depois,
   `network_validation_task()` registra o resultado agregado para todos,
   incluindo os que não chegaram a executar. Uma falha pode adiar domínios
   não consultados até seus intervalos de retry.
6. **Reconexão por amostragem.** O scheduler reinicia prazos quando observa
   `online=false`. Uma queda e retorno ocorridos inteiramente durante uma
   consulta podem não aparecer nessa amostragem. Não existe geração de DHCP
   transportada ao scheduler para detectar esse caso de forma inequívoca.
7. **Entrega à UI sem confirmação.** Um EventBus cheio recusa o snapshot; o
   produtor apenas registra `event deferred`, sem retry dedicado. Não houve
   essa mensagem nas capturas, portanto é risco de código, não causa provada.

Fontes principais: `network_validation_service.c`,
`connectivity_diagnostic.c`, `time_service.c`, `data_refresh_scheduler.c`,
`app_event_bus.c` e `app_state.c`.

## Cadência efetiva

| Domínio | Após sucesso | Após falha |
|---|---:|---:|
| Bitcoin/altcoins | 1 min | 2 min |
| Clima | 2 h | 15 min |
| PTAX | 24 h | 1 h |
| Fear & Greed | 24 h | 1 h |
| Índices | 24 h | 1 h |

Valores iguais não comprovam ausência de consulta. Distinguir instante da
tentativa, sucesso de aquisição, timestamp do dado na origem e alteração do
valor. Índices são apresentados stale após 30 min, embora consultados a cada
24 h; essa política merece alinhamento com a UX.

## Próximo incremento recomendado

Priorizar diagnóstico e confiabilidade das atualizações antes de novas telas:

- Registrar por domínio tentativa/sucesso, fase da falha, duração e próximo
  prazo; no ponto de falha obter códigos TLS antes de destruir o cliente,
  com `esp_http_client_get_and_clear_last_tls_error()` do IDF fixado.
- Separar resultados dos domínios no refresh completo e não adiar os que
  não executaram. Preservar uma conexão HTTPS global e deadlines.
- Provar nova aquisição após queda/reconexão, DNS ruim, NTP indisponível,
  timeout de provedor e operação eWeLink, sem reset manual e sem apagar cache.
- Qualificar recuperação conforme a causa: falha de um servidor externo
  isolado não deve provocar tempestade de resets do C6 ou reboot do P4.
- Confirmar o caminho inteiro: request concluído → snapshot publicado →
  app_loop → revisão → widget atualizado.

Na etapa inicial nenhuma correção de firmware foi aplicada. As falhas
capturadas estão abertas; aumento da frequência de consulta não resolve
timeout de conexão.

## Cards amarelos após várias horas — nova captura de 04/10

O operador informou que os cards estavam amarelos naquele momento. A captura
passiva mostrou o P4 ainda executando consultas após mais de cinco horas de
uptime:

| Uptime | Registro |
|---|---|
| 19.153.636 ms | BTC terminou com `ESP_ERR_HTTP_CONNECT`, DNS/NTP OK, 10.062 ms |
| 19.273.736 ms | BTC agendado novamente, 120,1 s após a conclusão |
| 19.283.776 ms | Mesma falha, duração 10.042 ms |
| 19.335.316 ms | Descoberta LAN: 3 dispositivos online, 3 com estado |

Essa janela afasta uma parada definitiva do scheduler: ele continua tentando.
A descoberta LAN demonstra tráfego local, não a saúde de HTTPS externo.
No PC, sondas sem credenciais responderam HTTP 200: CoinGecko `/ping` em
516 ms, Open-Meteo em 656 ms e a série BCB em 219 ms. São outra pilha de rede
e outra origem de conexão; não comprovam acesso equivalente pelo P4 nem
validam a chave da consulta de mercado.

A primeira captura nova omitiu por engano `np2_netcheck` do filtro; por isso,
silêncio nesse arquivo não serve como evidência de worker parado. A captura
seguinte incluiu a tag correta. Logs locais, fora do Git:
`%TEMP%/np2-refresh-yellow-20261004.log` e
`%TEMP%/np2-refresh-yellow-detail-20261004.log`.

### Significado do amarelo

`NP_DATA_STALE` usa `np_c_warning()`. Esse estado surge quando o provider
falha e conserva o último valor, quando a idade excede o limite ou quando
o relógio deixa de ser confiável. Portanto, a cor sozinha não distingue
esses caminhos.

Limites da projeção: BTC 30 min, clima 4 h, dólar 36 h, índices 30 min.
O ADR-067 escolheu consulta diária de índices, mas a projeção conservou
30 min de recência. Um índice pode ficar amarelo após 30 min sem que o
executor tenha parado; não tratar esse indicador como prova de bloqueio.

### Instrumentação da etapa seguinte

Foram acrescentados logs sanitizados ao helper HTTPS e à projeção:
provider fixo (sem URL/header), fase, status HTTP, bytes, duração, erro TLS,
errno e heap interno livre/maior bloco. A publicação registra aceitação
ou rejeição do snapshot, e `app_loop` registra transições de stale/confiança
temporal com idade dos três dados da Home. Não imprime cotações, credenciais
ou identidade de rede. A política de consultas e recuperação permanece a
mesma, para observar a causa antes de alterá-la.

### Configuração antiga comprovada e resultado do primeiro reteste

O build antigo `firmware/build/np2_p4.bin` tem SHA-256
`74A41874C9351FFAF88A7FBAD3C6CDE81DBB2AD09F9960F616BC1A8C56B10FD9`,
igual ao último registro de gravação. Seu ELF tem SHA-256
`61EA749FC57AE7CC40FB1CAB690A3E95A3748546749758D411F1312DC9060525`.
O `firmware/sdkconfig` utilizado antes da revisão tinha SHA-256
`0BE2019E74EA3B296BF7C2929C4BEDBF2EED573D5FF7BB2ABB1BB26DA3C52535`
e mantinha `CONFIG_MBEDTLS_HARDWARE_AES=y`/GCM acelerado. O header gerado
confirmava esses macros; o map do ELF contém `esp_aes_acquire_hardware` e
o objeto `esp_aes_gcm.c.obj`. Isto contradiz o default `AES=n` e o ADR-076:
defaults não substituem escolhas já existentes no sdkconfig gerado.

Foi acrescentado um gate CMake que recusa AES acelerado antes da compilação.
Um configure real isolado com a configuração antiga falhou exatamente nesse
gate, como esperado, sem compilar/gravar o perfil negativo.
O sdkconfig normal também foi alinhado apenas nessa opção e regenerado
oficialmente por `idf.py -DIDF_TARGET=esp32p4 -B build reconfigure`.
O configure passou; a configuração agora tem o mesmo hash `2524F49D...67B8`
da imagem instrumentada, sem macros AES/GCM de hardware. A cópia anterior
foi preservada em `build/data-refresh-diag-20261004/sdkconfig-legacy-before-aes-fix`.
O ELF/binário antigo foi mantido como artefato histórico; o binário gravado
nesta rodada vem explicitamente do diretório `data-refresh-diag-20261004`.

O primeiro build instrumentado limpo, em
`firmware/build/data-refresh-diag-20261004`, passou com IDF 5.5.4/esp32p4 e
AES por software; app `0x34b950`, 59% livres no slot de 8 MiB. Imagem
`63B3411AED68A25E52D6C88BB86214AF55DE6B54C13D9A6C71A300AB3AE5D4D5`,
ELF `8ABB8D48B13BC4A0C73C4D9EA843D36EA28A4251103E72299AAFCCD16967E0C8`.
Gravada somente a app na COM8 em `0x20000`, hash verificado.

No boot todos os cinco domínios concluíram com sucesso (sete HTTP 200,
considerando três índices). A projeção passou de `flags=0xf` sem hora a
`flags=0x10`: hora confiável e nenhum dos quatro indicadores stale.
**Isso não resolveu a falha do Bitcoin**:

| Uptime | Resultado |
|---|---|
| 28.821 ms | CoinGecko HTTP 200, 3.929 bytes, 1.746 ms de HTTPS |
| 88.911 ms | Nova consulta BTC |
| 100.561 ms | `connect-tls`, `ESP_ERR_HTTP_CONNECT`, TLS `0x8006`, code/flags/errno zero; 11.640 ms |
| 100.561 ms | Snapshot aceito, BTC stale=1; `app_loop flags=0x11`, BTC com 74 s |
| 220.661 ms | Próxima consulta, após retry de 120 s |
| 232.621 ms | `headers`, `ESP_ERR_TIMEOUT`, sem status HTTP; 11.959 ms |

O operador confirmou **somente Bitcoin amarelo** após essa gravação.
Assim, AES-DMA era uma divergência real, mas não explica sozinho o sintoma:
o mesmo problema reapareceu com AES por software. Não há erro explícito de
certificado, HTTP 401/403/429, parser ou fila recusada nessa amostra. A tela
recebe corretamente a marca stale após a falha; não está congelada.

Os logs mediram apenas 44–55 KiB internos livres antes de alguns HTTPS e
23–32 KiB enquanto o cliente ainda existia. Estes valores **não atendem ao
piso de memória do projeto**. A pressão de SRAM é um problema aberto; não
foi provado que ela causou os timeouts capturados. Não qualificar estabilidade
ou margem TLS a partir dos HTTP 200 iniciais.

### Sonda de controle na imagem seguinte

Para discriminar origem/rede, após uma falha transitória de BTC pode ocorrer
um HEAD em `example.com`, sequencialmente e dentro do orçamento remanescente
de 20 s, com cooldown de 10 min. O resultado não substitui o erro do mercado,
não atualiza preços nem muda a cadência. O rótulo `control` não inclui URL
ou credenciais nos logs. A imagem seguinte também incorpora o gate CMake.

Build final passou: app `0x34ba80`, 59% livres; SHA-256 app
`B730DE7E19B0F1F1FC9C39A437FEC0BB74FC6A3C1F432F52E2920D6C04EFF806`,
ELF `9AF7D23FE67E6EC513109F5D4CC912EA4246C9DDBCA82A72345600597CC09A82`.
Sdkconfig `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8`,
tabela `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23`.
AES hardware e auto-suspend desligados, TLS interno/RX 8 KiB, pool LVGL
extra 128 KiB e três framebuffers preservados. Gravação somente da app
COM8/`0x20000` com hash verificado; boot confirmou P4 v1.3, PSRAM 32 MiB,
Hosted/RPC v2/SW_AGGR e `P4 local bring-up ready`. Hash C6 instalado não
foi obtido. Logs em `%TEMP%/np2-refresh-instrumented-20261004.log`,
fora do Git; cada captura fecha a serial ao terminar.

### Resultado final desta rodada

Na imagem `B730DE7E...F806`, foram registradas **12 consultas BTC com HTTP
200**, desde 28.781 ms até 777.511 ms de uptime, incluindo renovações
automáticas. Houve duas janelas de captura na mesma execução, com um intervalo
sem monitor entre elas; não contar o intervalo como amostra contínua.
As consultas registradas levaram aproximadamente 2–3,2 s. Todas publicaram
snapshot aceito com BTC stale=0. Nenhum panic/WDT foi encontrado no log
filtrado dessas janelas. Não houve outra falha BTC registrada, por isso a
sonda de controle **não chegou a executar em bancada**; sua discriminação de
origem ainda precisa ser observada numa recorrência real.

A captura terminou com a COM8 fechada. A imagem final permaneceu na placa.
Não houve mudança de provider, URL de mercado, intervalo, preço sintético ou
relaxamento de certificados. Não foi provada recuperação automática após
timeout na imagem final, pois a falha não reapareceu nela durante a amostra.

Conclusão delimitada: o amarelo BTC foi reproduzido como consequência de
timeout TLS/HTTP, com scheduler, EventBus e projeção ainda funcionais.
Foi corrigida a regressão de configuração AES efetiva e bloqueado seu retorno
em build. A origem dos timeouts intermitentes continua aberta, assim como
a margem de SRAM e o ensaio por várias horas. Sucesso após uma gravação/reset
não basta para atribuir causalidade à correção AES ou encerrar o defeito.

### Nova verificação após o retorno do operador — 04/10, 17:51–17:54

Sem nova gravação/reset, uma captura passiva de 180 s na COM8 registrou mais
três consultas BTC com HTTP 200 na mesma imagem, aos uptimes 2.215.591,
2.277.841 e 2.340.381 ms (aproximadamente 37–39 min após o boot). As durações
HTTPS foram 1.957, 2.141 e 2.443 ms. Os três snapshots foram aceitos com BTC
stale=0. A sonda de controle não disparou, pois não houve falha nessa janela.
A serial fechou ao final; não houve comando enviado à placa.

Antes dessas consultas havia 48.879–48.891 bytes de heap interno livre,
maior bloco 23.552 bytes; com o cliente ativo, cerca de 25.700 bytes livres
e maior bloco 14.336. Não há queda progressiva nesses pontos comparáveis
às capturas anteriores, mas a margem ainda não atende ao piso do projeto.

Para orientar a correção de memória, os tipos foram medidos no DWARF do
ELF P4 efetivamente gravado, sem depender do layout do compilador host:

| Tipo | Tamanho P4 |
|---|---:|
| `app_event_t` | 616 B |
| `offline_data_snapshot_t` | 488 B |
| `app_ui_projection_t` | 4.456 B |
| `flash_request_t` | 4.800 B |
| `camera_command_t` | 132 B |

A fila EventBus de 32 vagas reserva **19.712 B de payload** antes do overhead
FreeRTOS, porque cada evento carrega espaço para o snapshot completo.
É um alvo concreto para eventos pequenos com pool limitado/ownership,
mantendo `app_loop` como único escritor e política explícita de saturação.
Naquela captura isso ainda era análise: nenhuma redução de fila, transferência
para PSRAM ou mudança nas pilhas havia sido aplicada. O requisito é medir o ganho e os picos TLS,
não atribuir automaticamente os timeouts ao consumo de memória.

### Primeira redução de SRAM — EventBus e entrega pendente, 04/10

Conforme ADR-088, a fila mantém 32 vagas, mas seu item privado passa a guardar
somente o payload ativo. Os snapshots usam quatro posições estáticas internas,
com índice/geração e cópia de propriedade do barramento. `receive` copia para
o evento público e libera a posição; uma rejeição de enqueue também libera.
Nenhuma operação de fila ocorre dentro do lock das posições. O worker de rede
mantém o snapshot mais recente pendente se fila/pool recusarem a entrega e
tenta novamente no próximo ciclo, sem repetir HTTP ou escrita de cache.

`tools/run_app_event_bus_host_test.ps1` compila o EventBus real, o codec real e
as funções reais de entrega extraídas do serviço, com I/O mockado. Passaram:
inicialização/falha de alocação, round-trip dos comandos, propriedade das
cópias, fila cheia, pool cheio, liberação após rejeição, 1.000 ciclos de
reciclagem, rejeição de handle antigo sem liberar o atual, wrap da geração,
reenvio do mais recente sem persistência extra e 8.000 snapshots de quatro
produtores concorrentes sem perda/duplicação dos eventos aceitos.
Compilação `gcc -std=c11 -O2 -Wall -Wextra -Werror`. Os testes de scheduler,
codec e parsers de providers também passaram com `-pedantic`.

O DWARF do ELF P4 confirmou os mesmos tamanhos do host: evento público 616 B,
item de fila 56 B, snapshot 488 B e posição do pool 496 B. Payload da fila +
pool = 3.776 B; economia de **15.936 B** ante 19.712 B, sem contar o controle
FreeRTOS, cujo número de filas permanece igual. A redução não move
TLS/JSON/corpos para PSRAM nem altera hardware, dependências, Kconfig,
partições ou a cadência de consultas.

Build limpo ESP-IDF 5.5.4/P4 concluído sem warnings, comando:
`idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=build/event-bus-pool-20261004/sdkconfig -B build/event-bus-pool-20261004 build`.
App `0x34bf90` (3.456.912 B); 59% livres no slot de 8 MiB. Artefatos locais
ignorados em `firmware/build/event-bus-pool-20261004`; log de compilação em
`firmware/build/event-bus-pool-20261004-build.log`.

| Artefato | SHA-256 |
|---|---|
| App P4 | `06AAE6A5DE9FD9BC1D9995D237D359C574BC65FCBF345A14DB82B47F8B8EF9A1` |
| ELF P4 | `05A301A557374EBD71E19D01551267DF9C7DC4008EDD32F65AF9F10081E8F780` |
| sdkconfig | `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8` |
| Tabela | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |

Sdkconfig e tabela são idênticos aos da imagem anterior. Imediatamente antes
da gravação, uma captura de 70 s registrou HTTP 200 BTC aos 3.961.461 ms de
uptime (~66 min): heap interno antes 48.891 B/maior bloco 23.552 B; cliente
ativo 25.719 B/maior bloco 14.336 B. Snapshot aceito, BTC stale=0; LAN com
três dispositivos online/stateful. A primeira linha residual do buffer USB
não foi usada como evidência de uptime ou reset. Captura encerrou e liberou
COM8 antes do flash.

Gravação: mesmo `-DIDF_TARGET`, `-DSDKCONFIG` e `-B`, com
`-p COM8 -b 460800 app-flash`. Esptool verificou o hash e escreveu somente
`0x20000` (app); NVS, storage, tabela, C6 e eFuses não foram escritos pelo
comando de flash. O firmware pode continuar sua persistência normal pelo
coordenador. Boot identificou P4 v1.3, PSRAM 32 MiB, ELF `05a301a55...`,
RPC v2/SW_AGGR e `P4 local bring-up ready`. O log `np2_events` confirmou
`entries=32 item=56 payload=1792 snapshot_slots=4 pool=1984`.

A captura passiva de 330 s registrou 11 HTTPS completos com HTTP 200: cinco
BTC, um clima, um BCB, um Fear & Greed e três índices Brapi. Foram nove
entregas de snapshot aceitas (os três índices compõem uma entrega). BTC
concluiu aos 27.948, 89.698, 151.548, 213.408 e 276.148 ms de uptime,
em 1.680–2.636 ms por HTTPS, sempre com BTC stale=0. Nenhuma rejeição de
entrega, falha HTTPS ou panic/WDT apareceu no log filtrado. LAN manteve
três dispositivos online/stateful. Não houve saturação provocada em bancada;
o reenvio após saturação foi validado no host.

| Ponto comparável BTC | Imagem anterior | Imagem com fila compacta |
|---|---:|---:|
| Heap interno antes do HTTPS | 48.891 B | 64.143–65.447 B |
| Maior bloco antes do HTTPS | 23.552 B | 27.648 B |
| Heap interno com cliente ativo, fim do corpo | 25.719 B | 40.963–42.227 B |
| Maior bloco com cliente ativo, fim do corpo | 14.336 B | 27.648 B |

A comparação nova usa as quatro renovações, após a inicialização. A primeira
consulta teve 71.835 B antes e 47.923 B ao fim do corpo, por isso não entrou
na faixa de regime. São amostras pontuais antes do request e antes de destruir
o cliente; **não medem o mínimo durante todo o handshake/parser**. Mudanças
de alocação e fragmentação também afetam o delta observado; a economia de
15.936 B é a do layout da fila + pool, confirmada no target.

Logs filtrados em `%TEMP%/np2-event-bus-pool-20261004.log`, fora do Git;
captura por `%TEMP%/np2_event_bus_capture.py 330`, COM8/115200, sem comandos
ao dispositivo, com fechamento da serial ao final. A imagem nova permanece
na placa. Base Git `cf335d9f9c20773f0e3b599d22c85290de83f247` mais alterações
locais preservadas; hash C6 instalado não obtido, C6 não atualizado.

Conclusão: redução de SRAM entregue e medida, com refresh BTC funcionando
na amostra. O piso >80 KiB em regime e a margem ≥96 KiB no pior pico ainda
não estão atendidos/comprovados. Timeouts não foram reproduzidos nesta imagem,
portanto a sonda de controle e a recuperação após timeout continuam sem
evidência nela. Não declarar causa dos timeouts resolvida, estabilidade por
horas ou release a partir desta captura curta.

### Nova conferência passiva — 04/10, após 91 min de uptime

A pedido do operador, a COM8 foi monitorada por mais 180 s, sem enviar comandos
ou reiniciar a placa. A mesma execução avançou monotonicamente dos uptimes
5.466.308 a 5.632.308 ms. Três renovações adicionais do Bitcoin terminaram
em 5.486.408, 5.548.638 e 5.610.808 ms: todas HTTP 200, em 1,86–2,10 s,
`market_stale=0` e snapshot aceito. Antes: 65.427–65.447 B livres, bloco
maior 27.648 B; ao fim do corpo: 42.163–42.191 B, bloco maior inalterado.
Nenhum evento de fila adiado, falha HTTPS ou WDT/panic surgiu no trecho novo.

Isso confirma ciclos de refresh operacionais por volta de 91–93 minutos após
o boot e nenhuma queda de heap nessas amostras. Não comprova a janela de várias
horas em que o sintoma original foi relatado, nem provoca ou testa recuperação
após falha. O piso de heap interno segue sem atendimento.
