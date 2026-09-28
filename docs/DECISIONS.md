# Decisões de arquitetura

Este registro materializa decisões que já passaram de proposta para execução.
Uma decisão de arquitetura não substitui o respectivo gate físico.

## ADR-007 — Flash/render: coordenador serial e teste NVS explícito

**Estado:** a primeira gravação pequena durante carga passou em bancada.
Compactação/erase NVS foi reprovada por piscadas repetidas e está bloqueada.

**Contexto:** o flash GD `0xC84019` desta placa não permite auto-suspend neste
caminho; habilitá-lo causou boot loop. Erase de flash, busca de glyph em flash
e uso indevido de framebuffer já produziram tela branca ou tearing. Um mutex
LVGL ou uma prioridade menor não torna uma operação SPI1 segura por si só.

**Decisão:** manter `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`. Toda escrita normal
futura passa por `FlashCoordinator`. O primeiro corte vertical é uma task de
flash com fila de um item, que inicializa NVS sem apagar automaticamente e
executa somente uma gravação diagnóstica pequena, sem segredo, limitada a uma
vez por minuto. O callback LVGL apenas enfileira a intenção; a task LVGL é a
única que apresenta seu resultado. Falha de inicialização ou saturação recusa
a solicitação e preserva o estado existente.

**Consequências:** a escrita NVS pequena durante carga móvel passou, mas erase,
GC, LittleFS, staging C6 e OTA continuam bloqueados. Eles não voltam ao produto
por tentativa de UI; exigem uma alternativa de plataforma medida, como XiP em
PSRAM numa variante isolada, ou mudança de armazenamento/BOM.

**Alternativas rejeitadas neste estágio:** auto-suspend (falha já observada),
escrita direta por callback de toque (viola ownership e dificulta recuperação),
e adiar qualquer coordenação até a Fase 4 (deixaria o risco crítico sem ensaio).

## ADR-008 — Variante XiP em PSRAM para qualificação de flash da Fase 2

**Estado:** qualificado para o escopo físico do G2 em 2026-09-08; não aprovado
para produto.

**Contexto:** no baseline sem XiP, um lote de 64 commits NVS de blob de 512 B
concluiu sem reset, mas provocou piscadas repetidas mesmo com o backlight
desligado. A documentação do ESP-IDF para ESP32-P4 informa que XiP em PSRAM
mantém o cache habilitado durante operações SPI1, mas transfere `.text` e
`.rodata` para PSRAM e aumenta a carga desse barramento. Há ainda uma issue
aberta de OTA com XiP em P4; logo a configuração não é uma correção presumida.

**Decisão:** habilitar somente `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` nos defaults
da imagem de qualificação e preservar `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`. Não
alterar a frequência/Modo da PSRAM, os três framebuffers, o modo
`TRIPLE_PARTIAL`, a rotação, BSP, LVGL, adapter ou a imagem C6 neste A/B.

**Resultado do G2:** NVS pequeno e três lotes de 64 commits de 512 B com
reboot passaram sem artefato visual. LittleFS passou format/erase explícito e
64 ciclos de 4 KiB `write` → `fsync` → `rename` → leitura/verificação sob
render em 3.814 ms (256 KiB), seguido de reboot e 30 minutos de render ativo
sem artefato relatado. O C6 não foi regravado; o link Hosted/SDIO voltou no
boot observado.

**Promoção futura:** antes de promover XiP para produto, executar três ciclos
OTA aplicar/reverter e validar Hosted/SDIO durante essas operações. Cache de
produto ainda deve provar CRC, duas gerações, quota, GC e endurance nos gates
G4/G6. Qualquer artefato, reset, erro de hash OTA ou regressão de memória
reprova a variante.

**Rollback:** remover a única linha `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, gerar
uma build limpa e restaurar o artefato anterior. O backend persistente segue
RAM-only enquanto a variante não for aprovada.

## ADR-009 — Sonda assíncrona Hosted/Wi-Fi sem credenciais para início do G3

**Estado:** boot/link/scan validados em 2026-09-08; associação e recuperação
implementadas, mas pendentes de validação física. G3 continua aberto.

**Contexto:** a aplicação P4 já possuía as dependências Hosted 3.0.6 e
Wi-Fi Remote 1.6.4, mas não chamava `esp_hosted_init()`, não criava a interface
STA nem exercitava o C6. A inicialização do enlace pode aguardar dezenas de
segundos na ausência do C6; executá-la em `app_main` ou na task LVGL degradaria
a UI. O exemplo oficial Network Split exige criar `WIFI_STA_DEF` antes de
`esp_hosted_init()`.

**Decisão:** criar `connectivity_diagnostic`, uma task de 8 KiB e prioridade
3 que inicializa `esp_netif` e o loop de eventos, cria a STA, conecta Hosted
por SDIO, inicia Wi-Fi em `WIFI_STORAGE_RAM` e executa um único scan ativo. A
sonda recebe associação somente por mailbox privada de uma vaga; copia o pedido,
não o registra e apaga a cópia ao consumi-lo. Ela usa backoff 2/4/8/16/30 s
com jitter, trata eventos de IP fora de LVGL e esquece a configuração em RAM.
Não lista SSIDs, nem usa NVS, DNS, NTP ou HTTP(S). O resultado fica em um
snapshot curto para futura projeção de UI e em logs sem dados sensíveis.

**Validação e rollback:** gravar a imagem P4 e confirmar P4/C6 3.0.6,
RPC v2, SW_AGGR, STA pronta e contagem de APs, enquanto o diagnóstico de render
permanece responsivo. Em C6 ausente, o display deve continuar funcional e o
erro precisa ser limitado à task. Se falhar, remover a chamada
`connectivity_diagnostic_start()` e seus dois arquivos; nenhum segredo ou
persistência é deixado pela sonda.

**Resultado inicial:** na unidade P4 v1.3, o boot identificou C6 (`chip=0x0d`),
Hosted host/coprocessador 3.0.6, RPC v2, SDIO SW_AGGR e streaming; a sonda
concluiu o scan credencial-free com 38 APs. Associação, falhas induzidas,
DHCP/DNS, NTP, TLS e a responsividade visual durante esses casos permanecem
critérios do G3.

## ADR-010 — Continuidade local e recuperação limitada de conectividade

**Estado:** adotado para os próximos incrementos da Fase 3; ainda não é uma
aprovação física do G3.

**Contexto:** o produto deve permanecer utilizável durante perda de AP,
DHCP, DNS, Internet ou C6. Esses eventos podem impedir comunicação externa,
mas não podem parar render, touch, estado local ou a capacidade de recuperar a
conexão. Um booleano `online` não distingue enlace, associação, IP, DNS, hora
confiável e backend; usá-lo como saúde do sistema esconderia falhas e levaria
a resets indevidos.

**Decisão:** tratar continuidade como contrato verificável. A UI e o estado
local continuam ativos em modo offline degradado; a rede progride por estados
separados e deadlines explícitos. Associação tem prazo de 15 s, DHCP de 20 s,
DNS de 5 s e cada request externo prazo total de 20 s. A recuperação usa
backoff com jitter de 2/4/8/16/30 s. Senha rejeitada não entra em tentativa
infinita: exige novo pedido explícito. Após duas falhas DNS, a implementação
futura pode reassociar no máximo uma vez por minuto.

O P4 não reinicia por ausência de AP, DNS ou Internet. Recuperação do C6 deve
cancelar o recurso, recriá-lo e limitar-se a três ciclos completos em dez
minutos; depois entra em cooldown de cinco minutos ou depende de tentativa
explícita. O supervisor observa a inicialização Hosted por 30 s, sem colocar a
task que pode aguardar o C6 sob um watchdog impossível. Credenciais ficam em
RAM no estágio atual; persistência só poderá ocorrer após 30 s contínuos com
IP, por caminho de segurança e `FlashCoordinator` aprovados separadamente.

**Critério de aceitação:** o protocolo em
[`G3-CONTINUITY-VALIDATION.md`](G3-CONTINUITY-VALIDATION.md) é obrigatório.
AP restaurado deve voltar a IP em até 90 s e a dado válido em até 120 s quando
hora e backend estiverem saudáveis. Panic, WDT, reset P4 por falha externa,
travamento de UI, loop sem limite, segundo handshake TLS, ou segredo em log,
dump ou C6 reprovam o gate.

**Consequências:** a sonda RAM-only atual não pode prometer reconexão após
reboot porque não persiste credenciais. Isso preserva segurança e evita escrita
de flash antes do caminho de persistência estar aprovado; a experiência de
produto só será declarada contínua após o teste de provisionamento, retenção
segura e recuperação descrito no protocolo.

## ADR-011 — Bridge USB físico para teste Wi-Fi sem senha

**Estado:** implementado para a bancada; não é o provisionamento de produto.

**Contexto:** G3 precisa provar associação, DHCP e recuperação sem pôr uma
senha em widgets LVGL, logs, código-fonte ou NVS. A sonda RAM-only já possui um
mailbox de associação, mas ainda não havia caminho de bancada que o acionasse
de forma auditável e sem segredo.

**Decisão:** criar um serviço USB Serial/JTAG de manutenção que nasce
desarmado. Um toque físico arma uma única janela de 60 s. Dentro dela, aceita
somente `OPEN <ssid>` para uma rede aberta de laboratório, ou `FORGET`; limpa o
buffer de entrada e desarma depois de uma solicitação. Não aceita senha, não
repete SSID, não persiste configuração nem registra a entrada. A UI mostra
somente o estado armado, tempo restante e resultado. A task USB não toca LVGL;
a task LVGL é a única que atualiza a tela a partir de snapshots.

**Consequências:** a próxima campanha pode validar associação/DHCP e
AP-off/AP-on com um AP temporário aberto, sem enviar credencial por chat ou
introduzir uma rota de segredo prematura. Uma senha WPA, retenção após reboot,
NTP e HTTPS permanecem bloqueados até um desenho de provisionamento seguro e
seus testes de extração. Falha de instalação do driver USB deixa o serviço
fechado e não interfere no worker de rede, display ou C6.

## ADR-012 — Verificação externa serializada por manutenção física

**Estado:** implementado, aguardando validação física na imagem P4 candidata.

**Contexto:** G3 precisa separar falha de DNS, hora e TLS, mas executar NTP ou
HTTPS automaticamente após todo DHCP aumentaria tráfego, consumo de SRAM e a
superfície de falha do diagnóstico. O ESP-IDF oferece
`esp_netif_sntp_sync_wait()` para aguardar sincronização e o cliente HTTP(S)
permite o bundle oficial de CAs; ambas as rotinas podem bloquear, portanto não
podem executar na task LVGL.

**Decisão:** o console físico já armado aceita `CHECK` somente quando a STA
RAM-only tem IP. O pedido entra em um executor de uma vaga: resolução DNS do
servidor NTP fixado, SNTP de uso único sem gravação em NVS e, somente após hora
plausível, `HEAD` HTTPS a um destino fixado. Há uma task de 8 KiB, prioridade
2, e nunca duas verificações ou handshakes TLS em voo. O HTTPS usa
`esp_crt_bundle_attach`, mantém validação de CA e hostname e descarta a
resposta; não há URL configurável, redirecionamento, corpo, token ou segredo
em log. Os timeouts iniciais são DNS 5 s, NTP 15 s, TLS 10 s e 20 s para a
rodada. Uma chamada DNS que o lwIP ainda não devolveu não pode ser cancelada
por esta camada; ela permanece isolada da UI e a rodada é marcada como timeout
se ultrapassar 5 s.

Duas falhas DNS consecutivas podem solicitar uma reassociação, controlada pelo
worker Wi-Fi e limitada a uma por minuto. O reset físico do C6 continua sob
propriedade do ESP-Hosted: a aplicação não toma GPIO54 diretamente. A
recuperação completa Hosted/Wi-Fi após `TRANSPORT_DOWN` permanece bloqueada
até haver uma sequência de `esp_wifi`/netif/Hosted deinit-reinit comprovada em
bancada, pois um reset P4 como atalho reprova G3.

**Validação e rollback:** com o AP de laboratório associado, registrar o
resultado de `CHECK` e memória antes/depois, depois induzir NXDOMAIN, timeout,
NTP indisponível e TLS inválido em três rodadas. Caso haja artefato, WDT,
segundo handshake ou pressão de memória, voltar à imagem P4
`A9B8C334C48B6BD167C3D866B547330ABE78DCA9D404AE0793B72C04ED917C5B`; o C6 não
é regravado. Referências: [SNTP do ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)
e [cliente HTTP(S) do ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_client.html).

## ADR-013 — Falha SDIO do C6 não reinicia o P4

**Estado:** adotado; implementação e bancada de recovery pendentes.

**Contexto:** a configuração efetiva do ESP-Hosted 3.0.6 ativou
`CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE=y`. A própria opção
documenta que uma falha de transporte em runtime reinicia o host. Isso viola o
contrato de G3: AP, DNS, Internet ou C6 ausente não podem reiniciar o P4,
interromper render ou retirar o painel do modo offline local.

**Decisão:** fixar a opção em `n` no default versionado. O worker de
conectividade deve observar `EH_HOST_EVENT_TRANSPORT_FAILURE`/`DOWN`, marcar a
rede indisponível e executar, fora de LVGL, no máximo três ciclos completos de
`esp_wifi` + Hosted deinit/reinit em dez minutos. O componente Hosted conserva
a propriedade de GPIO54 e do pulso de reset; a aplicação nunca dirige o pino
diretamente. Depois do limite, a rede entra em cooldown de cinco minutos ou
aguarda pedido físico explícito. Display, touch e estado local não participam
da recuperação.

**Validação e rollback:** antes de induzir reset54, confirmar no `sdkconfig`
efetivo que o reinício automático do host está desabilitado. Com render e
touch ativos, registrar três resets C6 e a recuperação do IP, depois observar
o cooldown. Panic, WDT, reboot P4, reset do display, mais de três ciclos ou
GPIO54 tomado por código da aplicação reprovam o gate. Se a nova recuperação
falhar, restaurar a imagem P4 anterior e manter o painel offline; C6 não é
regravado.

## ADR-014 — Provisionamento WPA2 por touch, RAM-only até o gate de segurança

**Estado:** implementado como candidato de G3; persistência deliberadamente
adiada.

**Contexto:** o produto precisa de Wi-Fi protegido por WPA2, enquanto o canal
USB físico de bancada aceita somente uma rede aberta e não aceita senha. A
configuração efetiva atual tem NVS Encryption e Flash Encryption desabilitados;
gravar uma senha em NVS neste estado a deixaria recuperável por leitura física
da flash. Ativar a proteção de chaves por Flash Encryption ou HMAC exige eFuse
e é irreversível, portanto permanece bloqueado pelos gates de OTA/recovery e
autorização humana explícita.

**Decisão:** adicionar ao diagnóstico atual um botão `CONFIGURAR WI-FI WPA2`
que abre um modal LVGL. O serviço de provisionamento, e não a UI, é dono dos
buffers de SSID e senha em SRAM interna. A UI envia um caractere por toque,
exibe apenas o SSID e a contagem de bolinhas, e nunca guarda a senha em
textarea, AppState, evento, log ou USB. Ao conectar, o serviço copia a
credencial ao mailbox privado de uma vaga do worker de conectividade, apaga
seus buffers e a configuração permanece em `WIFI_STORAGE_RAM`. Rejeição de
autenticação limpa também a configuração do driver e exige nova entrada
explícita; não há loop de senha.

**Consequências:** o usuário pode configurar e testar WPA2 sem depender do AP
aberto de bancada, mas a credencial é perdida no reboot. A futura
`EncryptedNvsCredentialStore` será incluída somente após a estratégia de
NVS Encryption/eFuse, atualização P4/C6 e rollback estarem validados. A
referência oficial para a exigência de NVS Encryption é a
[documentação ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/storage/nvs_encryption.html).

## ADR-015 — StateStore com escritor único e projeções sem segredo

**Estado:** implementado como primeiro incremento da Fase 4; validação física
de G4 pendente.

**Contexto:** os diagnósticos das fases anteriores expunham snapshots de cada
serviço diretamente à UI. Isso é adequado para bancada, mas não estabelece o
fluxo de produto em que a UI consome dados pequenos e sanitizados, sem chamar
rede, NVS ou filesystem. Também não havia uma fila de eventos comum com limite
e métrica de saturação para evoluir resultados de providers e persistência.

**Decisão:** `app_loop` (8 KiB, prioridade 3) torna-se o único escritor da
projeção publicada. Ele consome um `EventBus` de 32 eventos de tamanho fixo e
coleta snapshots curtos dos adaptadores existentes. A projeção inclui somente
estado de rede, disponibilidade/resultado de persistência e gerações de cache
e configuração; não contém SSID, senha, corpo HTTP, objeto JSON ou view-model
LVGL. Para esses dois domínios, a task LVGL apenas copia essa projeção sob
seção crítica curta. A fila recusa quando cheia e contabiliza a perda, em vez
de crescer ou bloquear a UI.

**Consequências:** o primeiro corte remove da tela de diagnóstico a leitura
direta de conectividade e `FlashCoordinator`, preservando seus comandos
assíncronos. Providers, modelos de clima/mercado, idade do dado, quotas,
limpeza e a UX offline final ainda precisam publicar eventos tipados e obter
testes host/bancada próprios. O build P4 verde desta alteração não é evidência
de corte de energia, corrupção, filesystem cheio ou qualidade visual; esses
ensaios continuam requisitos de G4.

## ADR-016 — Contrato portável de integridade para cache de duas gerações

**Estado:** implementado como incremento de base da Fase 4; ensaio físico de
corte/corrupção continua pendente para G4.

**Contexto:** o `FlashCoordinator` já conserva duas gerações do cache em
LittleFS e valida CRC, versão e tamanho, mas a gramática do registro estava
embutida na task que possui NVS e LittleFS. Isso impedia testar no host os
casos que definem recuperação offline e aproximava uma regra de domínio do
acesso físico à flash.

**Decisão:** o header binário, o CRC-32, a validação limitada de header e a
seleção da maior geração válida passam a residir em `cache_record`, módulo C
sem dependência de ESP-IDF, LVGL ou filesystem. O `FlashCoordinator` continua
dono exclusivo de abrir, escrever, sincronizar, renomear e ler LittleFS; ele
somente mapeia erros de I/O para `esp_err_t`. Magic, schema v1, tamanho do
header e alternância de slots não mudam neste incremento.

**Consequências:** há teste host para CRC, header inválido, limite de payload
e fallback à geração anterior. A verificação ainda não é evidência de flash
real: quota, limpeza, cortes em cada fronteira e corrupção na placa continuam
obrigatórios no G4. Nenhum provider, request HTTPS, dado de produto ou escrita
automática foi introduzido.

## ADR-017 — Snapshot offline v1 com inteiros e origem explícita

**Estado:** codec, persistência através do coordenador, projeção e cards de UI
implementados; adapters HTTPS e validação física do snapshot ainda pendentes.

**Contexto:** G4 precisa exibir clima e mercado mesmo sem rede, mas não pode
deixar JSON, ponto flutuante de provider, URLs ou erros de transporte cruzarem
para a UI. Open-Meteo fornece temperatura e umidade, enquanto o endpoint
`simple/price` do CoinGecko fornece preço, variação e timestamp; ambos têm
formatos externos que não são contrato de produto.

**Decisão:** `offline_snapshot.v1` usa temperatura em décimos de grau Celsius,
preço Bitcoin/USD em centavos e variação de 24 h em pontos-base. Cada domínio
declara `available`, `stale` e `observedAtUnixS`; a origem é somente `cache` ou
`live`. O modelo C equivalente não depende de ESP-IDF, LVGL, HTTP ou storage.
O contrato compartilhado, seu exemplo e os eventos sanitizados são a fonte de
verdade para adapters e services futuros.

**Consequências:** o formato armazenado é uma sequência explícita de 25 bytes,
sem padding de ABI, dentro do registro de cache já protegido por CRC. Ao montar
o filesystem, o coordenador valida também esse payload; dados técnicos antigos
ou inválidos continuam diagnosticáveis, mas não são exibidos como dados de
produto. O `app_loop` transforma a origem em `cache` e marca clima após 2 h e
mercado após 30 min como stale quando há hora plausível. Os parsers portáteis
recusam corpo acima de 768 B, campos ausentes e números fora dos limites antes
de criar o snapshot. Uma escrita de snapshot aceita no máximo uma vez a cada
30 min; a fila de um item recusa sobreposição. Este ADR não autoriza chave,
polling automático, novo handshake paralelo ou escrita de flash por callback
de UI.

**Configuração de produto (2026-09-12):** a localidade escolhida pelo operador
é Brasília-DF (`-15.793889`, `-47.882778`). O refresh continua explicitamente
acionado pela manutenção: clima e BTC/USD passam pelo mesmo worker HTTPS
serial, preservam o último valor válido como stale quando um provider falha e
não usam token, URL configurável ou polling automático. A operação precisa de
HIL antes de ser considerada aprovada.

## ADR-018 — Controles G4 no diagnóstico quando o RX da COM não está disponível

**Estado:** implementado para bancada; não é funcionalidade de produto.

**Contexto:** na unidade P4 v1.3, a COM8 entrega logs ao monitor oficial sem
reset, mas não encaminha bytes recebidos ao serviço `usb_serial_jtag` mesmo
com a janela física de 60 s confirmada na tela. Isso impediria executar os
ensaios G4 de filesystem cheio e corte de energia, embora os ensaios dependam
de uma ação física explícita e não de um protocolo de produto.

**Decisão:** expor no diagnóstico quatro botões: filesystem cheio, corte antes
do `rename`, corte após o `rename` e refresh explícito dos dados de Brasília.
Cada callback LVGL somente enfileira uma
intenção já existente no `FlashCoordinator` e mostra a aceitação e o resultado
final do worker. O worker de flash permanece único dono de
LittleFS; a janela de 10 s e o corte de alimentação continuam físicos e o
resultado canônico continua no log `flash_coord`.

**Consequências:** a bancada não depende do RX serial para disparar os ensaios,
mas o monitor em modo somente-leitura ainda deve capturar os marcos. Os botões
não formatam `storage`, não liberam escrita automática, não alteram C6 e não
fecham G4 sem reboot, inspeção visual e evidência registrada.

**Recuperação adicional:** um reset durante o preenchimento pode deixar os
arquivos `full-probe.*` ocupando a partição. No próximo mount, o coordenador
remove exclusivamente esses temporários. As gerações `cache.0` e `cache.1`
continuam intocadas; falha nessa limpeza recusa o mount em vez de formatar
`storage`.

**Ajuste de bancada:** os botões G4 não exigem a carga sintética contínua.
Na unidade P4 v1.3, ela pode manter a task LVGL pronta a ponto de atrasar o
worker de flash, embora a UI normal permaneça renderizando e responsiva. Antes
de preencher o filesystem, o coordenador também remove `cache.tmp`, que é uma
proposta nunca promovida; isso impede blocos previamente alocados de fazer a
proposta do teste aparentar caber em uma partição cheia.

O preenchimento usa blocos de 16 KiB para o corpo do ensaio e, no primeiro
`ENOSPC` ou numa escrita parcial, reduz ao tamanho exato do snapshot real
(header mais payload de 25 B). Propostas adicionais continuam limitadas ao
máximo de 64 arquivos do gate. O ensaio só aprova erro efetivamente causado por
`ENOSPC`, exige limpeza confirmada, recuperação do espaço inicial e nunca faz
`rename` sobre `cache.0/1`.

## ADR-019 — Parsing estrutural, validação semântica e HTTP abortável

**Estado:** implementado em software em 2026-09-12; repetição HIL pendente.

**Contexto:** a auditoria A1–A6 reproduziu seleção de chaves JSON fora do
objeto correto, aceitação de documento truncado e umidade decimal, snapshots
fora de faixa, perda do sinal de valores entre -1 e 0, leitura HTTPS que só
avaliava limite após `perform()` e falso passe possível no ensaio cheio.

**Decisão:** os adapters usam um leitor JSON recursivo limitado a oito níveis,
validam o documento inteiro, objetos `current`/`bitcoin`, duplicatas e números.
Inteiros recusam ponto decimal; valores fixos arredondam na primeira casa
descartada. Encode/decode compartilham validação semântica, e a UI formata o
sinal pela magnitude. HTTPS usa `open`/`fetch_headers`/`read`, recalcula o
orçamento monotônico antes de cada operação e fecha o cliente assim que excede
bytes ou prazo. O ensaio cheio preserva a causa `ENOSPC`, usa o snapshot real e
exige limpeza e recuperação de espaço.

**Consequências:** quatro testes host cobrem codec, providers, cache e
formatação. O build P4 confirma integração, mas limites HTTPS sob chunks lentos
e o novo ensaio LittleFS continuam dependendo de bancada; G4 permanece aberto.

## ADR-020 — Preenchimento durável no ensaio de filesystem cheio

**Estado:** correção de software em 2026-09-12; gate físico permanece aberto.

**Contexto:** o operador relatou `ESP_ERR_INVALID_STATE` após 234.461 ms.
O teste host com LittleFS do componente fixado reproduziu a aceitação de
todas as 60 propostas pelo código anterior. O núcleo marca o arquivo como
`LFS_F_ERRED` quando a alocação falha e pode retornar zero de `file_sync`
sem salvar seus dados. Preencher até ENOSPC e só então sincronizar não
garantia que o volume continuava cheio após fechar o arquivo.

**Decisão:** sincronizar cada append bem-sucedido de até 16 KiB. Após ENOSPC,
fechar/reabrir a última versão persistida e reduzir o append até 49 B;
depois exercitar propostas reais de snapshot em arquivos descartáveis.
Não sincronizar para confirmar uma escrita que já falhou. Preservar a causa
ENOSPC, limpar todos os temporários em cada saída (tentando os restantes
mesmo se uma remoção falhar), verificar espaço e cabeçalho/CRC selecionado.
O orçamento de 180 s é cooperativo entre operações, com limpeza obrigatória.
Não altera defaults, modo de display, partições, dependências ou gerações
ativas; o worker existente continua único dono das operações de flash.

**Validação:** o mesmo código passou em flash NOR simulada com LittleFS
real de 9 MiB, cache técnico de 4 KiB, snapshot de 25 B, repetição/remount,
falha de I/O, falha de limpeza e timeout. Na configuração simulada, a
rejeição ocorreu após 49/50 propostas aceitas, com o espaço inicial
recuperado. Tempos e continuidade visual exigem nova bancada; nenhum
resultado host encerra G4. Rollback: reverter somente este incremento,
mantendo o ensaio cheio bloqueado até outra correção comprovada.

## ADR-021 — Serviço de hora confiável para o estado offline

**Estado:** implementado em software; validação física pendente.

**Contexto:** o refresh manual obteve DNS, NTP e HTTPS com `ESP_OK`, mas os
cards offline continuaram mostrando `hora nao confiavel`. A sincronização NTP
era apenas uma etapa do worker de manutenção e não publicava um estado
durável que a projeção da interface pudesse usar.

**Decisão:** o `TimeService` passa a ser o único dono de SNTP e publica,
de forma limitada, confiança, instante da última sincronização, duração e
resultado. O worker de rede o aciona depois de DNS e antes dos providers, sem
paralelizar conexões HTTPS. Após a primeira inicialização o serviço mantém
SNTP disponível; a projeção calcula a idade somente quando o serviço afirma
que a hora é confiável e o relógio está dentro da época válida.

**Consequências:** uma falha de NTP preserva os dados locais, mas sua idade
fica explicitamente não confiável. A UI não faz I/O nem inicia sincronização;
não há polling de providers ou segunda conexão TLS. O gate físico ainda deve
confirmar: NTP OK seguido de refresh mostra `ha 0 min`; após reboot sem hora
válida a indicação volta corretamente para `hora nao confiavel`.

## ADR-022 — Iniciar G5 por políticas portáveis e OTA P4 com C6 preservado

**Estado:** adotado para o primeiro incremento em 2026-09-12; integração OTA
e gate físico pendentes.

**Contexto:** G4 foi fechado no escopo da unidade. O bootloader P4 possui
rollback habilitado, mas a aplicação ainda não implementa confirmação de
`PENDING_VERIFY`. A reprodução C6 com lock, patch/hash e recuperação autônoma
também permanece uma exclusão de G0.

**Decisão:** implementar primeiro `update_policy`, C portável seguindo os
contratos já presentes neste firmware, sem IDF, LVGL, heap ou I/O. A política
de metadados aceita somente candidato P4 para produto/placa/revisão corretos,
imagem completa dentro do slot inativo, schema legível e a mesma security
version. Exige app atual confirmado, transação ociosa e C6 Hosted 3.0.6,
RPC v2, SDIO SW_AGGR. Recusa troca de C6, bootloader e partições nesta etapa.
Identificadores numéricos ainda precisam de registro de produto; a estrutura
em memória não define formato de manifesto e não deve ser serializada por ABI.

A política de boot recomenda confirmação somente após 15 s de saúde local
observada continuamente, com primeiro frame, display, serviços locais e
progresso de app/UI. A janela reinicia se faltar saúde ou houver intervalo
entre amostras maior que 500 ms. Progresso app/UI vence em 1000/500 ms;
timestamps futuros, anteriores ao boot e fonte ainda não observada não valem.
Em 60 s recomenda rollback se há fallback bootável, ou recuperação sem loop.
Relógio monotônico regressivo exige recuperação. Rede não participa desse
contrato. O adapter futuro deve iniciar o acompanhamento antes do bring-up,
observar progresso real e validar o resultado da escrita de `otadata` pelo
coordenador; recomendação em RAM não é confirmação persistida.

**Consequências:** `UPDATE_METADATA_MATCH` prova somente comparação de
metadados. Autenticação dos bytes exatos do manifesto, assinatura da app,
SHA-256, inspeção do header real, matriz de fallback e journal são gates
adicionais obrigatórios antes de qualquer ativação. Não existe verificador
criptográfico simulado nem caminho de atualização exposto à UI neste corte.
O roteiro de implementação e bancada está em `G5-VALIDATION.md`.

**Validação/rollback:** executar `tools/run_update_policy_host_test.ps1` e
build limpo P4. O incremento não modifica defaults, locks ou partições e não
executa gravação OTA; o fluxo do repositório grava o P4 de desenvolvimento
após mudanças de firmware e registra a evidência separadamente. Reverter o
módulo e sua entrada CMake remove somente a política ainda sem consumidores;
G5 permanece aberto até os ciclos físicos assinados.

## ADR-023 — Recipe C6 fixada a partir do exemplo oficial 3.0.6

**Estado:** adotado em 2026-09-13; reprodução de software em execução,
ativação na unidade bloqueada pelo gate de recuperação C6.

**Contexto:** a recipe anterior apontava para o exemplo em managed_components,
com dependência Hosted `*` e sem lock C6. O SDK instalado já possui a mudança
SDIO, mas não tem histórico Git disponível para identificar seu diff. O
exemplo remoto usa dois slots de `0x1C0000`, menores que o staging P4 de 2 MiB.

**Decisão:** tornar `coprocessor/` um projeto ESP-IDF mínimo e versionar somente
bootstrap, defaults e tabela do exemplo oficial, com manifesto Hosted
`==3.0.6`/IDF `==5.5.4` e lock próprio. Preservar a configuração funcional do
exemplo neste corte, explicitando que não é imagem de produção. Registrar
proveniência dos arquivos copiados; não duplicar o componente Hosted.
Ativar `CONFIG_APP_REPRODUCIBLE_BUILD=y`, mecanismo suportado pelo IDF para
retirar timestamps e caminhos variáveis da imagem, e exigir dois builds limpos
em diretórios diferentes com hash de app/bootloader/tabela iguais.
Antes do build, comparar o arquivo SDIO completo ao original da tag IDF 5.5.4,
aceitando somente o patch oficial de uma linha, normalizando apenas CRLF/LF.
Invocar `eh.py patch-idf` e repetir para provar idempotência; registrar hashes
brutos, normalizados e diff. Qualquer outra diferença bloqueia a compilação.

**Limites:** não habilitar rollback por Kconfig isoladamente: falta a saúde
autônoma C6 e não está comprovado o bootloader instalado. Não ativar Secure
Boot, assinatura exigida pelo bootloader, encryption ou anti-rollback. O
bootstrap upstream faz erase NVS em erros de inicialização; esse comportamento
será corrigido no caminho de produto antes de sua promoção. A reprodução não
autoriza gravar essa imagem na unidade. C6 somente via Slave OTA por SDIO,
após inventário e recovery; não executar o alvo `flash` do projeto C6 na COM8.

**Validação:** build limpo C6, configuração efetiva, tabela gerada, tamanho da
app, hashes, lock e relatório de recursos. SHA-256 do build local não é hash
do rádio instalado. Gates físicos continuam abertos. Reverter esta recipe
não altera firmware gravado no P4 ou C6.

## ADR-024 — Manifesto P4 canônico e autenticação sem acesso privado ao C6

**Estado:** parser canônico, verificador RSA-PSS e sessão de hash streaming
implementados em software em 2026-09-13; keyring de produção, integração com
HTTPS/flash e ativação OTA pendentes.

**Contexto:** G5 precisa rejeitar pacotes ambíguos antes de qualquer erase ou
seleção de slot. JSON aceitaria ordenações, espaços, escapes e duplicatas que
complicam a definição dos bytes assinados. Ao mesmo tempo, o host Hosted 3.0.6
em uso só retorna a versão C6 pelo descritor público, embora o C6 possua um
RPC interno mais completo; nem uma resposta completa fornece bootloader ou
partições instaladas.

**Decisão:** usar um envelope P4-only de 104 bytes, versão 1, com endianness e
offsets fixos, campos reservados obrigatoriamente nulos e assinatura destacada
sobre os bytes recebidos sem reserialização. O parser não aloca memória e
recusa ID/SHA nulos, faixas invertidas, flags desconhecidas e operações sobre
C6, bootloader ou tabela. Não chamar headers privados do Hosted nem modificar
o componente fixado para obter metadados adicionais do C6.

**Consequências:** o verificador exige RSA-3072/PSS/SHA-256 com MGF1 SHA-256 e
salt de 32 B sobre os bytes canônicos, usando chave pública DER injetada pelo
adapter/keyring. Ele não contém chave privada, não persiste chaves e não
autoriza sozinho uma imagem. A sessão de SHA-256 mantém no máximo o estado do
digest e aceita blocos de até 4 KiB, exigindo tamanho exato e digest final
igual ao manifesto; ela não possui transporte nem grava flash. Keyring de
produção, rotação/revogação, registro de anti-replay e integração com a
política de metadados/HTTPS/FlashCoordinator continuam obrigatórios. A operação
conjunta C6 permanece bloqueada pela inspeção física de layout e recovery, sem
USB/UART como rota alternativa. O formato v1 não é persistido nem exposto à
UI; qualquer extensão incompatível exige nova versão.

**Validação/rollback:** o teste host cobre tamanho, magic/versão, bytes
reservados, destino, flags, ID/SHA nulos e faixas. Um vetor efêmero RSA-3072
validado por OpenSSL cobre aceitação da assinatura e recusa após mudança de
byte; sua chave privada é removida após a execução. Um vetor SHA-256 em blocos
independentes confirma o digest esperado. Reverter `update_manifest.c/.h`,
`update_signature.c/.h`, `update_image_hash.c/.h` e entradas CMake remove
somente código sem consumidor de I/O; não altera slots, NVS, C6 ou eFuses.

**Complemento em 2026-09-13:** o keyring e a admissão agora vinculam parser,
assinatura, política, anti-replay e hash antes de abrir flash. A gravação P4
fica em um adapter que depende apenas do componente nativo `app_update` do
ESP-IDF 5.5.4. O `FlashCoordinator` persiste o journal em duas cópias NVS e
atribui suas gerações; recusa corrupção, empate com dados distintos, salto de
estado e estouro. O próprio coordenador compara o registro solicitado à cópia
NVS antes da seleção; esta requer, cumulativamente, finalização de
`esp_ota_end` e o registro persistido `P4_PENDING`. Nenhum desses adapters é
acionado pela UI ou por boot normal enquanto não houver artefatos de release e
os ensaios físicos obrigatórios. A decisão não muda os bloqueios do C6.

## ADR-025 — EEZ Studio como fonte visual de Boot e Home

**Estado:** substituído em 2026-09-14 pela ADR-027.

**Contexto:** a UI de produto foi desenhada no EEZ Studio com LVGL 9.5, mas o
firmware ainda criava o dashboard manualmente. Duplicar a árvore visual em C
faria o editor e a placa divergirem e aumentaria o risco de invalidações
desnecessárias durante atualizações de rede e dados.

**Decisão:** `ui/NP2.eez-project` é a fonte de verdade da árvore visual, e o
Build do EEZ gera arquivos somente em `firmware/main/ui_generated`. O módulo
`eez_ui` é a ponte de runtime: ele cria Boot e Home sob o lock do adapter,
consome apenas a projeção sanitizada de `AppState`, compara textos antes de
alterá-los e mantém todo acesso a objetos na task LVGL. Boot abre sem animação
e só troca para Home depois do tempo mínimo, quando a hora é confiável ou após
10 s com estado local pronto; o limite preserva o funcionamento offline. A
gaveta lateral usa uma animação limitada de 96 px. Os campos ainda ausentes do
modelo, como dólar, Ibovespa, vento, sensação e UV, aparecem indisponíveis em
vez de reutilizar valores do protótipo.

**Nota de implementação:** a Home possui identificadores EEZ estáveis para
seus bindings. A página Boot, no arquivo atualmente aberto no Studio, ainda
não os preserva ao gerar; por isso o bridge acessa somente seus dois labels
dinâmicos pela ordem fixa documentada dos filhos. Não mover ou inserir filhos
em Boot sem antes nomeá-los e regenerar a tela.

**Consequências:** serviços, HTTPS, NTP e persistência continuam fora da UI; o
arquivo gerado não recebe lógica de produto. Novos dados exigem primeiro um
contrato em `AppState` e só depois o binding visual. Regenerar o projeto EEZ é
parte do fluxo antes do build ESP-IDF. A mudança preserva RGB565, rotação 180°,
três framebuffers e `TRIPLE_PARTIAL`; build e flash não demonstram ausência de
glitch, que depende de observação física com a tela atualizando.

**Validação/rollback:** executar o Build do EEZ, build limpo P4, flash pela
porta do P4 e capturar boot, transição, relógio e atualizações dos cards. Para
rollback, restaurar `offline_dashboard_create` em `board_bringup.c`; nenhum
estado, cache, credencial, partição ou firmware C6 é alterado.

## ADR-027 — UI LVGL compartilhada entre design e firmware

**Estado:** adotado em 2026-09-14.

**Contexto:** a edição visual no EEZ Studio criou duas fontes de verdade, uma
ponte extensa entre objetos gerados e o estado da aplicação e falhas recorrentes
de geração, inicialização e manutenção das telas.

**Decisão:** remover o projeto EEZ, seus arquivos gerados, imagens embutidas,
scripts de regeneração e a ponte de runtime. As fontes LVGL geradas que contêm
os glifos latinos e Material Symbols são preservadas em `firmware/main/ui/fonts`.
As telas continuam como código LVGL próprio, criado e atualizado exclusivamente
na task LVGL e alimentado pela projeção de `AppState`. Tokens, estilos,
componentes e telas ficam em `firmware/main/ui/core` e
`firmware/main/ui/screens`; o firmware os compila diretamente. O controlador
`product_ui` transforma estado em conteúdo, sem repetir layout. `design/` fica
restrito a mockups, imagens e outras referências visuais.

**Consequências:** o build do firmware deixa de depender do EEZ Studio e não
incorpora mais a imagem de fundo gerada. O dashboard LVGL direto volta a ser a
tela ativa. As regras de concorrência, três framebuffers, `TRIPLE_PARTIAL`,
rotação, serviços e persistência permanecem iguais.

**Validação/rollback:** executar build P4, gravar pela porta do P4 e confirmar no
boot a mensagem `Direct LVGL UI active`. O histórico das validações antigas do
EEZ permanece em `docs/BRINGUP-EVIDENCE.md` apenas como evidência histórica.

## ADR-026 — Wizard inicial com perfil sem segredo

**Estado:** infraestrutura de serviço preservada; interface EEZ removida pela
ADR-027 em 2026-09-14.

**Contexto:** o primeiro uso precisa coletar rede, fuso e formato de hora antes
da Home, sem tornar uma senha WPA2 parte do estado da aplicação ou da
persistência normal.

**Decisão:** o wizard possui etapas Wi-Fi, senha, hora e resumo. A senha
permanece somente nos buffers internos do `provisioning_service`, é apagada
depois do envio ao executor de conectividade e não é copiada para widgets,
`AppState`, eventos ou logs. O `FlashCoordinator` persiste em duas gerações
somente o perfil não secreto: conclusão, formato 24 h e índice de fuso. O
produto pode concluir o wizard offline. Reconexão automática após reboot com
WPA2 permanece bloqueada até existir um cofre de credenciais revisado, com
proteção criptográfica e gate próprio.

**Consequências:** a futura UI LVGL direta poderá consumir os serviços de
onboarding. NVS continua acessada exclusivamente pelo coordenador, em uma
requisição de fila limitada.

O fuso pode ser atualizado posteriormente pela Settings: o callback apenas
registra a intenção no `onboarding_service`; o `app_loop` aplica a política de
hora local e o serviço solicita a gravação serializada ao `FlashCoordinator`.
O perfil permanece independente do schema de dados offline.

**Validação/rollback:** os serviços permanecem cobertos pelo build P4. A UI do
wizard deverá receber validação própria quando for reimplementada sem EEZ.

## ADR-028 — Acionamento OTA P4 de laboratório e saúde antes do display

**Estado:** implementado em 2026-09-15; ciclo HTTPS e bancada OTA ainda abertos.

**Decisão:** `OTA_STATUS`, `OTA_PREFLIGHT` e `OTA_APPLY` passam pela janela
USB de manutenção existente. Host, três URLs e chave pública RSA-3072 são
imutáveis na build, por header gerado com `configure_p4_development_ota.py`;
sem esse header a aplicação/preflight devolve `ESP_ERR_NOT_SUPPORTED`.
Não receber URL, chave ou credencial pelo comando. O pacote de laboratório
expõe somente imagem, manifesto e assinatura em `public/`; chave privada e
DER ficam fora dessa raiz de publicação. Nenhuma dependência/Kconfig muda.

O worker HTTPS confere novamente versão C6, revisão P4, tamanho do slot,
security version e estado VALID da aplicação atual antes da admissão.
RPC v2/SW_AGGR continuam sendo o perfil de bancada fixado, não uma atestação
de hash/layout C6. A geração persistida é incorporada ao journal recalculando
seu CRC antes da seleção. Erases P4 passam a ser incrementais pela API IDF.

O coordenador e o supervisor iniciam antes do display. Um timer da própria
task LVGL sinaliza progresso também na tela estática, mantendo o primeiro
render como condição separada. Enfileirar confirmação não significa sucesso:
o supervisor observa o estado real de otadata. Falha de confirmação conduz ao
deadline; o coordenador revalida o fallback antes de invalidar o candidato.
Se o coordenador não responde, registrar recovery bloqueado em até 65 s sem
inventar recuperação concluída. Esse caso ainda exige implementação/ensaio
de recuperação local antes de G5.

**Limites:** o primeiro slot serial sem estado VALID precisa ser qualificado
como fallback antes de OTA. Journal pós-corte, falha tardia após VALID,
manutenção visual durante OTA e recuperação conjunta C6 seguem gates abertos.
Os testes host usam falhas determinísticas; não substituem ciclos na placa.

## ADR-029 — CredentialVault protegido para Wi-Fi

**Estado:** implementado; ativação física de segurança e validação de reboot
pendentes.

**Contexto:** a credencial WPA2 precisa sobreviver a reboot sem virar dado de
UI, log, evento ou armazenamento implícito do driver. Salvar em NVS comum não
protege a senha diante de acesso físico e não atende ao produto.

**Decisão:** `CredentialVault` preserva `WIFI_STORAGE_RAM` no driver e grava
somente pela fila do `FlashCoordinator`, após 30 s contínuos com IP. O vault
mantém duas gerações CRC no namespace `np2_credentials` e restaura apenas para
a mailbox privada da conectividade no boot. `FORGET` remove as duas gerações;
uma tentativa de troca rejeitada descarta só o candidato e preserva a última
rede confirmada. Não existe getter para UI, `AppState`,
eventos ou USB; os buffers temporários são zeroizados.

Na build de produção, o vault recusa ler, gravar ou inicializar uma chave
enquanto a imagem não foi compilada com `CONFIG_NVS_ENCRYPTION=y` e o P4 não
reporta Flash Encryption ativa. A inicialização usa `nvs_flash_init()`,
permitindo ao IDF carregar a chave por unidade na partição `nvs_keys` e abrir
NVS de forma cifrada. A partição de chaves deve ter flag `encrypted` no perfil
de produção. Para a unidade desbloqueada de desenvolvimento, a opção CMake
explícita `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION` mantém a mesma política
de cópia e CRC na NVS local; ela não é perfil de produção.

**Ativação:** Secure Boot RSA, Flash Encryption e NVS Encryption devem ser
ensaiados primeiro numa placa dedicada, depois de fechar os gates de OTA e
recovery. A queima de eFuses exige autorização humana explícita e não é feita
por esta mudança. Até essa ativação, o painel continua associando apenas na
sessão atual, em vez de gravar uma senha sem proteção.
O procedimento, perfil exigido e critérios de bancada estão em
`CREDENTIAL-VAULT-PRODUCTION.md`.

## ADR-030 — Sincronização automática e serializada de hora, clima e Bitcoin

**Estado:** implementado em 2026-09-22; requer observação física contínua.

**Contexto:** o painel já possuía `TimeService`, adapters Open-Meteo e
CoinGecko, cache offline com duas gerações e um executor HTTPS único. Porém,
o refresh era disparado apenas por manutenção. Com a estação WPA2 persistente
em desenvolvimento, a Home continuava exibindo valores indisponíveis até uma
intervenção manual, contrariando o fluxo de produto `IP → NTP → HTTPS →
providers → cache`.

**Decisão:** o `network_validation_service` agenda o refresh do snapshot ao
receber uma estação online e a cada 30 minutos enquanto ela permanecer com IP.
Cada rodada resolve DNS, sincroniza NTP pelo `TimeService`, consulta
sequencialmente clima de Brasília e BTC/USD e envia o snapshot validado ao
`FlashCoordinator`. Qualquer falha espera dois minutos antes da próxima
tentativa; sem IP não há DNS, NTP ou HTTPS. Pedidos de diagnóstico e OTA
continuam compartilhando o mesmo worker, portanto há no máximo um handshake
TLS em voo. A política de escrita do cache continua limitada pelo coordenador
a uma geração a cada 30 minutos.

**Consequências:** `AppState` e a UI permanecem consumidores passivos: o
relógio usa apenas a hora confiável, e os cards recebem somente o snapshot
sanitizado e seu estado live/stale. URLs, respostas HTTP, JSON e credenciais
não entram em UI, eventos ou logs de produto. Uma falha preserva o último dado
válido como stale; não bloqueia a Home nem reinicia o P4/C6.

**Validação/rollback:** build limpo P4, flash somente da app e boot em rede
WPA2 devem confirmar NTP, os dois providers e o cache sem reinício ou perda de
responsividade. Reverter o agendamento no `network_validation_service` volta
ao refresh manual sem alterar a estrutura do cache, a NVS, o C6 ou eFuses.

## ADR-031 — Fontes da UI pertencem ao firmware

**Estado:** implementado em 2026-09-22.

**Contexto:** as fontes C da interface estavam em `design/v5`, embora fossem
compiladas pelo firmware. Isso confundia referências visuais com código de
produção e dificultava localizar a UI que executa no painel.

**Decisão:** os fontes executáveis da UI ficam em `firmware/main/ui`: tokens,
estilos e componentes em `core/`; as telas e o controlador em `screens/`; e
as fontes tipográficas em `fonts/`. `firmware/main/CMakeLists.txt` compila
somente esses caminhos. `design/` contém mockups, imagens, scripts de geração
de referência e exports visuais, sem participação no build do firmware.

**Consequências:** a organização não altera o desenho das telas, o fluxo de
`AppState` ou a regra de acesso exclusivo da task LVGL. Mudanças de UI de
produto passam a ser feitas sob `firmware/main/ui`; arquivos em `design/`
continuam úteis para comparação visual, mas não atualizam o painel.

**Validação/rollback:** build limpo e flash P4 verificam que os novos caminhos
de compilação preservam o boot e a Home. Reverter este commit restaura os
caminhos anteriores sem mudar contratos, dados persistidos ou o C6.

## ADR-032 — Fonte visual XML para o LVGL Editor oficial

**Estado:** preparado em 2026-09-22; exportação e adoção no runtime pendentes.

**Contexto:** a UI compilada do NovaPanel é escrita diretamente em C/LVGL 9.5,
o que não permitia editar visualmente as telas no VS Code. O projeto precisa
adotar o formato oficial XML sem introduzir outro editor ou esconder a UI em
`design/`.

**Decisão:** `firmware/main/ui/lvgl` é o projeto do LVGL Editor: `project.xml`
fixa o display de 1024 × 600 no esquema compatível com a extensão instalada;
`globals.xml` contém tokens e
subjects; e `screens/` contém Boot e Home. `NovaPanel-UI-LVGL.code-workspace` abre essa
pasta como raiz adicional no VS Code, condição requerida pela extensão
`LVGL.lvgl-editor`. A versão C atual em `firmware/main/ui/core` e `screens`
continua ativa até a exportação gerar C revisável.

**Consequências:** editar XML no editor oficial não modifica a placa por si só.
Após cada exportação, os arquivos gerados entram em
`firmware/main/ui/generated`, enquanto a ponte de `AppState` fica em arquivos
não gerados. Só então o CMake passa a compilar a exportação, seguido de build,
flash e captura de boot. Arquivos `*_gen.c` e `*_gen.h` nunca recebem lógica
manual porque são substituídos pelo editor.

**Validação/rollback:** os XMLs devem abrir no LVGL Editor e mostrar os dois
alvos no preview. Enquanto a exportação não estiver integrada, o firmware em
execução não muda; remover `firmware/main/ui/lvgl` e
`NovaPanel-UI-LVGL.code-workspace`
desfaz somente o ambiente visual.

## ADR-033 — Cadência serial por domínio para clima, dólar e Bitcoin

**Estado:** implementado e validado em build/flash/boot em 2026-09-23;
continua pendente a observação do ciclo completo de cada provider com Wi-Fi
configurado.

**Contexto:** a ADR-030 atualizava o snapshot inteiro a cada 30 minutos. Isso
mantinha um único handshake, mas atrasava BTC/USD e consultava clima mais vezes
do que o uso do painel exige. A Home também exibia o dólar como indisponível.

**Decisão:** o worker HTTPS mantém uma agenda portável de domínios vencidos e
executa apenas um por vez: BTC/USD a cada 5 min, clima de Brasília a cada 2 h
e USD/BRL a cada 24 h. O dólar usa a série diária 1 (venda/PTAX) do Banco
Central do Brasil, sem token e com URL fixa. Falhas tentam novamente de forma
independente após 2 min (BTC), 15 min (clima) ou 1 h (dólar). Quando mais de
um domínio vence, o agendador alterna a escolha; nenhum DNS, NTP, TLS ou corpo
HTTP de uma atualização se sobrepõe a outro. OTA e diagnósticos permanecem no
mesmo worker e, portanto, também são exclusivos.

O snapshot v2 acrescenta USD/BRL em ten-thousandths e é publicado por evento
sanitizado para o `app_loop`. A UI passa a ver o resultado em RAM logo após
cada provider, enquanto a gravação do snapshot agregado em LittleFS continua
limitada globalmente a uma vez por 30 min. O decoder aceita o cache v1 como
snapshot v2 sem dólar, preservando seu valor de fallback até a primeira escrita
nova.

**Consequências:** a hora é sincronizada no primeiro uso e no máximo a cada
seis horas, em vez de uma vez por cotação BTC. A data exibida para PTAX é a
hora da obtenção confiável da última taxa publicada; finais de semana e
feriados preservam a última taxa, com stale após 36 h. O firmware não promete
cotação de câmbio em tempo real. O cache de 30 min é deliberadamente mais
conservador que a atualização em RAM para reduzir risco de flash/render.

**Validação/rollback:** testes host cobrem parser BCB, codec v2 e a ordem/tempo
do agendador. Build P4, flash pela porta do P4 e bancada devem confirmar a
ordem serial, os três valores na Home, ausência de segundo TLS e UI responsiva
durante pelo menos um ciclo de cada domínio. Reverter os módulos de agenda,
provider BCB e schema v2 retorna ao snapshot clima/BTC da ADR-030; não altera
C6, credenciais, partições ou eFuses.

## ADR-034 — Retenção Wi-Fi como padrão do fluxo de desenvolvimento

**Decisão:** `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION` passa a ter padrão
`ON` no CMake do repositório. Assim, builds limpos, flash e monitor executados
pelo assistente no P4 desbloqueado preservam a credencial de laboratório sem
depender de um cache CMake anterior. O perfil de produção deve passar
explicitamente `-DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=OFF` e atender os
gates de NVS Encryption e Flash Encryption.

**Motivo:** o fluxo real desta unidade é feito pelo assistente no VS Code,
incluindo reconfiguração e `fullclean`; o padrão anterior `OFF` podia desativar
silenciosamente a reassociação após um build novo. O custo aceito é que esta
árvore não deve ser usada sem a flag de produção para uma imagem de campo. Só
credenciais descartáveis de laboratório podem existir no P4 de desenvolvimento.

## ADR-035 — Background meteorológico limitado ao card da Home

**Decisão:** a Home volta a usar `np_scene` neutra e concentra a ambientação
meteorológica no `weather_card` de 500 × 462 px. O card cria uma imagem RGB565
fixa, um overlay escuro e os widgets existentes; a source é trocada no próprio
widget, nunca com reconstrução da Home. A classificação Open-Meteo passa por
`weather_condition_from_code()`, compartilhada pela descrição e pelos assets.
O fallback de período é dia das 06:00 às 17:59 e noite no restante, isolado em
`np_home_is_day()` para futura troca por sunrise/sunset.

**Motivo:** uma imagem de tela inteira aumentava a área invalidada e misturava
o estado climático com toda a cena. Manter 20 imagens opcionais em flash
(dia/noite × 10 condições) conserva a leitura do texto, limita a atualização à
região do card e não reserva RAM nem executa conversão/redimensionamento. Sem
os binários finais, a imagem fica oculta e o card escuro continua funcional.

**Consequências:** os 20 `.bin` RGB565 precisam ter exatamente 500 × 462 px.
Vento, sensação, UV e Ibovespa permanecem indisponíveis até que seus contratos
de dados sejam adicionados; a UI não sintetiza valores. A mudança é reversível
removendo o mapeador/asset opcional e restaurando o backdrop anterior, sem
tocar em cache, rede, C6, credenciais ou eFuses.

## ADR-036 — Backgrounds meteorológicos da Home no microSD

**Estado:** implementado; uma captura inicial confirmou a coexistência de
SDMMC, ESP-Hosted/Wi-Fi e HTTPS. O gate de estresse prolongado permanece
pendente.

**Contexto:** os vinte backgrounds RGB565 de 500 × 462 px somam 9,24 MiB e não
caberiam na menor partição OTA de 8 MiB. Embuti-los reduziria a margem de OTA e
subiria o consumo de flash sem melhorar a atualização visual. O hardware expõe
um microSD em SDMMC separado, mas o plano exige provar que ele não interfere no
Hosted/SDIO do C6 antes de torná-lo requisito de produto.

**Decisão:** os arquivos ficam no cartão em `/sdcard/np2/weather/`, com os
nomes `np_bg_day_*` e `np_bg_night_*`. `weather_asset_service` é o único dono
da montagem e da leitura; usa o BSP da Waveshare, que mantém
`format_if_mount_failed=false`. O `app_loop` apenas deposita na mailbox a hora
e o `weather_code` já sanitizado. O worker resolve período e condição pelo
catálogo comum, lê somente o arquivo selecionado em dois buffers RGB565 fixos
em PSRAM (924.000 B) e publica um descritor para a projeção. A task LVGL não
toca no SD: só troca a source do `lv_image` existente e invalida o card.
Se o cache de clima e a seleção visual persistida existirem mas o NTP ainda não
estiver confiável, o app_loop reutiliza o último período confirmado; sem essa
seleção, preserva o card neutro. Quando a hora chega, a mesma rota troca
somente o asset para o período real.
O profile P4 habilita `CONFIG_FATFS_LFN_HEAP=y` com `CONFIG_FATFS_MAX_LFN=64`:
sem LFN, o FATFS reduz esses nomes a aliases 8.3 e a seleção deixa de encontrar
o arquivo original.

**Consequências:** cartão ausente, FAT inválido, arquivo ausente ou tamanho
divergente preservam o card escuro sem formatar, gravar ou bloquear a Home. Os
arquivos-fonte são provisionados por `tools/copy_weather_assets_to_sd.ps1`;
flash P4 não copia assets para o cartão. Cada mudança posterior de clima ou
dia/noite reutiliza o worker e não recria widgets. A entrada definitiva em
produto depende do ensaio simultâneo de Wi-Fi/HTTPS e leitura SDMMC previsto
em `RESTART-HARDWARE-BRINGUP.md`; falha nesse ensaio exclui o visual do SD da
release e conserva o fallback neutro.

## ADR-037 — Seleção visual meteorológica persistida no cache offline

**Decisão:** o snapshot offline v3 passa a guardar somente se o último visual
meteorológico confirmado era dia ou noite. O `weather_code` permanece no mesmo
snapshot e continua sendo a fonte para a condição. No boot sem hora confiável,
a Home carrega essa seleção persistida; sem ela, conserva o card neutro. Depois
de NTP, o `app_loop` calcula a seleção atual e pede somente a troca do asset.

**Motivo:** usar um período diurno arbitrário no boot escondia o estado real do
produto e podia apresentar uma cena incoerente. Persistir um enum de dois
estados, em vez de bitmap ou segundo filesystem, preserva a experiência
offline sem custos de RAM/flash relevantes e sem I/O da UI.

**Compatibilidade e limites:** o decoder continua aceitando snapshots v1 e v2;
eles não têm seleção visual e, sem NTP, mantêm o fallback neutro até a próxima
atualização válida promovê-los a v3. A seleção é atualizada pelo worker de rede
somente após hora válida e passa pelo `FlashCoordinator`; não há escrita em
callback LVGL nem por toque.

## ADR-038 — Ícones Meteocons animados no card de clima

**Decisão:** os 20 SVGs fornecidos em `design/clima/` são a fonte versionável.
Uma ferramenta local usa o mecanismo SMIL de Chrome/Edge para amostrar cada
animação em 96 × 96 px e 8 fps. Cada ícone vira um único pacote `NPWI` v1 com
quadros BGRA8888 e CRC32, provisionado em `/sdcard/np2/weather/icons/`.
O worker SD existente carrega e valida somente o pacote selecionado em um dos
dois slots de PSRAM; a task LVGL apenas alterna os descritores no `lv_animimg`.
O mapeamento segue `weather_condition` e a seleção persistida de dia/noite.

**Motivo e limites:** a placa não precisa de rede nem de interpretador SVG para
animar, e os quadros são gerados sem edição manual. Os 20 arquivos ocupam
aproximadamente 26 MiB no cartão; dois slots reservam até 3,4 MiB de PSRAM
quando os pacotes estão presentes. O visual conserva fallback estático se o
cartão, o pacote ou sua validação falhar. O uso de PSRAM e o render em 8 fps
precisam de medição em bancada junto com SDMMC, Wi-Fi e HTTPS antes de aceite.

## ADR-039 — Detalhes da Home no snapshot offline v4

**Decisão:** promover o contrato offline para v4 com disponibilidade explícita
para sensação térmica, vento, UV, máxima/mínima e volume em USD do BTC, e
variação diária da PTAX. Open-Meteo fornece os três detalhes de clima no mesmo
request; CoinGecko `/coins/markets` fornece o resumo de 24 h do Bitcoin; o BCB
SGS 1 fornece as duas últimas PTAX, cuja variação é calculada em pontos-base.
Todos permanecem no executor HTTPS único e no cache de duas gerações. A Home
identifica o dólar como PTAX. O Ibovespa permanece indisponível a pedido do
responsável, sem cotação sintética ou token embutido.

**Compatibilidade:** o codec v4 preserva os primeiros 34 bytes do payload v3,
aceita registros v1 a v3 e promove os campos novos como indisponíveis até uma
resposta válida. O payload v4 tem 169 bytes fixos, com limites e CRC do cache
existente. A migração não formata armazenamento nem escreve a partir da UI.

## ADR-040 — Settings Wi-Fi com senha privada e desenho temporário

**Decisão:** manter somente um modal principal lazy de Settings, e no fluxo
Wi-Fi criar sob demanda apenas um diálogo filho compacto. Gerenciar redes
faz scan no próprio modal. Seleção segue o SSID público, e o Status recebe
somente a associação real publicada pelo rádio → serviço → `app_loop` → UI.

O diálogo de senha usa o teclado matricial persistente da tela, que é trazido
para frente do diálogo durante a entrada, sem textarea. A UI guarda apenas
SSID público, comprimento e visibilidade; `product_ui` faz a ponte das teclas
para o buffer privado do `provisioning_service`. O responsável autorizou em
2026-09-28 Mostrar senha por desenho temporário sem texto em widgets: cada
caractere é consultado separadamente, convertido em um glyph no passe de
render e nunca concatenado em label, evento, AppState ou log. Descritores de
desenho e pixels necessariamente existem durante render/exibição; isto não
torna dumps brutos de memória seguros para exportação.

**Limites:** máscara por padrão, 8–63 ASCII imprimíveis para rede protegida,
senha vazia somente para rede aberta indicada pelo scan. Cancelar, fechar,
sair de Settings ou submeter zeram a sessão e revogam visibilidade. Transferência
continua exclusivamente pela mailbox privada de conectividade; persistência
usa a política de vault/FlashCoordinator existente. Sem mudança no schema
offline, WDT, C6 ou configurações de hardware. Build/teste host não fecham
validação de toque, render, recuperação ou estabilidade na placa.

## ADR-041 — Modo noturno local e fechamento de Settings

**Decisão:** Modo noturno aplica o horário local configurado, das 22:00 às
06:00, e limita o brilho efetivo a 15%, conservando a preferência diurna.
Não aumenta um brilho abaixo do limite nem altera volume/notificações. Sem
hora confiável, conserva o brilho escolhido e informa espera de sincronismo.
O worker de controles aplica o efeito; UI emite a preferência e consome a
projeção do app_loop. A preferência usa o perfil pequeno de controles no
FlashCoordinator, com leitura compatível do payload antigo de dois bytes
(modo desabilitado). O schema de dados offline permanece igual.

Sistema apresenta versão real e temperatura interna do chip, com ausência
explícita se a leitura falhar. Reinício exige confirmação e execução pelo
coordenador de flash, recusando persistência pendente/OTA em andamento. OTA
de produto permanece no gate G5; Settings indica manutenção em vez de
oferecer um botão de atualização sem função. Validação física continua
dependente do flash manual e resultados do operador.

## ADR-042 — Perfil e Preferências com fallback incremental

**Decisão:** introduzir duas cenas leves, Perfil e Preferências, usando o
header e os tokens existentes. Há um único perfil, sempre principal, com
saudação obrigatória. Não existem seleção de perfil padrão nem controle de
saudação. Preferências é um hub de cinco linhas, sem controles locais.

**Motivo:** separar identidade e navegação das configurações sem substituir
serviços ou redesenhar Home. Nesta primeira fase, as cinco linhas abrem a
Settings anterior, ainda compilada e com seus modais lazy. O botão Perfil
no hub permite abrir a cena de identidade; cada cena libera a anterior antes
de construir a próxima, na task LVGL, após o callback de toque.

Não há fonte de identidade no AppState atual. Nome e avatar aparecem como
não configurados; Editar/Nome/Avatar explicam que a edição fica para a fase
de identidade. A saudação é "Olá!" até existir nome real. Localização é
omitida: a cidade fixa do provider de clima não é localização do usuário.
Não há backend, preferência persistida ou schema novo nesta fase.

Teclado e feedback continuam compartilhados. A saída cancela os trabalhos
adiados conhecidos dos modais e da construção de Settings antes do descarte.
Não são criadas antecipadamente as cinco futuras telas. WDT, display, C6,
partições e os contratos de persistência permanecem no baseline existente.
Build não comprova estabilidade física; o flash é manual pelo operador.
