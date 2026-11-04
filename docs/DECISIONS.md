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

**Recuperação de boot (2026-09-30):** após observar que desligar a placa
recupera um timeout de OCR que sobrevive a um reset quente, a inicialização
opcional do microSD passou a ocorrer antes de iniciar ESP-Hosted. O worker
desliga a LDO do cartão por 500 ms, aguarda 1 s de estabilização e tenta
montar no máximo duas vezes para timeout ou resposta inválida. `app_main`
espera essa janela limitada de 3,5 s antes de iniciar o C6; a LVGL já está
ativa nesse ponto. O controlador SDMMC compartilhado não é reinicializado
globalmente, pois isso poderia interromper a conexão do C6. A montagem mantém
o mesmo slot, frequência, LDO e política FAT do BSP, sem formatação. Em falha
final, libera a LDO e conserva o ícone estático. Essa recuperação não atesta
a causa elétrica do timeout nem substitui ensaios de reboot frio/quente e SD
junto de Wi-Fi na placa.

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

## ADR-043 — Sistema com uma cena lazy reutilizada

**Decisão:** a migração de Sistema mantém o modal legado compilado e adiciona
uma cena específica em `np_settings_system.c/.h`. Labels, handles de firmware,
temperatura e reinício e a sincronização existente são compartilhados. O
callback de reinício continua pedindo confirmação e enfileirando o comando
pelo `FlashCoordinator`; nenhuma operação lenta foi transferida para a UI.

**Motivo e trade-off:** por solicitação do produto, Sistema é construído só
na primeira visita e conserva uma única árvore oculta entre entradas. Os
callbacks são instalados uma vez. A saída cancela o trabalho adiado e descarta
a confirmação de reinício, fecha o drawer e oculta a raiz. Esse cache é
limitado a uma cena, custa heap LVGL residente após a primeira visita e não
autoriza construir todas as telas futuras. As demais cenas mantêm o lifecycle
da migração anterior. Tempo e contagem de objetos são instrumentados para
bancada; a retenção real de heap e WDT dependem do flash manual.

Display e tipo de touch representam o baseline real, sem afirmar saúde do
touch por texto "OK". Firmware e temperatura vêm da projeção atual; sem leitura,
a temperatura permanece indisponível. Uptime, IP e RSSI não são inventados.
Atualizar sistema fica desabilitado e mantém a indicação de manutenção,
pois não existe fluxo de atualização de produto aprovado nesta tela.

## ADR-044 — Wi-Fi dedicado com pool e credencial privada

**Decisão:** ampliar `np_settings_wifi.c/.h` com uma cena específica, preservando
o modal legado e o provisionamento inicial/manutenção em `wifi_setup_view`.
Preferências abre essa cena; Voltar retorna ao hub. A árvore é criada somente
ao entrar e liberada ao sair, com quatro rows reutilizados e dez páginas no
limite atual de 40 resultados. Não existe construção eager das futuras telas.

**Motivo e trade-off:** reproduzir os dois painéis do modelo com dados reais,
sem outro backend. SSID/estado, RSSI da última busca e aberta/protegida usam o
snapshot existente. IP/gateway vêm do evento DHCP, passam pelo escritor único
`app_loop` e só aparecem para a rede atual. Não inferir modalidade WPA2/WPA3
nem validação de internet a partir do flag de IP. Switch Wi-Fi é omitido porque
não existe contrato liga/desliga. SSID manual usa o contrato de provisioning
existente, com escolha explícita aberta/WPA2 e diálogo de senha separado.

A textarea de senha é de uma linha, usa o estilo comum e password mode, mas
retém apenas a máscara por comprimento. O teclado global em modo privado
entrega teclas ao provisioning através de `product_ui`. Revelação temporária
continua usando o getter de um único glyph durante o desenho, conforme a
autorização anterior do produto; nenhum texto secreto é guardado em widget,
AppState, projection, eventos ou feedback. Ações de olho, Wi-Fi, sinal,
gateway e botões são glyphs das fontes existentes, ampliadas sem remover
ícones ou alterar altura/baseline.

Saída cancela aberturas adiadas, limpa a sessão, esconde teclado e apaga o
target antes de destruir objetos. Fechar o modal legado também cancela seu
diálogo de senha. A reconciliação de foco pendente do teclado é cancelada ao
limpar target. Esquecer mantém confirmação e a mailbox do backend; não é
declaração de remoção durável quando somente o enfileiramento foi aceito.

Nenhuma alteração de WDT, display, schemas offline, criptografia ou retenção
do vault. Os testes LVGL em host verificam lifecycle/pool/máscara; não fecham
os gates físicos de rede, reboot, latência, memória ou WDT.

## ADR-045 — Fuso virtualizado e teclado global com foco reconciliado

**Decisão:** reutilizar o módulo de Fuso existente numa cena dedicada lazy,
com seis posições visíveis e sete rows fixas. A cena permanece em cache após
a primeira entrada; não recriar árvore nem duplicar callbacks. Seleção é um
rascunho até Aplicar. `product_ui` continua a ponte para onboarding_service,
app_loop, time_service e FlashCoordinator; a UI não persiste nem muda TZ.
Modal legado continua compilando e usando o mesmo catálogo.

**Motivo e trade-off:** a busca anterior fazia consultas quadráticas ao CSV.
Um índice imutável em flash mantém a ordem dos 462 índices já gravados e
torna a busca linear. Filtro e textos das sete rows usam buffers limitados;
labels estáticas não realocam texto no scroll/busca. Metadados públicos de
país entram somente como dados estáticos de apresentação, sem novo backend.
Offset exibido é explicitamente padrão, sem inventar DST atual.

Teclado LVGL oficial permanece único em product_ui_state_t, 1024 px no
rodapé, com targets registrados num pool. DEFOCUSED agenda reconciliação
async: procurar outro campo visível com foco, retarget ou esconder. Interação
do teclado preserva a sessão. Clear/hide/destroy cancelam async antes do
teardown; DELETE libera bindings e destroy remove callbacks de campos vivos.

O cache limita custo de reentrada, mas mantém a árvore e buffers de Fuso em
memória junto do cache de Sistema. Não modifica WDT, pipeline de display,
persistência, schema offline ou contratos de credenciais. Build e testes
LVGL host não substituem aceite físico de foco, reboot, memória e WDT.

## ADR-046 — Identidade local do perfil principal

**Decisão:** Perfil passa a editar um nome local de até 48 bytes UTF-8 e uma
cor de avatar entre quatro tokens existentes. As iniciais são derivadas do
nome; não há foto, conta remota, localização inferida, troca de perfil ou
controle de saudação. O modal é criado somente ao primeiro uso, usa o teclado
global e é descartado ao sair da cena.

**Motivo e trade-off:** o AppState anterior não tinha fonte de identidade.
`product_ui` envia somente um pedido pelo EventBus; `app_loop` é o escritor da
identidade e publica nome, cor e estado de persistência na projeção. O
FlashCoordinator serializa um registro pequeno de NVS em dois slots com CRC,
sem alterar o schema offline nem iniciar escrita no callback LVGL. Uma falha
de gravação fica visível no Perfil; build e teste host não comprovam retenção
após reboot ou comportamento de WDT na placa, que exigem flash manual.

## ADR-047 — Piloto de navegação por passagens LVGL

**Decisão:** Home e Preferências usam um shell permanente com header, drawer e `content_host`. O Navigation Manager executa `LEAVE` no início de uma passagem do worker LVGL, `CLEAN` após `lv_timer_handler`, espera a passagem seguinte e então executa `BUILD` e `ENTER`. `handler_generation` conta passagens do worker; `page_generation` conta entradas concluídas em páginas. O adapter 0.6.4 recebe somente hooks genéricos `ui_cycle_begin`/`ui_cycle_end` por override local versionado. Somente a task LVGL chama o gerenciador e modifica objetos. O fluxo lazy de cache das outras telas continua disponível e não é removido neste piloto.

**Motivo e trade-off:** o timer de 32 ms não prova que a limpeza e a construção ocorreram em passagens distintas do handler, e o cache de todas as roots retém heap. O limite por geração elimina essa ambiguidade para as duas páginas piloto e permite medir heap interno, maior bloco, heap LVGL, objetos, timers e tempo de cada fase. O shell compartilhado evita recriar header e drawer entre Home e Preferências; páginas legadas ainda usam seus próprios headers até migração posterior. A instrumentação e a cópia local do adapter aumentam código versionado e exigem revisão ao atualizar a versão. Build não substitui navegação repetida, memória e WDT em placa; o responsável faz flash manual.
## ADR-048 — Espera mínima do worker LVGL em ticks reais

**Decisão:** configurar a espera mínima do worker LVGL como um tick FreeRTOS e
a máxima como dois ticks, mantendo o tick LVGL de 1 ms. Com
`CONFIG_FREERTOS_HZ=100`, o timeout fica entre 10 e 20 ms; notificações podem
acordar a task antes desse prazo.

**Motivo e trade-off:** o valor anterior de 1 ms era convertido por
`pdMS_TO_TICKS(1)` em zero ticks no adapter. Isso permitia ciclos de handler
sem bloqueio quando o LVGL retornava prazo curto; as medições do piloto
mostraram centenas de milhares de passagens em poucos segundos. O novo limite
cede CPU ao IDLE0 e reduz a taxa máxima de polling. Um WDT observado após
cerca de 19 minutos ainda exige repetição em placa; o PC isolado do watchdog
não prova a causa, especialmente quando o monitor usa um ELF diferente do
binário gravado.

## ADR-049 — Quadro preservado durante transições de navegação

**Decisão:** o Navigation Manager suspende a invalidação e o timer de refresh
do display antes de `LEAVE`. O último quadro permanece no painel durante
`CLEAN` e a espera pela passagem seguinte. Depois de `ENTER`, o gerenciador
reativa a invalidação, invalida a tela inteira e agenda um refresh imediato.
Em falha de `BUILD`, também reativa o display e sinaliza o erro. A proteção
vale para todas as rotas que passam pelo gerenciador; foi removida a exceção
que mantinha Perfil visível apenas para Home e Preferências.

**Motivo e trade-off:** ocultar a árvore antiga antes de a nova página estar
pronta permite que o LVGL desenhe um quadro branco intermediário. Congelar
somente o timer não basta, pois uma requisição de refresh pode reativá-lo;
por isso a invalidação também fica suspensa, de forma balanceada. A pausa
dura apenas a transição entre passagens do worker, sem bloquear a task. Esta
decisão exige verificação visual em placa de todas as rotas e não resolve por
si só o WDT observado após aproximadamente 19 minutos de navegação.

## ADR-050 — Navigation Manager para todas as telas de produto

**Decisão:** Perfil, Tela e som, Wi-Fi, Fuso horário, Notificações e Sistema
passam a construir suas raízes lazy diretamente na fase `BUILD` do Navigation
Manager. A sequência `LEAVE` → `CLEAN` → `WAIT_NEXT_PASS` → `BUILD` → `ENTER`
é agora a única rota compilada para qualquer transição iniciada por toque. Os
layouts, callbacks, serviços e caches pequenos existentes são preservados;
somente a orquestração de ciclo de vida foi centralizada.

**Motivo e trade-off:** o caminho anterior chamava um construtor assíncrono
legado de dentro de `BUILD`, deixando duas políticas de navegação no mesmo
produto. Centralizar elimina timers auxiliares e impede que uma árvore seja
criada fora da passagem medida pelo gerenciador. O `content_host` compartilhado
é limpo em toda transição para não reter Home ou Preferências ocultas depois
de uma tela de configurações. As raízes das demais telas permanecem lazy e
podem ser evacuadas pela política de heap; todas as rotas exigem novo ensaio
em placa, inclusive a investigação do WDT prolongado.

## ADR-051 — Cadência original dos ícones animados de clima

**Decisão:** Home e Clima reproduzem os quadros no intervalo `frame_ms` informado
pelo pacote NPWI. Os assets atuais são amostrados a 8 quadros/s (125 ms por
quadro), e a duração total de cada ciclo é calculada a partir desse intervalo.

**Motivo e trade-off:** a Home havia duplicado o intervalo para 250 ms por quadro
para reduzir carga do renderer por software e risco de starvation do IDLE0.
Isso reduzia a animação a 4 quadros/s e dobrava a duração visual dos ciclos;
Clima repetia o mesmo limite sem justificativa própria. Restaurar 125 ms não
altera o tamanho dos pacotes nem a memória reservada, mas dobra a frequência de
redesenho do ícone. O boot, a responsividade, o watchdog e a apresentação nas
duas telas devem ser verificados fisicamente após a mudança.

## ADR-052 — Dados da tela Mercado

**Decisão:** a tela Mercado reutiliza a cotação de Bitcoin do executor HTTPS e
expande uma única resposta autenticada da CoinGecko para Ethereum, Solana, BNB
e XRP. Fear & Greed vem da API pública da Alternative.me, atualizada a cada 24
horas, com retry após uma hora em caso de falha, e identificada junto ao dado.
O snapshot offline v6 guarda esses cinco
valores e continua lendo payloads v1 a v5. A navegação Mercado compartilha o
shell e o drawer existentes; a UI só projeta o snapshot.

**Limites da fonte:** S&P 500, Nasdaq e Ibovespa continuam visíveis sem cotação
até haver uma fonte autorizada configurada. FRED exige chave por aplicação e
alerta que séries podem ter copyright de terceiros; B3 distribui dados e
índices por seus serviços licenciados. Não raspar páginas nem embutir chave ou
cotação de exemplo. A tela preserva os três espaços até haver credencial e
direito de exibição compatíveis.

**Motivo e trade-off:** reunir os ativos digitais numa resposta reduz handshakes
e mantém a regra de conexão única. O campo de preço em micros de dólar conserva
precisão de XRP. O payload cresce de 275 para 341 bytes, ainda abaixo do limite
versionado do cache. A resposta de sentimento é uma chamada adicional
serializada e mostra a atribuição exigida pela Alternative.me.

## ADR-053 — Cotações de índices via brapi

**Decisão:** consultar `^BVSP`, `^GSPC` e `^IXIC` em uma chamada serializada
`GET /api/quote/%5EBVSP,%5EGSPC,%5EIXIC` à brapi. O adaptador só publica cotações
que retornem com o símbolo esperado, preço e horário local confiável; símbolo
ausente permanece indisponível e não recebe valor de exemplo. A credencial fica
na configuração local ignorada pelo Git e é enviada no cabeçalho
`Authorization: Bearer`; não deve ser colocada na URL ou em logs. O snapshot v7
acrescenta S&P 500 e Nasdaq e continua migrando cache v1–v6.

**Limites:** a documentação pública consultada confirma a rota para `^BVSP`,
mas não confirma cobertura de `^GSPC` e `^IXIC`. Esses símbolos só passam a ser
exibidos se a resposta autenticada os trouxer com campos válidos. A brapi informa
que não é distribuidora licenciada da B3. Esta decisão cobre a exibição no
produto conforme os termos da brapi, não cria licença de distribuição B3 nem
autoriza redistribuir dados brutos. Validar contrato, atraso e direitos de uso
antes de distribuição comercial.

**Motivo e trade-off:** um lote reduz conexões e mantém o executor HTTPS único.
O payload offline passa de 341 para 363 bytes. A credencial local é embutida na
imagem de desenvolvimento e deve migrar para o cofre de segredos de produção.
Referências: [rota Ibovespa](https://brapi.dev/faq/buscar-ibov),
[exemplos de autenticação](https://brapi.dev/docs/examples/typescript),
[índices disponíveis](https://brapi.dev/faq/outros-indices) e
[informações sobre licença B3](https://brapi.dev/faq/a-brapi-e-distribuidora-licenciada-da-b3).

## ADR-054 — Consultas individuais de índices na brapi

**Decisão:** a API aceita um ativo por chamada. Esta decisão substitui apenas a
estratégia de lote do ADR-053: o executor consulta `^BVSP`, `^GSPC` e `^IXIC`
em três requisições HTTPS sequenciais, mantendo no máximo uma conexão em voo.
Cada resultado atualiza somente seu próprio campo; falha parcial preserva as
demais cotações e marca como desatualizado apenas o ativo que falhou.

**Trade-off:** passam a ocorrer até três handshakes por ciclo de índices. A
cadência permanece em cinco minutos e as chamadas compartilham o limite de
tempo do executor; se o orçamento acabar, os valores ainda não consultados
permanecem indisponíveis ou desatualizados até o próximo ciclo.

## ADR-055 — Atualização horária dos índices no plano gratuito

**Decisão:** o ciclo normal de `^BVSP`, `^GSPC` e `^IXIC` passa a ocorrer a cada
60 minutos. A retentativa depois de falha permanece em dois minutos; as três
requisições continuam sequenciais e passam pelo executor único.

**Motivo e trade-off:** a documentação atual da brapi informa atualização de
cerca de 30 minutos no plano Gratuito, uma cotação por chamada e limite de
15.000 chamadas mensais. Um ciclo horário consome até 2.160 chamadas mensais
em operação contínua, contra 25.920 na cadência anterior de cinco minutos.
Assim, o polling normal fica menos frequente que a recência de origem, mas os
dados podem ficar mais antigos entre ciclos. A retentativa curta só é usada
após falha e permite recuperação mais rápida; se falhas persistentes ocorrerem,
elas ainda podem consumir chamadas da cota. Referências: [limites da brapi](https://brapi.dev/faq/quais-as-limitacoes)
e [delay por plano](https://brapi.dev/faq/qual-a-diferenca-real-no-atraso-delay-dos-dados-entre-os-planos).

## ADR-056 — Primeira tela IoT e caminho de integração SONOFF

**Decisão:** a primeira tela IoT mostra somente Dispositivos e Sensores; Cenas
ficam fora deste incremento. O usuário confirmou que tem SONOFF TX1C, TX2C e
TX3C e usa somente o app eWeLink. A tela não cria dispositivos ou leituras de
demonstração e mantém os estados vazios até uma fonte real estar conectada. O
primeiro caminho técnico é o protocolo LAN Zeroconf implementado pela
comunidade SonoffLAN (`_ewelink._tcp`, porta 8081 e endpoints `/zeroconf/*`).
TX1C, TX2C e TX3C constam na matriz SonoffLAN como modelos locais, com 1, 2 e
3 canais (UIID 6, 7 e 8, firmware 3.8.0 na matriz consultada). Isso sustenta
um candidato de compatibilidade, não valida os aparelhos/firmwares físicos do
usuário; descoberta, estado e acionamento de todos os canais ainda exigem
teste na bancada. Não é necessário escolher Matter para estes modelos.

**Motivo e trade-off:** a documentação SonoffLAN descreve suporte a firmware
original via rede local e informa que a lista de dispositivos e chaves de
cifragem vem dos servidores eWeLink, ficando depois armazenada localmente.
Com o firmware original, `devicekey` é necessária para decifrar atualizações e
cifrar comandos; mDNS por si só só descobre metadados de LAN e não substitui
essa credencial. A obtenção da chave precisa ser desenhada sem expor senha ou
chave em UI, eventos, logs ou URLs. Qualquer persistência passa pelo
FlashCoordinator e exige decidir a forma de provisionamento e proteção em
repouso antes de integrar conta eWeLink. A recomendação do mantenedor é tratar
LAN como caminho sujeito a indisponibilidade e validar reconexão/timeout. A API
DIY oficial é outro protocolo e não é requisito para esses três modelos.
Referências: [implementação SonoffLAN](https://github.com/AlexxIT/SonoffLAN),
[matriz de modelos SonoffLAN](https://github.com/AlexxIT/SonoffLAN/blob/master/DEVICES.md)
e [ferramentas do modo DIY oficial](https://github.com/itead/Sonoff_Devices_DIY_Tools).

## ADR-057 — Autorização eWeLink por OAuth 2.0

**Decisão:** seguir o fluxo OAuth 2.0 oficial do eWeLink para vincular a conta e
buscar a lista de dispositivos e suas `devicekey`. O NovaPanel não coleta nem
envia a senha da conta eWeLink. A autorização ocorre na página do eWeLink e
retorna ao fluxo local do painel; depois, o controle dos TX1C/TX2C/TX3C segue
por Zeroconf na LAN. Credenciais de aplicação e tokens ficam fora do código,
logs, AppState e view-models; persistência passa pelo FlashCoordinator.

**Motivo e trade-off:** o portal oficial exige cadastro e aprovação de uma
aplicação de desenvolvedor, com URL de retorno registrada. Para desenvolvedor
pessoal, a documentação atualmente indica autorização OAuth 2.0, acesso
gratuito limitado e disponibilidade restrita de interfaces/dispositivos. A
documentação OAuth lista `devicekey` no registro de dispositivo e define o
intercâmbio do código de autorização por tokens. Isso evita colocar a senha da
conta na aplicação embarcada, mas depende da aprovação da aplicação e de
confirmar a URL de retorno aceita pelo portal antes de implementar o callback.
Tokens e chaves locais continuam sensíveis: o protótipo ainda não tem NVS
Encryption/Flash Encryption ativados; teste de bancada não equivale a proteção
contra extração física. Referências: [OAuth 2.0 oficial](https://github.com/CoolKit-Technologies/eWeLink-API/blob/main/en/OAuth2.0.md),
[limites de desenvolvedor pessoal](https://github.com/CoolKit-Technologies/eWeLink-API/blob/main/en/Pricing.md)
e [API oficial de dispositivos](https://github.com/CoolKit-Technologies/eWeLink-API/blob/main/en/APICenterV2.md).

## ADR-058 — Descoberta local assíncrona dos SONOFF

**Decisão:** usar o componente `espressif/mdns` em versão fixada para pesquisar
`_ewelink._tcp` na rede local. A pesquisa e resolução de serviços rodam fora da
task LVGL; a interface recebe apenas um snapshot limitado de metadados públicos
(tipo anunciado, ID e endereço local), sem chaves ou tokens. A descoberta não
aciona interruptores nem confere compatibilidade além do modelo anunciado.

**Motivo e trade-off:** TX1C/TX2C/TX3C anunciam serviço local conforme a
implementação SonoffLAN. mDNS é o mecanismo apropriado para descobrir endereço
e porta sem pedir IP manual. A consulta tem duração e quantidade máximas para
não bloquear a UI nem permitir crescimento sem limite. O canal local só será
habilitado após a autorização OAuth obter a chave do dispositivo e a validação
do payload por modelo. Referências: [mDNS ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/protocols/mdns.html)
e [protocolo LAN SonoffLAN](https://github.com/AlexxIT/SonoffLAN/blob/master/custom_components/sonoff/core/ewelink/local.py).

## ADR-059 — Pomodoro local integrado ao AppState

**Decisão:** a tela Pomodoro usa o serviço de domínio chamado pelo `app_loop`;
comandos de iniciar/pausar, zerar e escolher duração passam pelo EventBus e o
AppState projeta o tempo restante e os totais para a task LVGL. A contagem usa
relógio monotônico; os totais diários reiniciam à meia-noite local quando a hora
é confiável. A duração personalizada fica entre 1 e 99 minutos. Contadores são
voláteis e reiniciam com o painel; persistência e pausas de descanso ficam fora
deste incremento.

**Motivo e trade-off:** mantém a UI sem temporizador de domínio e respeita o
escritor único de estado. O timer continua atualizando sem rede. A contagem não
sobrevive ao reboot e o fluxo implementado fecha ciclos de foco sem alternar
automaticamente para descanso; isso reduz o escopo da primeira tela e deixa a
política de histórico/descanso para uma decisão posterior.

## ADR-060 — aviso sonoro ao concluir Pomodoro

**Decisão:** quando o serviço transita para concluído, `app_loop` solicita uma
sequência de três notas em `device_control_service`. Um latch impede repetir o
pedido enquanto o estado concluído continuar publicado. O serviço de controles
reproduz o áudio em sua própria task e respeita o volume atual.

**Motivo e trade-off:** reaproveita o codec e a task de áudio existentes, sem
bloquear `app_loop` nem a task LVGL. Se o áudio não estiver pronto ou o volume
estiver em zero, a solicitação não toca som; o encerramento visual do ciclo
continua normal.

## ADR-061 — Primeiro dispositivo IoT: descoberta da câmera Tapo C200

**Decisão:** iniciar a integração da Tapo C200 pela descoberta ONVIF local e
pela apresentação da câmera e do estado online/offline na lista de
Dispositivos. A busca WS-Discovery é assíncrona, limitada a quatro câmeras e
publica somente modelo anunciado e endereço IPv4 no `AppState`; a tela não faz
I/O de rede. Após a primeira busca manual, a presença é atualizada a cada
60 segundos. Os endereços descobertos e o estado são voláteis nesta etapa. O
vídeo RTSP e a persistência de cadastro ficam para incrementos posteriores.
Nenhum usuário ou senha da câmera é coletado nesta etapa.

**Motivo e trade-off:** a TP-Link documenta a C200 com ONVIF Profile S,
serviço ONVIF na porta 2020 e RTSP na porta 554. A transmissão oferece
`/stream1` em alta qualidade e `/stream2` em qualidade padrão. O app Tapo exige
uma conta local da câmera separada da conta Tapo para integrar terceiros; ela
não é necessária para descobrir um serviço ONVIF anunciado. Adiar a captura da
credencial mantém segredo fora do `AppState`, eventos, logs e armazenamento até
definir o caminho protegido. O painel só anuncia online após receber uma
resposta WS-Discovery; uma câmera vista anteriormente permanece na lista como
offline quando deixa de responder. Essa primeira etapa não valida autenticação,
RTSP, PTZ ou exibição de vídeo. RTSP/ONVIF devem ficar na LAN confiável; não
expor as portas da câmera à Internet.

O escopo de vídeo foi atualizado pelo ADR-062; a descoberta e a lista continuam
sendo a primeira etapa visível da integração.

Referências: [guia oficial TP-Link RTSP/ONVIF](https://www.tp-link.com/br/support/faq/2680/),
[FAQ oficial Tapo ONVIF/RTSP](https://www.tp-link.com/br/support/faq/4465/),
[especificação de discovery ONVIF](https://www.onvif.org/wp-content/uploads/2022/07/ONVIF_Profile_A_Client_Test_Specification_22.06.pdf),
[ESP-IDF 5.5 sockets BSD](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-guides/lwip.html).

## ADR-062 — RTSP ao vivo da Tapo C200 no painel

**Decisão:** iniciar a visualização ao vivo usando a URL de baixa resolução
`rtsp://<ip>:554/stream2` (640×360), com cliente RTSP/RTP interno e decoder
H.264 Espressif fixado no manifesto e no lock. O cliente suporta autenticação
por cabeçalho, mantém o endpoint sem segredos e limita o tamanho de pacotes e
quadros. A recepção/decodificação fica em worker próprio;
somente a task LVGL apresenta os quadros por um buffer limitado. Usuário e senha
da Conta da Câmera são recebidos pela UI local e enviados por mailbox privada,
sem passar por AppState, EventBus, logs, URL registrada ou dump. Nesta primeira
etapa permanecem somente em RAM e precisam ser digitados novamente após reboot;
retenção depende de um cofre apropriado para credenciais da câmera.

**Motivo e trade-off:** a TP-Link documenta stream2 como fluxo RTSP padrão e
exige a Conta da Câmera para clientes de terceiros. A documentação atual da
Espressif lista H.264 acelerado no P4 como codificação; o decoder H.264 do
componente `esp_h264` é por software e entrega I420, que precisa ser convertido
para RGB565 antes de ser exibido. A resolução menor limita memória, CPU e
tráfego em relação ao stream1 1080p, mas a taxa real de quadros precisa ser
medida na placa junto do LVGL e da conectividade C6. A tela publica quadros
através de um objeto de imagem LVGL e dos buffers de renderização existentes;
não escreve diretamente no DSI nem substitui os framebuffers do display. Vídeo
usa PSRAM em buffers previamente dimensionados. A latência depende da câmera,
da rede, da decodificação e do ciclo de renderização; não é garantida em 50 ms.
RTSP/ONVIF permanecem restritos à LAN confiável; não abrir portas da câmera à
Internet. O componente `espp/rtsp` foi rejeitado na implementação porque não
oferece autenticação RTSP e registra as requisições completas, incompatível
com a política de segredos. Referências: [guia oficial TP-Link](https://www.tp-link.com/br/support/faq/2680/),
[decoder `esp_h264`](https://components.espressif.com/components/espressif/esp_h264/versions/1.4.1/readme).

## ADR-063 — Autenticação das consultas SOAP ONVIF da Tapo

**Decisão:** autenticar a leitura `GetVideoEncoderConfigurationOptions` com
WS-Security `UsernameToken` usando `PasswordDigest`, nonce aleatório e
timestamp UTC; conservar HTTP Digest para RTSP. O usuário e senha vêm somente
da mailbox volátil da sessão e não são registrados nem persistidos. A consulta
permanece estritamente somente de leitura.

**Motivo e trade-off:** a C200 respondeu `GetCapabilities` sem autenticação e
devolveu HTTP 400 / fault de autorização para a consulta de opções quando o
cliente não enviou autenticação SOAP. A especificação ONVIF prevê o perfil
UsernameToken para serviços que não usam HTTP Digest; o `PasswordDigest`
evita transmitir a senha em texto claro pela LAN. O timestamp exige relógio UTC
confiável, e o suporte específico da versão de firmware da câmera ainda precisa
ser validado em bancada. Nenhum método `Set*` é usado. Referências:
[ONVIF Core Security](https://www.onvif.org/specs/core/ONVIF-Core-Specification-v241.pdf)
e [ONVIF Application Programmer's Guide](https://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf).

**Correção de leitura (2026-10-03):** a resposta HTTP 200 da consulta de opções
era classificada como falha porque o parser procurava o nome do tipo XML
`H264Options`. O elemento transmitido é `H264` e suas opções incluem
`H264ProfilesSupported`; o parser agora verifica o elemento de perfis e registra
somente o tamanho do corpo em caso de falha. A consulta continua somente de
leitura. A [especificação Media ONVIF](https://www.onvif.org/specs/srv/media/ONVIF-Media-Service-Spec-v260.pdf)
define esses elementos. O [suporte TP-Link](https://community.tp-link.com/en/smart-home/forum/topic/742598)
informa que a C200 não permite modificar suas configurações de vídeo pela API
ONVIF; nenhuma chamada `Set*` foi adicionada.

**Resultado de bancada (2026-10-03):** com o parser corrigido, a C200 anunciou
somente `Main` em `H264ProfilesSupported`, e o `/stream2` entregou SPS H.264
`profile_idc=77` (Main). A autenticação e o RTSP chegaram a `PLAY 200`; o
decodificador de software integrado aceita somente Constrained Baseline.
Assim, o vídeo desta câmera exige um decodificador Main viável no P4 ou
transcodificação local. Não assumir que alterar a qualidade do stream ou a
consulta ONVIF converta o codec. A escolha e a validação de desempenho da
solução permanecem abertas.

## ADR-064 — Varredura Wi-Fi assíncrona via ESP-Hosted

**Decisão:** iniciar scans com `esp_wifi_scan_start(..., false)` e aguardar o
evento encaminhado `WIFI_EVENT_SCAN_DONE` no worker de conectividade, com
timeout local de 15 s. Recusar o scan antes de chamar o driver quando o
transporte Hosted ou Wi-Fi não estiver marcado como pronto. O scan não dispara
recovery por sua duração normal; o fallback para RPCs sem resposta está
definido no ADR-065.

**Motivo e trade-off:** ESP-Hosted usa RPC síncrona com timeout padrão de 5 s,
enquanto um scan bloqueante mantém a chamada aberta até o rádio concluir todos
os canais. Desacoplar o início da conclusão evita que a duração normal do scan
seja confundida com ausência de resposta RPC. A espera local limita o worker e
preserva a serialização dos pedidos; o usuário ainda vê uma falha se o C6 ou o
transporte não encaminhar `SCAN_DONE`. Um erro RPC real segue a política
limitada de recuperação do ADR-065 e o gate de Hosted/C6 do ADR-013.

## ADR-065 — Recovery limitado após ausência de resposta RPC do Wi-Fi

**Decisão:** a primeira chamada à API `esp_wifi` que retorne `ESP_FAIL`,
enquanto Hosted e Wi-Fi constam como prontos, agenda recovery no
worker de conectividade. O evento Hosted `TRANSPORT_FAILURE/DOWN` também
agenda o mesmo caminho. Nenhuma chamada de teardown/reinit ocorre dentro do
callback de evento, da UI ou da task LVGL. O worker usa `recover_hosted_link()`
e respeita o limite já definido no ADR-013: no máximo três ciclos em dez
minutos, seguidos de cooldown de cinco minutos. Recovery não reinicia o P4 e
usa exclusivamente o ciclo de reset do C6 pertencente ao ESP-Hosted.

**Motivo e trade-off:** após a perda de rede, chamadas de scan, disconnect e
connect registraram `eh_host_feat_rpc: request: no response` por 5 s e
retornaram `ESP_FAIL`, embora o estado local ainda marcasse o transporte como
ativo. Sem um evento Hosted de queda, o worker repetia associações sem tentar
restabelecer o plano de controle. A primeira falha RPC já consumiu o timeout de
5 s; iniciar recovery nesse ponto evita desperdiçar outro timeout repetindo
uma chamada em um canal de controle sem resposta. Desconexões reportadas pelo
evento normal `WIFI_EVENT_STA_DISCONNECTED` continuam usando somente retry de
associação; só `ESP_FAIL` em RPC ou evento de falha Hosted inicia recovery. O
limite e cooldown existentes contêm ciclos repetidos; o resultado ainda
precisa ser observado em bancada após flash, e não prova estabilidade da rede
ou do SDIO.

## ADR-066 — Cadência diária para Fear & Greed

**Decisão:** consultar a API Fear & Greed da Alternative.me a cada 24 horas
após sucesso e tentar novamente após uma hora em caso de falha. A tela Mercado
continua exibindo o valor mais recente disponível sem solicitar uma atualização
ao abrir a tela.

**Motivo e trade-off:** a própria fonte informa que calcula e publica esse
índice diariamente e fornece `time_until_update` para a última leitura. Uma
consulta a cada 15 minutos repetia até 96 pedidos por dia para um valor que não
muda com essa frequência. A cadência diária reduz tráfego HTTPS e mantém o
dado compatível com a frequência da fonte; uma falha pode deixar o valor
desatualizado por mais tempo, mitigada pelo retry horário. Referência: [API e
frequência oficial da Alternative.me](https://alternative.me/crypto/fear-and-greed-index/).

## ADR-067 — Cadência diária para índices de mercado

**Decisão:** consultar `^BVSP`, `^GSPC` e `^IXIC` a cada 24 horas após
sucesso; em falha, tentar novamente após uma hora. As três consultas brapi
continuam sequenciais no executor HTTPS único, com atualização parcial por
ativo.

**Motivo e trade-off:** a cadência horária anterior gerava até 72 requisições
brapi por dia para três índices cuja origem tem atraso de cerca de 30 minutos.
O ciclo diário reduz o consumo de dados e chamadas; os valores podem ficar
menos recentes e uma falha mantém os dados antigos por mais tempo, mitigada
pelo retry horário. Essa frequência é uma política de consumo do painel, não
uma alegação de que as fontes publiquem valores somente uma vez ao dia.
Complementa a redução de Fear & Greed registrada no ADR-066.

## ADR-068 — Buffers DMA alinhados do ESP-Hosted em PSRAM

**Decisão:** habilitar `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y` no P4. O
alocador alinhado do ESP-Hosted deve preferir PSRAM DMA-capable para buffers de
transporte SDIO e manter fallback para SRAM DMA interna. TLS, pilhas e outros
buffers com requisito interno continuam usando suas capacidades atuais; C6,
perfil SW_AGGR e tamanho dos buffers do protocolo não mudam.

**Motivo e trade-off:** em bancada, o Hosted perdeu uma leitura SDIO de 4.776 B
(`dma_alloc(5120) failed`) enquanto o executor HTTPS estava ativo; após isso,
as associações/IP permaneceram aparentes, mas conexões HTTPS e DNS de produto
falharam. O limite de 32 KiB em `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` não reserva
uma região contígua DMA interna para essas leituras. O ESP32-P4 oferece EDMA
para buffers DMA em PSRAM quando alocados com `MALLOC_CAP_SPIRAM |
MALLOC_CAP_DMA`, conforme a documentação do ESP-IDF 5.5. O ESP-Hosted 3.0.6 já
fornece a opção e tenta a PSRAM antes de recuar para a SRAM interna. O trade-off
é tráfego adicional/banda de cache da PSRAM e latência de DMA; o buffer máximo
de SDIO segue limitado pelo transporte e a carga combinada com display/TLS
continua gate físico obrigatório. Não altera fontes em `managed_components/`.

**Validação exigida:** build P4 limpo com configuração efetiva confirmada,
flash apenas do app P4 e bancada com HTTPS serializado sob tráfego recebido.
Confirmar ausência de `dma_alloc`/`rx_get_buffer` failure, sucesso dos domínios
de dados, associação/IP e render sem regressão. A build isolada não fecha esse
gate.

## ADR-069 — Tela principal configurável no perfil local

**Decisão:** permitir que o usuário escolha em Perfil > Preferências pessoais >
Tela inicial entre Home, Clima, Mercado, Dispositivos e Pomodoro. A escolha é
salva junto ao perfil no armazenamento local e aplicada no próximo boot. Um
perfil antigo ou inexistente continua iniciando em Home; registros antigos são
lidos com esse valor padrão.

**Motivo e trade-off:** o painel tem várias telas principais e não deve impor a
Home a quem usa outra delas como ponto de partida. Reutilizar o registro de
perfil e o FlashCoordinator evita criar um segundo caminho de persistência. A
preferência pode existir antes de o usuário preencher o nome; nesse caso o
registro local guarda a seleção sem inventar uma identidade.

## ADR-070 — Navegação por gesto entre telas principais

**Decisão:** permitir deslizar horizontalmente na área de conteúdo para navegar
na ordem Home, Clima, Mercado, Dispositivos e Pomodoro, com indicador de páginas
no padrão expanding-dot no rodapé: o ponto ativo se alonga e muda para a cor de
destaque. A navegação para nos limites, ignora movimentos iniciados no header e
usa o Navigation Manager existente para preservar o ciclo de vida das telas.

**Motivo e trade-off:** o painel é touch-first e essas telas formam o conjunto
principal de navegação. Ler os pontos de toque do indev evita habilitar o
reconhecedor genérico de gestos do LVGL, que depende de suporte a float. A
classificação simples exige deslocamento horizontal mínimo e predominância
horizontal para reduzir conflitos com toques e movimentos verticais. Objetos
genéricos do LVGL são clicáveis por padrão; por isso a flag `CLICKABLE` não é
usada para distinguir controles. Quando reconhecido, o swipe consome os eventos
de clique da mesma interação antes de eles alcançarem o widget. Em callbacks
registrados diretamente no indev, o próprio dispositivo vem de
`lv_event_get_target()`; o parâmetro do evento é o objeto tocado.

## ADR-071 — Ações do header e histórico local de notificações

**Decisão:** o ícone Wi-Fi abre a configuração de rede pela navegação existente.
O sino abre um modal sobre a tela atual com notificações de rede, mercado e
sistema. O `app_loop` produz eventos somente para transições observadas:
desconexão/recuperação com credenciais ativas, nova cotação de mercado,
falha/recuperação de armazenamento e falha de reinício. O histórico mantém os
oito eventos mais recentes em RAM; abrir um item ou usar “Marcar todas como
lidas” remove o evento da contagem de não lidas sem removê-lo do histórico.

**Motivo e trade-off:** esses sinais tornam o header acionável e mostram
mudanças que já são observadas pelo estado central, sem chamadas de rede ou
LVGL fora dos seus proprietários. Um limite fixo evita crescimento de heap; o
histórico é volátil e o evento mais antigo é descartado quando a capacidade é
atingida. Persistência do histórico após reboot fica para uma decisão posterior.
As preferências de entrega já persistidas continuam controlando eventos gerais
e de sistema.

## ADR-072 — Sincronização eWeLink sob demanda com inventário local

**Decisão:** manter autenticação direta eWeLink como funcionalidade permanente
do serviço, acionada somente sob demanda. A senha e os tokens existem apenas
durante a sessão de sincronização em RAM e são zerados/descartados ao concluir,
inclusive em falha. O fluxo usa o APP_ID e a derivação de assinatura validados
no POC/SonoffLAN, faz login e consulta de devices sequencialmente no executor
HTTPS único, e reconcilia por `deviceid`. A `devicekey` é distinta de `apikey`
e integra o inventário persistido. Devices novos são adicionados; existentes
recebem metadados atualizados preservando configurações exclusivamente locais;
devices ausentes são marcados como ausentes na última sincronização, sem
remoção automática. Após sincronizar, controle dos dispositivos segue pela LAN.

O inventário é persistido através do `FlashCoordinator` e carregado no boot.
Enquanto NVS Encryption/Flash Encryption não estiverem habilitadas, as chaves
ficam expostas a extração física; esse risco é uma pendência de segurança do
protótipo, não uma exigência para autenticar novamente após reboot. Nenhuma
senha, token ou `devicekey` pode aparecer em logs, AppState, eventos ou
view-models. Esta decisão supersede o ADR-057; o ADR antigo permanece como
histórico da direção anterior baseada em OAuth.

**Motivo e trade-off:** a sincronização direta reproduz o fluxo já validado
para importar o inventário e as chaves sem exigir conta de desenvolvedor. A
sessão temporária evita retenção de credenciais cloud e mantém a operação
normal independente da nuvem. Persistir as `devicekey` permite continuidade
LAN após reboot e sincronizações futuras corrigem chaves e metadados. A
reconciliação não exclui devices ausentes para evitar perda acidental de
configurações locais. A NVS atual não oferece proteção contra extração física;
NVS Encryption/Flash Encryption e gates de segurança continuam necessários
antes de produção. A derivação HMAC depende da ordem e serialização exatas da
tabela de regiões do mecanismo de referência, que deve ser preservada e
validada por vetor conhecido do POC. Workspaces de inventário e derivação
temporários ficam em PSRAM; o corpo HTTP limitado é alocado em SRAM interna
somente durante a sincronização e liberado ao terminar. Isso mantém livre a
RAM interna de DMA usada pelo áudio e pelo transporte de rede; o inventário
persistente continua disponível sem alocação no caminho de boot.

## ADR-073 — Buffer temporário eWeLink em PSRAM

**Decisão:** manter o corpo HTTP eWeLink limitado a 48 KiB, mas alocar seu
buffer temporário em PSRAM. Buffers TLS e a política de uma conexão HTTPS em voo
permanecem inalterados. O buffer é zerado e liberado ao fim da sincronização;
respostas acima do limite falham sem truncamento.

**Motivo e trade-off:** depois de abrir a tela Dispositivos, havia cerca de
27 KiB no maior bloco contíguo de SRAM interna, enquanto o cliente reservava
49.153 bytes antes de iniciar o login. A sincronização terminava em
`ESP_ERR_NO_MEM` antes de qualquer requisição de autenticação. O corpo real do
POC tem 9.401 bytes; mantemos o limite de 48 KiB para comportar respostas maiores
sem competir com DMA/TLS pela SRAM interna. O custo transitório é uma cópia
limitada em PSRAM, cuja banda já é compartilhada com display; medir duração e
renderização no teste de sincronização. JSON/cJSON e TLS conservam seus
alocadores atuais. Esta decisão ajusta para eWeLink a preferência geral por
SRAM interna dos corpos HTTP e supersede somente essa parte do ADR-072.

## ADR-074 — Params transitórios e registro eWeLink compacto

**Decisão:** manter `params` integralmente no inventário de sessão durante o
fetch/reconcile, com buffer de 1 KiB por device e falha explícita caso uma
resposta futura exceda esse limite. O registro duplicado A/B da NVS persiste
somente os campos necessários para o inventário LAN e as configurações locais;
`params` não é necessário para o controle LAN após reboot e não é gravado. A UI
aloca seu snapshot maior em PSRAM, sem aumentar o uso de stack LVGL.

**Motivo e trade-off:** o POC retornou `params` entre 462 e 835 bytes, excedendo
o antigo limite de 256 bytes que era ignorado silenciosamente. Aumentar o
registro completo duplicado na partição NVS de 32 KiB consumiria espaço
desnecessário. O novo limite comporta os dados observados; acima dele a
sincronização falha visivelmente em vez de salvar um inventário parcial. A
persistência mantém `deviceid`, `devicekey`, `apikey`, nomes, modelos, canais,
estado de presença e settings locais. A versão de registro compacta rejeita
slots legados por tamanho/CRC e grava a geração válida seguinte sem formatar a
NVS.

## ADR-075 — Limites dos buffers HTTP na sincronização eWeLink

**Decisão:** para as requisições eWeLink, usar buffers HTTP de 2 KiB RX/1 KiB
TX. O corpo JSON segue sendo copiado em PSRAM por eventos HTTP. Não forçar
`HTTP_TLS_DYN_BUF_RX_STATIC`: a primeira tentativa não evitou a falha de AES e
essa opção mantém um buffer de recepção TLS maior durante a conexão.

**Evidência/correção:** o reteste confirmou a mesma falha AES com essa estratégia
e buffers reduzidos, logo esses limites não corrigem a alocação que falha. A
origem foi localizada no alocador AES-DMA do IDF; ver ADR-076. Os limites menores
permanecem para o cliente HTTP, mas não são uma mitigação de capacidade do
AES-DMA. A configuração efetiva deve seguir o buffer TLS dinâmico padrão.

## ADR-076 — AES por software no mbedTLS do P4 até validar o caminho DMA

**Decisão:** desabilitar `CONFIG_MBEDTLS_HARDWARE_AES` no perfil P4 e usar AES
por software no mbedTLS, mantendo seus buffers em SRAM interna. Não alterar
drivers ou fontes do ESP-IDF. A mudança abrange todas as operações AES/GCM via
mbedTLS neste firmware, inclusive TLS de providers e qualquer cliente LAN que
use essas APIs; não é uma opção por requisição.

**Motivo e trade-off:** login eWeLink completa, mas a leitura HTTPS falha com
`esp-aes: Failed to allocate memory`. O código do ESP-IDF 5.5.4 mostra que o
caminho AES acelerado do P4 usa alocações com `MALLOC_CAP_DMA` para buffers de
entrada/saída temporários e descritores. Reduzir os buffers de `esp_http_client`
não muda essas alocações, e `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`
não informa o maior bloco que atende exatamente às capabilities DMA usadas por
AES. O caminho de software elimina a dependência do alocador AES-DMA sem mover
TLS ou dados criptográficos para PSRAM. O trade-off é maior uso de CPU/latência
de criptografia; medir sincronização, HTTPS dos providers, heap e temperatura.
Reavaliar aceleração quando houver métricas do heap DMA no ponto da falha e
ensaio de regressão da carga combinada.

**Validação:** build limpo P4; conferir no `sdkconfig` efetivo que
`CONFIG_MBEDTLS_HARDWARE_AES` está desabilitado; gravar somente a aplicação P4 e
capturar boot. O gate de bancada desta mudança exige fetch eWeLink completo,
sucesso dos HTTPS existentes, ausência de falha TLS/AES, heap interno dentro dos
limites do projeto e sem regressão de render/UI. Nenhuma build isolada fecha
esse gate.

## ADR-078 — Conteúdo de modal contido e rolável

**Decisão:** limitar tamanho/posição do painel de cada modal à tela e tornar
rolável verticalmente sua área de conteúdo, com barra automática quando houver
overflow. Textos de inventário com quebra de linha devem crescer naturalmente
para que a rolagem revele todo o conteúdo. Ao fechar o modal eWeLink, ocultar
primeiro o teclado virtual e depois limpar os campos vinculados.

**Motivo e trade-off:** após a sincronização, o resumo dos devices pode exceder
a altura atualmente disponível. Altura fixa em labels cortava o inventário e
conteúdo posicionado abaixo do viewport deixava controles inacessíveis. A área
de rolagem mantém cabeçalho, X e botões do modal dentro da tela, permitindo
alcançar qualquer conteúdo maior. Ocultar o teclado antes da limpeza evita
alterar uma textarea ainda associada ao teclado durante o fechamento. O
trade-off é um gesto vertical de rolagem para ler resultados extensos.

**Validação:** build P4, flash somente da aplicação e ensaio em tela dos modais
de importação eWeLink, incluindo inventário longo, rolagem, fechamento pelo X
com o teclado ativo e ausência de panic. Build ou boot isolados não comprovam
layout ou fechamento em HIL.

## ADR-077 — Requisição de inventário eWeLink fora da pilha da task de rede

**Decisão:** enfileirar a gravação do inventário eWeLink usando o envelope
compartilhado do `FlashCoordinator`, protegido pelo mutex de submissão já
usado por outras gravações pequenas. Consultar a conclusão por um getter
específico que copia somente sequência/resultado, sem materializar o status
completo do coordenador na pilha da task de rede. Registrar a margem mínima de
pilha ao final da sincronização para medir a carga real.

**Motivo e trade-off:** a task `np2_netcheck` tem pilha limitada e o envelope
`flash_request_t` inclui um chunk OTA de 4 KiB mesmo quando a operação grava
somente metadados. A submissão do inventário criava esse objeto localmente e
também copiava o status completo, que inclui snapshots de dados offline. O
log de bancada mostrou um stack protection fault após a resposta cloud com
três devices. Reutilizar o buffer estático serializado remove o chunk OTA da
pilha do chamador; o getter curto reduz a cópia repetida de status no polling.
O custo é compartilhar o mesmo mutex/buffer de submissão usado por outras
gravações já serializadas. A margem reportada em execução deve orientar
eventual ajuste da pilha, sem aumentar seu tamanho sem medição.

**Validação:** build P4 limpo, flash somente da aplicação e captura do boot;
executar sincronização eWeLink, confirmar gravação sem panic, inventário
persistido após reboot e margem de pilha reportada. Nenhuma build isolada
fecha a validação de sincronização ou retenção.

## ADR-079 — Inventário eWeLink na tela principal de dispositivos

**Decisão:** publicar no AppState uma projeção sanitizada do inventário eWeLink
persistido e renderizá-la junto às câmeras na seção principal “Dispositivos”. A
projeção contém somente ID, nome, modelo, contagem de canais e presença na
última sincronização; chaves, parâmetros e configurações locais permanecem no
serviço. A área da lista é rolável verticalmente para acomodar devices
importados e câmeras no mesmo espaço limitado da tela.

**Motivo e trade-off:** a modal já mostrava os devices carregados pelo serviço,
mas a tela principal recebia somente a projeção ONVIF. Expor um getter público
com campos secretos ou fazer I/O do serviço pela UI quebraria a fronteira de
estado. O AppState copia apenas os campos necessários e mantém o app_loop como
único escritor. Como a projeção agregada é maior que a pilha disponível do
app_loop, o snapshot temporário de refresh usa armazenamento estático dentro
do único escritor. A lista compartilha rolagem entre eWeLink e câmeras.

**Validação:** build P4, gravação somente da aplicação e captura do boot sem
novo panic do app_loop. É necessária confirmação na tela para verificar a
visibilidade dos devices persistidos e a rolagem; build e boot não validam
interação tátil.

## ADR-080 — Fluxo dedicado para adicionar e sincronizar dispositivos

**Decisão:** organizar a seção Dispositivos em uma tela vazia com chamada para
importação, uma tela de escolha do tipo (Sonoff/eWeLink ou câmera ONVIF), uma
tela dedicada para credenciais eWeLink e uma tela de progresso da sincronização.
Quando houver inventário, mostrar os cards e manter a ação de adicionar
disponível. A sincronização continua sob demanda e o estado de progresso vem do
serviço. Os cards informam que o estado LAN ainda não foi consultado; não
simulam estados ligados/desligados antes da integração de controle LAN.

**Motivo e trade-off:** o fluxo anterior concentrava a importação numa modal e
não orientava o usuário entre lista vazia, seleção de tipo, login e atualização.
Páginas explícitas tornam cada etapa alcançável e a lista principal continua
sendo o inventário local. A ação “Continuar em segundo plano” apenas volta à
lista enquanto a operação segue; cancelamento de HTTPS em voo ainda não é
suportado.

**Validação:** build e flash P4 confirmam integração e boot, mas a composição,
rolagem e navegação entre telas ainda precisam de confirmação tátil no painel.

## ADR-081 — Criar cartões de dispositivos conforme o inventário

**Decisão:** construir cartões eWeLink somente para os dispositivos presentes
na projeção local. Quando uma sincronização trouxer novos devices, criar os
cartões ausentes durante a atualização da tela. Manter os limites máximos do
inventário, sem reservar objetos LVGL para slots vazios.

**Motivo e trade-off:** o backtrace do congelamento mostrou a task LVGL parada
no assert de alocação chamado por `lv_obj_add_style()` durante a construção da
página. A tela instanciava todos os cartões possíveis mesmo com poucos devices
cadastrados, pressionando o pool LVGL de 64 KiB. A construção sob demanda
reduz objetos e estilos para o inventário atual; uma nova sincronização faz a
alocação dos cartões adicionais quando eles forem necessários.

**Validação:** build limpo ESP-IDF 5.5.4 e flash P4 concluídos. Boot serial
chegou à tela inicial; a confirmação tátil de entrada e permanência na página
Dispositivos está pendente.

## ADR-082 — Pool adicional de objetos LVGL em PSRAM

**Decisão:** registrar uma única área fixa de 128 KiB em PSRAM no alocador
interno do LVGL, antes de criar a UI. Configurar
`CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=128` para que o TLSF aceite essa área.
O pool original de 64 KiB permanece;
buffers de desenho, TLS e JSON conservam suas políticas próprias. Se a reserva
ou o registro falhar, o bring-up da UI retorna erro em vez de iniciar com um
orçamento de objetos insuficiente.

**Motivo e trade-off:** a página Dispositivos ainda esgotou o orçamento de
montagem após reduzir os cartões ao inventário presente e ocultar subpáginas
durante a construção. A navegação registrou apenas 40.296 bytes livres no
pool LVGL após limpar a página anterior. O watchdog ficou repetidamente na
task `lvgl` antes de emitir `BUILD`, em endereços diferentes da biblioteca,
compatível com falha de alocação seguida pelo handler padrão de assert do
LVGL. A reserva externa limitada aumenta a capacidade dos objetos e preserva
SRAM interna para TLS, DMA e pilhas, ao custo de 128 KiB e acesso adicional à
PSRAM compartilhada com o display.

**Validação:** build P4 e gravação confirmaram o pool de 128 KiB registrado
(194.596 bytes totais, maior bloco de 131.064 bytes). O operador confirmou que
a tela Dispositivos passou a funcionar após a gravação. Uma sessão prolongada,
a navegação pelas demais telas e o acesso ao login eWeLink seguem como
validações adicionais de regressão gráfica.

## ADR-083 — Controles Sonoff criptografados pela LAN

**Decisão:** complementar o inventário eWeLink persistido com descoberta mDNS,
leitura do estado anunciado localmente e comandos Sonoff LAN criptografados por
canal. A tela publica intenção pelo event bus; uma task do serviço resolve o
IP, obtém a `devicekey` do storage e executa HTTP local no endpoint
`/zeroconf/switch` ou `/zeroconf/switches`. O AppState projeta somente
descoberta, disponibilidade, estado e resultado; chaves não saem do serviço.
Cada canal é um subcard clicável dentro do card do device; tocar nele alterna
seu estado e destaca visualmente o canal ligado. O subcard fica desabilitado
até o serviço conhecer o estado do canal ou enquanto o comando está pendente.
A cloud continua usada apenas para importação/sincronização sob demanda.

**Motivo e trade-off:** a página já mostrava o inventário importado, mas não
consultava estado nem oferecia controle. Fazer HTTP/AES ou acesso à chave na
task LVGL violaria os limites de I/O e de segredos. A task dedicada mantém o
trabalho fora da UI e serializa os comandos locais; inicialmente há uma
operação por vez e até três canais por device. O estado inicial depende dos
anúncios mDNS Sonoff; tipos sem estado interpretável permanecem sem controle
até uma consulta de estado futura. A `devicekey` persistida ainda depende da
proteção de storage disponível no firmware e isso continua sendo pendência de
segurança até haver armazenamento protegido apropriado.

**Validação:** build limpo P4, flash somente da aplicação e boot serial
confirmam integração e inicialização. O teste HIL deve confirmar descoberta,
estado inicial e alternância de pelo menos um relé; build e boot não provam a
compatibilidade do comando criptografado com cada modelo Sonoff.

## ADR-084 — Fragmentos TLS de entrada de 8 KiB

**Decisão:** configurar `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=8192` no perfil P4,
mantendo alocações mbedTLS na SRAM interna e streaming dos corpos HTTP para os
buffers já limitados por endpoint. A saída TLS continua com o tamanho atual.

**Motivo e trade-off:** a conexão TLS validava o certificado, mas o ESP-IDF
falhava ao alocar 17.058 bytes para o buffer RX, enquanto o maior bloco
contíguo interno observado era 15.360 bytes. O novo RX exige aproximadamente
8.866 bytes incluindo cabeçalho/alinhamento, reduzindo a exigência por bloco.
Como cada TLS record de entrada agora tem limite de 8 KiB, endpoints que
transmitam records maiores podem falhar; corpos HTTP maiores continuam
possíveis quando divididos em records menores e são consumidos por streaming.
Não direcionamos TLS para PSRAM porque o produto mantém TLS na memória interna.

**Validação:** build limpo, flash somente do app P4 e captura serial de 75 s
concluídos. O domínio inicial de produto retornou DNS/NTP/HTTPS `ESP_OK`, a
descoberta LAN manteve três devices e não houve falha de alocação RX de
17.058 bytes, watchdog ou panic nessa janela. A sincronização eWeLink, os demais
domínios HTTPS e estabilidade sob carga gráfica ainda precisam de validação
específica; este resultado não fecha esses gates.

## ADR-085 — Presença e estado LAN tolerantes a anúncios incompletos

**Decisão:** a bolinha de estado do card eWeLink acompanha a presença LAN. Uma
falha de descoberta mDNS isolada mantém o último estado do device e dos canais,
e dispara uma nova varredura após 5 s; a segunda ausência consecutiva confirma
offline, limpa os estados de canal e desabilita seus cards. Quando o mesmo
device continua sendo anunciado mas um pacote TXT não traz estado decodificável,
preservar seu último estado conhecido. Para devices online sem estado válido no
mDNS, consultar `/zeroconf/getState` e tentar `/zeroconf/info` como fallback,
sempre pela LAN e usando a `devicekey` local.

**Motivo e trade-off:** a bolinha era criada cinza e nunca atualizada, embora o
texto mudasse para “Online na rede local”. Além disso, a varredura substituía
todo o snapshot, convertendo uma única resposta mDNS ausente em offline e
apagando o estado do canal. Os modelos TX1C/TX2C/TX3C anunciam formatos e
estados que não foram todos decodificados pelo primeiro caminho mDNS; a consulta
LAN read-only cobre dispositivos que não incluem estado utilizável no anúncio.
A confirmação adicional evita o flicker observado; um device realmente
desconectado pode permanecer apresentado como online até a varredura de
confirmação seguinte, por alguns segundos. Consultas seguem sequenciais, com
intervalo mínimo de 200 ms no mesmo device.

**Validação:** a primeira imagem foi compilada e gravada no P4, mas a captura
mostrou três devices online e apenas um com estado conhecido:
`state_queries=2 query_ok=0`. Um toque também falhou com
`ESP_ERR_HTTP_EAGAIN`. O estado inicial dos três modelos e a alternância
física continuam pendentes. Formato dos endpoints: protocolo ITEAD
[SONOFF DIY MODE](https://github.com/itead/Sonoff_Devices_DIY_Tools/blob/master/other/SONOFF%20DIY%20MODE%20Protocol%20Doc%20v1.4.md); consulta `getState` conforme
[implementação LAN do SonoffLAN](https://github.com/AlexxIT/SonoffLAN/blob/master/custom_components/sonoff/core/ewelink/local.py).

## ADR-086 — Corrigir a mensagem criptografada e o limite do estado mDNS Sonoff

**Decisão:** conservar o IV original da requisição antes da chamada
`mbedtls_aes_crypt_cbc`, que modifica seu argumento IV; transmitir o IV
original em Base64. Serializar `sequence` como string única, conforme o
protocolo LAN usado pelo SonoffLAN. Consultar `/zeroconf/getState` antes de
`/zeroconf/info`. Ampliar o buffer de estado mDNS para acomodar os quatro
campos TXT `data1` a `data4`, até 1.020 bytes codificados, e descriptografar
diretamente no buffer de saída para limitar o aumento de pilha. Registrar
somente contagens e tamanhos no diagnóstico, sem corpo HTTP, chave ou IV.

**Motivo e trade-off:** o mbedTLS altera o IV após AES-CBC, portanto o código
anterior enviava um IV diferente do usado para cifrar o corpo. O formato
numérico de `sequence` também divergia do protocolo de referência. O buffer
anterior de 512 bytes podia truncar os anúncios de modelos com vários canais;
o JSON `params` real de TX2C/TX3C capturado no POC possui 835 bytes, embora
isso não prove que seus anúncios mDNS tenham o mesmo tamanho. Os buffers
maiores elevam o pico de pilha do worker. A primeira imagem excedeu os 8 KiB
da task `sonoff_lan`; o inventário público e o próximo snapshot da varredura
passaram então para uma área reutilizável em PSRAM. Chaves, buffers AES e
corpos HTTP continuam na SRAM interna e são apagados após o uso.

**Validação:** build P4 e `app-flash` na COM8 concluídos. Três varreduras da
imagem corrigida mostraram `online=3 stateful=3 state_queries=0`, com maior
anúncio de 664 bytes Base64 e mínimo de 1.956 bytes livres na pilha da task.
O contador bruto de estados deu sete posições para seis canais físicos
cadastrados; o POC mostra quatro posições `switches` anunciadas pelo TX2C,
embora ele possua dois canais físicos. A contagem agregada não substitui a
verificação de cada canal na UI. A alternância do relé físico por toque requer
confirmação na placa. A bolinha de disponibilidade agora usa somente o estado
LAN, independentemente da presença na última importação cloud. A referência de
criptografia, formato e consulta é a
[implementação LAN do SonoffLAN](https://github.com/AlexxIT/SonoffLAN/blob/master/custom_components/sonoff/core/ewelink/local.py); a
[API AES-CBC do mbedTLS](https://github.com/Mbed-TLS/mbedtls/blob/development/include/mbedtls/aes.h)
documenta que atualiza o argumento IV.

## ADR-087 — Configuração TLS efetiva e diagnóstico de refresh por etapa

**Decisão:** impedir build P4 com `CONFIG_MBEDTLS_HARDWARE_AES=y`, conforme
o ADR-076. A verificação usa o sdkconfig efetivo: defaults não sobrescrevem
um arquivo gerado antigo. Registrar provider fixo, fase, status, erro TLS,
duração e heap antes de destruir o cliente, sem URL, headers ou segredos.
Registrar entrega do snapshot e transições de stale no escritor de estado.

Na investigação de falha BTC, permitir um HEAD de controle em `example.com`
após timeout de conexão/resposta, somente quando restarem pelo menos 2 s
do orçamento de 20 s e no máximo uma vez a cada 10 min. Destruir o cliente
anterior antes da sonda. O resultado não substitui o resultado do mercado,
não muda cadência, autenticação TLS, cache ou estado de conectividade.

**Evidência:** em 04/10/2026, a configuração/hash do último build gravado
mantinha AES/GCM de hardware, em desacordo com o ADR-076. O configure
negativo isolado foi recusado pelo novo gate; o build com AES por software
passou e foi gravado. A primeira captura instrumentada ainda reproduziu
timeout TLS `0x8006` e timeout de headers para CoinGecko: a divergência AES
não é causa suficiente para explicar todas as falhas. A entrega do snapshot
e a projeção funcionaram; o operador confirmou Bitcoin amarelo. A memória
interna medida permanece abaixo do piso do plano. Esta decisão melhora
diagnóstico e evita regressão de configuração; não encerra o gate de rede.
Comandos, hashes e limites: [diagnóstico de refresh](DATA-REFRESH-DIAGNOSIS.md).

## ADR-088 — Fila compacta com snapshots limitados e reenvio do mais recente

**Decisão:** manter 32 entradas no EventBus, com union privada para o payload
ativo. Snapshots ficam em quatro posições estáticas na SRAM interna. Cada
entrada de produto leva índice e geração; o barramento copia o valor recebido
e libera a posição antes de devolver ao app_loop. Nenhum ponteiro emprestado
cruza tasks. Reserva, geração e liberação têm lock; operações de fila ficam
fora da seção crítica. Fila ou pool cheio retorna `ESP_ERR_TIMEOUT`, aumenta
o contador de rejeições e não retém o dado do produtor. Outros comandos
continuam usando as 32 vagas, mesmo com o pool de produto cheio.

O worker HTTPS mantém o snapshot mais recente pendente após rejeição e tenta
entregá-lo no próximo ciclo do worker, sem outra consulta HTTP nem escrita de
cache. Uma nova atualização substitui o valor pendente; snapshots já aceitos
mantêm a ordem FIFO. O app_loop continua sendo o único escritor de estado.

**Motivo e trade-off:** a fila anterior reservava 616 bytes por entrada no P4,
mesmo para comandos sem payload: 19.712 bytes. A nova estrutura tem orçamento
de até 128 bytes por entrada e quatro snapshots; o layout e ganho reais serão
medidos no ELF/boot. O limite de snapshots simultâneos cai de 32 para quatro,
com backpressure explícito e reenvio pelo produtor de rede. Isso reduz a
pressão de SRAM sem mover TLS/JSON/corpos para PSRAM. A mudança não presume
que memória seja a causa dos timeouts TLS e não encerra o gate de rede.

**Validação:** teste host do código real passou com saturação, propriedade das
cópias, reciclagem, handles antigos/wrap de geração, 8.000 snapshots de quatro
produtores concorrentes e reenvio do mais recente sem persistência extra.
Build P4 limpo passou sem warnings; DWARF/boot confirmaram item de 56 B,
pool de 1.984 B e economia de 15.936 B. App-flash COM8 verificou o hash;
primeiras consultas e renovações BTC responderam HTTP 200, com aumento da
memória interna livre e do maior bloco. A margem ainda está abaixo do piso
do plano. A bancada não provocou saturação nem reproduziu timeout nesta
imagem; a recuperação após falha e o ensaio por horas continuam abertos.
Hashes, amostra e limites: [diagnóstico de refresh](DATA-REFRESH-DIAGNOSIS.md).

## ADR-089 — Exibir a idade nas notificações

**Decisão:** o centro de notificações mostra o título, a explicação já definida
para o tipo, o estado visual de não lida e a idade do evento em relação ao
horário atual projetado pelo `app_loop`. O resumo usa pluralização natural da
contagem. Sem horário válido no evento, informar que ele está indisponível.

**Motivo e trade-off:** a projeção já contém `timestamp_unix_s`, mas a modal não
o mostrava, tornando difícil distinguir eventos recentes dos antigos. Usar a
idade relativa evita apresentar hora local incorreta antes de sincronização
ou no ajuste de fuso. A mudança é apenas visual e mantém a política atual de
histórico volátil, leitura e capacidade fixa; detalhes técnicos além dos já
guardados pelo evento não podem ser inventados pela UI.

## ADR-090 — Notificar ocorrências relevantes, não atualizações de API

**Decisão:** o centro registra falhas e alertas, além de sucessos pontuais que
representam uma transição relevante, como a recuperação da rede ou do
armazenamento. Consultas bem-sucedidas e atualizações rotineiras de dados dos
provedores não geram notificações.

**Motivo e trade-off:** atualizações frequentes de mercado ocupavam o histórico
sem exigir ação ou atenção da pessoa. A remoção reduz ruído e preserva a
capacidade para eventos que indiquem problema, alerta ou resolução. A idade,
leitura, capacidade de oito itens e controles de preferências permanecem como
estão.

## ADR-091 — Evitar cópia aninhada da projeção na pilha LVGL

**Decisão:** manter a pilha da task LVGL em 16 KiB, conforme o orçamento do
plano, e consultar somente `app_device_control_projection_t` ao instalar os
callbacks de brilho, volume e modo noturno. A navegação continua usando sua
projeção completa para montar a tela; o instalador de callbacks não cria uma
segunda cópia completa na mesma pilha.

**Motivo e trade-off:** ao abrir Preferências > Tela e som, a task `lvgl` sofreu
`Stack protection fault`; o ponteiro da falha ficou 680 bytes abaixo do limite
inferior da pilha de 12 KiB. O fluxo mantinha uma `app_ui_projection_t` durante
a construção da cena e copiava outra para obter apenas brilho e volume. O
getter tipado reduz a cópia aninhada; os 16 KiB alinham a configuração real ao
valor inicial já definido no plano. O custo são 4 KiB adicionais de SRAM
reservados à task LVGL.

**Validação:** build limpo P4, gravação somente do app e captura de boot. A
reprodução tátil de Tela e som deve confirmar ausência de panic e medir o
high-water mark sob a carga dessa construção; build e boot isolados não fecham
essa validação.

## ADR-092 — Orçamento mensal conservador da CoinGecko

**Decisão (2026-10-07):** a pedido do responsável, usar intervalo normal de
5 min e ampliar para 20 min ao atingir 80% da cota de 10.000 consultas/mês.
Bloquear novas consultas quando o orçamento local chegar a 10.000. Todos os
caminhos, inclusive refresh manual e reconexão, passam pela mesma admissão
antes do HTTP autenticado. Outubro/2026 começa com o piso informado de
8.010 créditos; meses posteriores começam em zero somente com hora confiável
e mês UTC posterior. Relógio regressivo não devolve créditos.

Reservar até oito tentativas em um registro NVS pequeno com magic/schema,
geração e CRC, via FlashCoordinator, com commit e leitura de verificação antes
do HTTPS. O worker usa os créditos reservados em RAM; erros também consomem
uma tentativa local, de forma conservadora. Reservas não utilizadas são
descartadas no reboot, sem devolução. O registro conserva o final da janela
reservada: depois de um reboot, o painel aguarda essa janela para não antecipar
consultas/escritas. Isso pode adiar o mercado por até 40 min no uso normal ou
160 min na política reduzida. Não gravar a cada cotação; em funcionamento
contínuo, reservas ficam separadas por pelo menos 40 min. Corrupção ou falha
de persistência bloqueiam CoinGecko e preservam os demais providers.

**Limite:** o contador é um teto conservador local, não uma leitura da conta.
Outros clientes com a mesma chave e consumo posterior ao screenshot não são
sincronizados. A transição de 80% pode ocorrer até sete consultas antes por
causa da reserva. Não refundar falhas evita subcontagem após timeouts.
O histórico de mercado admite gaps de até 40 min para acomodar as duas
cadências. Não alterar dependências, Kconfig, partições, C6 ou eFuses.

**Validação:** testes host passaram; build limpo P4, app-flash e boot com
reserva confirmada/intervalo de 1.200 s e HTTP 200 estão registrados em
[COINGECKO-QUOTA.md](COINGECKO-QUOTA.md), com limites de bancada explícitos.

## ADR-093 — Diagnóstico de recursos em Preferências > Sistema

**Decisão (2026-10-10):** substituir especificações estáticas por CPU, heap
interno, PSRAM, temperatura, uptime e ocupação de LittleFS/SD. Coletar memória
e deltas dos contadores idle de ambos os cores a cada 2 s no worker de controles;
app_loop publica uma projeção limitada, sem I/O na task LVGL. Habilitar somente
as estatísticas FreeRTOS necessárias com relógio ESP Timer; não alterar clocks,
dependências, partições, C6, segurança ou pipeline TRIPLE_PARTIAL.

LittleFS é consultado pelo FlashCoordinator a cada 30 s; FAT pelo worker já
dono do SD a cada 60 s, sem montar/formatar a partir da UI. Falhas invalidam a
medida em vez de mostrar zeros. Heap significa memória gerenciada pelo alocador,
não toda a SRAM anunciada. CPU é média dos dois cores por janela, não consumo
elétrico. Histórico limitado a 30 amostras (60 s), sem persistência.

Animações de arcos/barras duram 160 ms, em regiões pequenas e só para mudanças;
sem animação decorativa contínua. Suspender/cancelar em cena oculta ou manutenção.
Atualizar permanece desabilitado até a rota de produto estar qualificada;
Reiniciar conserva a confirmação e coordenação existentes. Build, boot e render
host não encerram os gates físicos de desempenho gráfico/SD concomitante.

Material Symbols Rounded: acrescentar somente cinco glyphs ausentes em 24 px,
4 bpp, com eixos estáticos opsz=24/wght=400/FILL=0/GRAD=0. O subset complementar
entra como fallback da fonte Material 24 existente, preservando seus bitmaps;
refresh já existia. CPU, RAM, armazenamento, SD e ações usam ícones Material,
sem fontes dinâmicas. Fonte oficial fixada por revisão/hash e receita em
`tools/generate_system_material_icons.py`. Evidências e limites em
[SYSTEM-RESOURCES-VALIDATION.md](SYSTEM-RESOURCES-VALIDATION.md).

## ADR-094 — Agenda local, lembretes e sincronização Google limitada

**Decisão (2026-10-19):** a pedido do responsável, substituir a agenda de
referência por criação/edição de eventos e tarefas, conclusão, exclusão
confirmada e lembretes no centro de notificações, com som sob as preferências
existentes. `app_loop` permanece o escritor único; comandos têm ID/revisão,
fila limitada e resultado projetado. Domínio sem ESP-IDF/LVGL; seis linhas
reutilizadas e máximo de 12 itens. Agenda tem getter próprio, sem lista extra
nas cópias gerais do AppState.

Dados autoritativos usam duas gerações LittleFS, schema/magic/CRC, temporário,
fsync, verificação e rename pelo FlashCoordinator. Debounce de 5 s e intervalo
mínimo de 60 s por tentativa; a UI explicita dados ainda não duráveis.
Normalizados de agenda/storage/sync usam PSRAM por capability, como exceção
documentada; corpo HTTP, tokens JSON, OAuth e TLS ficam internos. Reutilizar
o buffer de 4 KiB do coordenador, sem aumentar sua fila. Sem mudança de
partições, dependências, Kconfig, C6 ou eFuses.

Uma primeira captura mostrou mínimo de 1.572 bytes na pilha de 8 KiB do
`app_loop`. Mover sua cópia normalizada do evento recebido para um buffer
PSRAM alocado antes de criar a task, com dono único e sem segredos. Isso
mantém a fila existente e evita ampliar a reserva interna de pilha. A margem
final precisa de medição separada; a otimização não aprova o pico HTTPS.

Google usa agenda principal, tarefas pendentes da lista padrão e eventos de
sete dias; overflow é erro explícito. Sincronização manual ou a cada 5 min
no executor HTTPS existente, com admissão conservadora de SRAM. O fluxo de TV
não suporta Calendar/Tasks: autorização inicial no PC com OAuth desktop,
PKCE/state/loopback e transferência USB fisicamente armada. Renovação fica no
painel. Vault OAuth NVS de duas gerações e conta identificada por subject;
reauthorização conserva a mesma conta, sem misturar IDs de outra conta.

ETag/If-Match, escolha explícita de conflito, ID determinístico de eventos e
marcador de criação de Tasks evitam overwrite/POST cego. Tasks tem prazo só
por data na API; hora/lembrete são locais. Dia inteiro é somente leitura no
primeiro editor. Lembretes exigem hora confiável e aceitam atraso de até 15 min;
uma perda antes do commit pode repetir um alerta, sem promessa de exactly-once.

**Validação/limites:** testes host de domínio, barramento, parser, sync HTTP
simulado, persistência com falhas e UI real. Build P4 e gravação/boot são
registrados separadamente em [AGENDA-GOOGLE.md](AGENDA-GOOGLE.md).
Projeto/consentimento Google, TLS real, margem de memória, interação física,
cortes de energia e soak continuam pendentes; esta decisão não fecha gates
físicos nem aprova integração Google para produção.

## ADR-095 — Recuperar SRAM para HTTPS da agenda sem reduzir o piso

**Decisão (2026-10-19):** após vincular Google, a admissão recusou o sync com
`ESP_ERR_NO_MEM`. O ELF mostrou 64 KiB do pool builtin LVGL e 24 KiB de
buffers de diagnóstico de flash permanentemente internos, além das cópias
públicas de UI/estado. Habilitar BSS externa e posicionar explicitamente em
PSRAM somente esses dados sem segredos: pool builtin, estado visual, projeções,
snapshots do barramento, metadados de ícones, resultado de scan Wi-Fi e buffers
de diagnóstico. Locks, filas, pilhas, credenciais e TLS permanecem internos.
O fragmento do linker seleciona apenas BSS de `lv_mem_core_builtin`, mantendo
o pool de 64 KiB e a extensão existente de 128 KiB; não diminui capacidade UI.

Corpo de providers e chunk de OTA de 4 KiB passam de reservas permanentes para
alocações internas transitórias, com dono no executor HTTPS e liberação em
todos os retornos. A admissão Google conserva piso de 80 KiB, envelope TLS
de 64 KiB, workspace e maior bloco de 32 KiB. Medir mínimo durante TLS real,
integridade, retorno ao regime e margem de pilhas antes de declarar sucesso.
PSRAM exige cache ativo nesses consumidores; o experimento XiP já existente
permanece inalterado. Não mover DMA/draw, retirar framebuffer, trocar modos,
alterar dependências, C6, partições ou eFuses. Gates físicos continuam abertos.

A primeira leitura real retornou HTTP 200 em token, Tasks e Calendar, com
mínimo interno de 125.899 B e heap íntegro. Porém o worker de 8 KiB ficou com
apenas 112 B de pilha: ampliar sua pilha interna para 12 KiB, colocar o corpo
de escrita Google de 1 KiB no workspace interno transitório e liberar espaço
equivalente por BSS pública de conectividade/flash, snapshot do provider e
buffer de diagnóstico NVS em PSRAM. Repetir leitura e escrita reais e medir
>=25% livre; a primeira conexão sozinha não aprova essa margem.

Na revisão de 12 KiB, pico total medido de 51.956 B inclui workspace de
22.408 B; o restante observado é 29.548 B, incluindo HTTP/TLS e atividade
concorrente. Dimensionar envelope TLS em 48 KiB, mais de 19 KiB acima desse
restante observado, mantendo ainda 4 KiB separados para formulário OAuth.
O piso de 80 KiB e o bloco mínimo de 32 KiB não mudam. O envelope provisório
de 64 KiB recusava novas tentativas por variações transitórias de poucos KiB,
mesmo com mínimo real acima de 120 KiB. Esta é calibração por medida em
laboratório; cadeias futuras, reconexão e carga máxima continuam no gate de
memória. Novos testes conferem os dois limites e nenhuma chamada HTTP abaixo
do limiar. Repetir leitura e criação reais após a calibração.

**Evidência de 20/10:** GET dos dois eventos conhecidos confirmou a ida/volta
do evento criado na UI; a exclusão explícita do teste retornou HTTP 204.
Mínimo interno final de 123.767 B, maior bloco de 100.352 B, heap íntegro,
rede com 4.216 B livres em 12 KiB e app_loop com 2.284 B em 8 KiB.
Artefatos e limitações em [AGENDA-MEMORY-VALIDATION.md](AGENDA-MEMORY-VALIDATION.md).

## ADR-096 — Agenda com hierarquia visual e navegação local

**Decisão (2026-10-20):** a pedido do responsável, manter o design system,
usar toda a largura para a informação da Agenda e mover criação/edição para
o componente `np_modal` existente. Quatro linhas de 60 px têm data/hora
separadas, título maior, ícone e etiqueta de Evento/Tarefa, estado e lembrete.
Filtros Todos/Eventos/Tarefas exibem contagens. A ordenação usa índices limitados
a 12 itens; sem prazo e tarefas concluídas ficam ao final, sem reordenar o
modelo ou mudar a identidade usada pelos comandos.

Agrupar data/hora por início e término, ocultar término em tarefas, explicitar
novo/editar e usar os estados/estilos compartilhados para salvar/excluir.
O modal é construído no primeiro uso e reutilizado até sair da tela. Cancelar
não envia comando; fechar durante envio conserva seu resultado. ACK de comando
aceito fecha o formulário e a lista continua mostrando a persistência pendente
até confirmação. Todos os campos de texto ficam acima do teclado compartilhado,
que é liberado ao fechar/sair. Fontes, paleta, pool LVGL e pipeline permanecem.

**Validação/limites:** regressão host com LVGL e 100 lifecycles, revisão de
renders, build/flash e boot registrados em
[AGENDA-UX-VALIDATION.md](AGENDA-UX-VALIDATION.md). Fotografias/renders não
encerram os gates físicos de interação, tearing ou soak.

## ADR-097 — Agenda compacta e autorização Google contextual

**Decisão (2026-10-21):** a pedido do responsável, substituir a paginação de
quatro linhas da ADR-096 por seis de 56 px, sem rodapé. Filtros, criação,
sincronização e conexão ficam no cabeçalho; as duas últimas ações usam somente
ícones Material de 48 px. Conectar aparece apenas sem vínculo ou diante de uma
falha de autorização identificada, com redistribuição das ações vizinhas.
Tipo/estado continuam explícitos na linha, sem repetir a etiqueta em um badge.
Tarefas usam modal de 400 px; eventos, 480 px. Reutilizar os botões do formulário
para escolhas de conflito, sem alocar dois botões ocultos extras.

Acrescentar somente o resultado sanitizado `AGENDA_SYNC_AUTH_REQUIRED=-2001`
e o booleano transitório `google_auth_required` à projeção. HTTP 401 ou
`invalid_grant` no refresh HTTP 400 sinalizam autorização necessária; falhas
de rede, 403 e 5xx não são convertidas cegamente em pedido de reautorização.
O app_loop mantém o booleano até um sync bem-sucedido. Sem novos segredos,
mudança de vault, registro durável, schema, partições ou dependências.

O status Google abre detalhes no modal reutilizado e permite o gesto físico
Liberar USB para o helper confirmar o commit depois de o ícone conectar sumir.
Preservar 60 s e um comando por janela; ACK de fila continua sem equivaler a
credencial persistida. O helper mantém a segunda consulta e não dispara OAuth
de novo em `--check-link`. Mensagens comuns expiram em 8 s; instruções USB,
em 60 s, por tick da UI, sem timer com lifetime próprio.

**Validação/limites:** UI com 100 lifecycles, 12 itens alcançáveis, rotas de
conflito, modal de conexão/retorno ao editor, autorização/expiração de aviso;
HTTP simulado com invalid_grant/401/403/5xx; cinco testes do helper; barramento
com 8.000 snapshots. Build, boot e limites físicos registrados em
[AGENDA-UX-VALIDATION.md](AGENDA-UX-VALIDATION.md). Base de erros:
[OAuth desktop](https://developers.google.com/identity/protocols/oauth2/native-app#errors)
e [Calendar](https://developers.google.com/workspace/calendar/api/guides/errors).

## ADR-098 — Agenda do mês corrente e capacidade de 64 itens

**Decisão (2026-10-21):** substituir a janela móvel de sete dias pelo mês
civil inteiro no fuso configurado do painel: início inclusivo no dia 1,
fim exclusivo no dia 1 seguinte. Calcular os dois limites por calendário
local, com `tm_isdst=-1`, e converter para RFC3339 UTC na consulta. Manter
recorrências expandidas e eventos já encerrados durante o mês; a limpeza
local remove somente eventos Google fora da janela, sem apagá-los no servidor.
O cabeçalho identifica mês/ano; eventos de dia inteiro conservam a data visível.

Ampliar o limite conjunto de eventos/tarefas de 12 para 64, mantendo seis
linhas LVGL e até onze páginas. Percorrer páginas remotas de dez itens sem
aumentar o corpo de 12 KiB ou os 512 tokens JSON internos. Tokens têm no
máximo 256 bytes e são codificados; repetição, excesso de páginas, dados ou
capacidade falham explicitamente. Conservar deadline global de 120 s e uma
única conexão HTTPS. Consultas individuais são reservadas para ETags de
edições pendentes, evitando 64 GETs extras a cada polling mensal.
Um snapshot adicional limitado de 15.244 B em PSRAM permite reconstruir cada
lista e restaurá-la integralmente diante de falha. Isso também permite a
substituição de um item removido por um novo com o cache cheio, preservando
metadados de itens observados e sem exigir uma 65ª vaga temporária.

Persistência v2 tem payload de 15.244 B e lê v1 de 2.868 B com CRC antes da
expansão, preservando IDs, ETags, pendências e lembretes consumidos. Não
formatar nem regravar durante a leitura. O coordenador recebe um snapshot
PSRAM de tamanho fixo, de propriedade da fila, liberado ao consumir ou se
o enqueue falhar; acrescentar somente um ponteiro ao envelope em vez de
multiplicar 15 KiB pelos slots da fila. Armazenamento e estado continuam
em PSRAM; credenciais, HTTP, TLS e JSON continuam internos. Sem mudança
de partições, dependências, Kconfig, vault ou permissões Google.

**Compatibilidade:** o leitor atual aceita v1 e v2. Firmware antigo não lê
v2; rollback para a versão anterior requer preservar/restaurar uma geração
v1 íntegra, nunca formatar automaticamente. Gerações anteriores permanecem
até serem naturalmente substituídas pelo esquema de dois slots.

**Validação:** limites de mês, ano, fevereiro bissexto e fronteira UTC/local;
64 itens em sete páginas HTTP e onze páginas de UI; aniversários de dia
inteiro, repetição/escape de token, falha em página posterior, migração
v1→v2, falhas de fsync/rename e fallback CRC. Evidência de build/placa em
[AGENDA-MONTH-VALIDATION.md](AGENDA-MONTH-VALIDATION.md).
Contrato da consulta: [Events.list](https://developers.google.com/workspace/calendar/api/v3/reference/events/list).

## ADR-099 — Melodias locais de notificações, alarmes e inicialização

**Decisão (2026-10-21):** compor três melodias originais por categoria,
geradas localmente como PCM16 mono a 22.050 Hz, sem MP3, arquivos externos,
rede, novas dependências ou mudança de codec. Notificações: Cristal/Gotas/
Dueto (0,30–0,44 s); alarmes: Aurora/Sinos/Pulso (1,41–1,55 s); inicialização:
Boas-vindas/Ascendente/Minimal (0,50–0,93 s). Variante zero silencia a categoria.
Seno com LUT interpolada, harmônico suave e envelope de ataque/decaimento/
release, amplitude abaixo de 0,23 FS; volume continua sendo o já configurado.

`device_control_service` conserva a propriedade do codec. Sintetizar/gravar
no máximo 256 samples por passagem (512 B na pilha), mantendo controles e
mute entre os blocos. No máximo três slots coalescidos, um por categoria;
alarme pode interromper outra categoria na fronteira de bloco. Sem novos
workers, alocações por som ou reprodução em LVGL. Master desligado cancela
sons; volume zero também limpa a reprodução. Rate limit de 350 ms/categoria.
O preview de volume conserva um tom fixo, como confirmação local do controle.

Preferências de melodia ficam no serviço de notificações e são projetadas
pelo app_loop. Master de som abrange as três categorias; notificações gerais
silenciam a entrega de avisos, e alarmes/Pomodoro e boot respeitam o master
independentemente desse switch visual. Boot é solicitado somente uma vez
após sair da cena de inicialização, com preferências/controles restaurados.
Não tocar ao trocar de página, reacender ou reconectar a rede. A tela de
configuração oferece três seletores e prévias, sem I/O nos callbacks.

Registro NVS de notificações v2 de 6 B com duas gerações/CRC; leitor aceita
v1 de 3 B e preserva seus três switches com melodias padrão 1/1/1, sem escrita
no restore. Escrita permanece no FlashCoordinator, debounce e ACK existente.
Firmware antigo não interpreta v2; rollback exige cópia v1 íntegra. Sem
mudança de partições, vault, Kconfig ou esquemas de providers.

**Validação:** síntese/duração/ganho e invariância de chunks, migração e
persistência concorrente de preferências, política de mute/preview e layout
LVGL. Build, flash e evidência física descritos em `docs/SOUNDS-VALIDATION.md`.

## ADR-100 — Fotos locais, idle e widgets independentes

**Decisão (2026-10-22):** implementar o modelo visual aprovado com foto que
preenche 1024 × 600, widget Bitcoin à esquerda e relógio/data à direita.
Sem faixa opaca de tela inteira. Cada widget tem visibilidade, posição
superior/inferior, tamanho e fundo independentes. Usar fontes subsetadas
existentes (32/48 px e legendas 16 px), sem fonte dinâmica. Formulário em
modal de Tela e som. Mercado pode usar a mesma foto sob os cards a 70% e
overlay preto a 40%; ausência de foto conserva as superfícies originais.

O SD existente continua com um único dono de leitura/montagem: o worker de
weather assets também executa o passo de fotos, de até 32 KiB a cada 100 ms.
Sem nova task, conexão HTTPS, escrita de SD, formato automático, Kconfig,
partição ou dependência. Galeria em `np2/photos/photo001.npf` a `photo064.npf`,
catálogo limitado a 256 entradas, reconsulta a cada 60 s. NPFP v1: header
24 B, RGB565 LE 1024 × 600 (1.228.800 B), tamanho exato e CRC32. Conversão
JPG/PNG no computador com orientação EXIF e recorte central, preservando a
proporção e evitando barras. Uma foto não provoca releituras periódicas.

Alocação preguiçosa de até dois buffers alinhados em PSRAM (2.457.600 B),
com publicação imutável e lease do consumidor LVGL. Slot publicado ou leased
nunca é sobrescrito. A UI atualiza ambos os objetos de imagem, descarta o
cache anterior e só então devolve o lease. Falha de arquivo, CRC ou memória
preserva a última foto íntegra. Buffers de imagens são exceção limitada e
documentada à prioridade de PSRAM para display; medir a margem na placa.

Idle padrão 5 min e troca 2 min, ajustáveis. Entrada automática nas páginas
principais (exceto dispositivos/câmera) e Agenda sem formulário; modais,
drawer, teclado e notificações visíveis bloqueiam a entrada. Prévia manual
em Tela e som. Um toque completo retorna à página anterior sem disparar
ações abaixo. Notificação e fim de Pomodoro interrompem a foto; a agenda e
os serviços continuam pelo app_loop. Não alterar brilho ou modo noturno.

Preferências ficam no perfil de controles, escrito apenas pelo coordenador:
payload 6 B (configuração geral e dois bytes de widgets), registro 32 B,
CRC e duas gerações. Leitura conserva payloads legados 2/3/4 B de registros
28 B, com defaults novos, sem escrita no restore. Firmware antigo não lê o
payload novo; rollback depende de geração anterior íntegra. Não migrar os
dados de agenda, rede ou notificações por causa das fotos.

**Validação:** testes host de lease/backpressure, ausência/remoção, corrupção,
truncamento, byte excedente, falha de alocação e migração NVS; fixture LVGL
real com modal e 100 ciclos. Evidências de build/placa e limitações ficam em
[PHOTO-FRAME-VALIDATION.md](PHOTO-FRAME-VALIDATION.md). A campanha não fecha
qualificação de SD concorrente, remoção a quente, tearing ou soak.

**Ajuste visual (2026-10-22):** por pedido do responsável, o valor do Bitcoin
acompanha o tamanho escolhido para o relógio (Normal 32 px/Grande 48 px),
inclusive ao ler perfis existentes com escolhas divergentes. A modal indica
Igual ao relógio para Bitcoin. Posição, visibilidade e fundo continuam
independentes; sem alteração do formato de storage ou migração automática.

**Revisão de escopo (2026-10-22):** remover Fundo no Mercado por preferência
explícita do responsável. Fotos ficam somente no quadro de inatividade e
na prévia. Excluir a UI e os objetos de wallpaper/shade do Mercado. Bit 1
antigo fica reservado e ignorado, inclusive na admissão de leitura; leitores
continuam aceitando perfis anteriores sem regravação no restore. Nenhuma
mudança de schema, partições ou pipeline de display.

## ADR-101 — Tela do quadro de fotos, seleção e miniaturas limitadas

**Decisão (2026-10-22):** mover o quadro para uma tela própria em
Preferências, com quatro miniaturas por página, seleção persistente de até
64 fotos e tempos independentes: idle 30 s–1 h, troca 10 s–1 h. Ampliar
relógio/Bitcoin para 48/64 px com fontes subsetadas existentes. Preservar
toque para voltar e as interrupções globais. Mercado não recebe fotos.

Perfil de controles passa a payload de 16 B: seis campos anteriores,
índices de idle/troca e máscara de 64 bits. Registro 40 B, CRC e duas
gerações no FlashCoordinator; aceitar payloads 2/3/4/6 B e mapear tempos
anteriores sem escrita no restore. Default seleciona todos os IDs, inclusive
novos arquivos; desmarcar não apaga arquivos. Seleção vazia impede entrada
e prévia. Firmware anterior não lê 16 B: rollback requer geração antiga.

Miniaturas 192 × 112 RGB565 são produzidas pelo dono SD existente, no máximo
16 linhas fonte/32 KiB por passo de 100 ms. Validar header/tamanho e leituras;
o CRC integral continua obrigatório na imagem do slideshow. Dois buffers de
página, quatro imagens cada, até 344.064 B de PSRAM, sem fallback interno,
leases imutáveis e mailbox coalescida. Troca rápida de página cancela a
leitura anterior. UI só troca descritores, solta cache e devolve lease após
destacar imagens. Sem task, escrita SD, dependência ou mudança de Kconfig.

**Motivo:** seleção visual com custo limitado, sem manter 64 imagens em RAM
nem fazer I/O em callbacks. A prévia em miniatura pode refletir corrupção
de pixels que será recusada pelo CRC do slideshow; não é prova de integridade.
Ensaiar migração, seleção durante leitura, páginas rápidas, leases, falhas,
modal e ciclos LVGL. Build/flash/boot e limites em PHOTO-FRAME-VALIDATION.md;
gates de render/SD concorrente e soak continuam abertos.


## ADR-102 — Retorno automático do quadro de fotos

**Decisão (2026-10-22):** acrescentar Voltar após na tela do quadro:
Não sair (padrão), 30 s, 1/2/5/10/15/30 min ou 1 h. A sessão começa quando
a foto é revelada, tanto na prévia quanto por inatividade; trocar a imagem
não reinicia esse prazo. Ao vencer, a task LVGL esconde o overlay e reinicia
a inatividade do display, retornando à página anterior sem reentrada imediata.
Toque, notificações e Pomodoro conservam as interrupções existentes.

Perfil de controles passa de 16 para 24 B, registro de 48 B. Campo de saída
no offset 16 e sete bytes reservados explicitamente zero; CRC não cobre
padding indeterminado. Aceitar 2/3/4/6/16 B, preservando tempos, widgets e
máscara, com saída desabilitada e sem escrita no restore. Somente o dono de
controles solicita persistência serializada ao FlashCoordinator, com duas
gerações e readback. ACK antigo não limpa uma edição de timer feita em voo.
Firmware anterior não lê 24 B; rollback depende de geração legada íntegra.
Sem task nova, I/O de toque, alteração do pipeline, Kconfig ou partições.

**Motivo:** permitir limitar a exibição sem mudar a duração de cada foto
nem provocar um ciclo de fechar/reabrir no mesmo refresh. Contagem monotônica
com subtração unsigned tolera wrap do tick LVGL; opção desativada mantém o
comportamento dos perfis já instalados. Testar limites, migração e edição
em voo; registrar build, flash e limites em PHOTO-FRAME-VALIDATION.md.


## ADR-103 — Investigar Spotify nativo em projeto de compilação isolado

**Decisão (2026-10-22):** após o responsável escolher áudio no próprio
NovaPanel e confirmar Premium, avaliar cspot em tools/spotify_native_probe,
sem importar a biblioteca no firmware do produto. Fixar o candidato
philippe44/cspot em 99e5c95ad6d862a1b7845dda81fd9c8233b45b9f e bell em
46a4a8f21f3b0bc0ccefd01915b24d74f0ab9bef, registrando transitivas e patches.
Usar ESP-IDF 5.5.4/esp32p4, remover a seleção de assembly Xtensa somente na
lista de fontes do ensaio, e gerar protobuf em ambiente Python isolado.
Compilar as bibliotecas; o app vazio é somente boilerplate do CMake, não uma
imagem de player para gravar. Nenhum ajuste de Kconfig do ensaio se propaga
aos defaults/lock do produto. Exceções ficam nos alvos externos do teste.

**Motivo:** existe referência de Spotify em outro P4/IDF 5.5, mas isso não
valida Hosted/Wi-Fi, codec, memória e concorrência desta placa. Auditoria
inicial encontrou HTTP/TLS em workers próprios, leitura de corpo sem limite
central, fallback de TLS sem validação se bundle ausente e log da URL de
CDN. Nenhum desses caminhos entra no produto sem adaptação ao executor
único, validação obrigatória, buffers/prazos limitados e remoção de segredos
em logs. Áudio precisa de dono/arbitragem com alarmes e notificações.

A licença do fork efetivamente baixado é GPL-3.0-or-later; a descrição de
MIT em outro fork não se transfere para este. Registro técnico em
SPOTIFY-NATIVE-INVESTIGATION.md. Compilação não comprova autenticação,
reprodução ou estabilidade. Não gravar o app vazio nem declarar Spotify
instalado; o painel permanece na aplicação validada até um player real
satisfazer os gates técnicos. Não há aprovação de release nesta decisão.

## ADR-104 — Transporte privado do Spotify no executor HTTPS existente

**Decisão (2026-10-22):** adaptar o HTTP de cspot/Bell por uma interface C
privada, com uma solicitação em voo e espera de 25 s. O executor de rede do
produto recebe um modo específico; somente ele abre HTTPS. Copiar entradas
para SRAM interna, prazo de 20 s desde o envio, máximo 2 KiB de URL/auth,
512 B de POST, 8 KiB de JSON e 14 KiB de range de áudio. Falha de fila,
DNS, NTP, TLS, prazo ou memória conclui o pedido sem transferir corpo. Duas
referências (chamador e worker) impedem liberação durante conclusão tardia;
o slot só volta a ficar livre quando ambas forem devolvidas. Limpar dados
privados na liberação. Não há fila ilimitada nem task HTTPS adicional.

Aceitar apenas endpoints previstos de resolve/token/storage e hosts de CDN
com HTTPS, CA/hostname obrigatórios, sem redirect. Range único, status 206,
Content-Range/tamanho coerentes; JSON exige 200. Corpo chunked de JSON tem
limite e verificação de EOF. Recusar compressão. Limitar o parser HTTP do
IDF a 8 KiB de cabeçalhos por definição de compilação no componente, pois
o default de 80 KiB permite alocar uma única linha antes do callback de
rejeição. O limite vale para os demais clientes do produto: testar agenda,
mercado e clima na bancada antes de considerá-lo qualificado.

Admissão preserva piso de 80 KiB, envelope conservador de 64 KiB para TLS,
cliente/cabeçalhos e corpo separado, com maior bloco de pelo menos 32 KiB.
Nenhum buffer HTTP vai para PSRAM. Medir os certificados/CDN reais antes
de reduzir envelope; falhar por memória é preferível a ignorar o piso.

**Motivo:** o HTTP original de Bell abre conexões independentes e não
impõe os limites do produto. O adaptador do ensaio substitui HTTPClient e
torna TLSSocket indisponível para impedir bypass. Pins, licença, decoder,
sessão Connect e áudio continuam sob ADR-103. Registrar testes host e
build/flash separadamente; esta ponte não anuncia dispositivo Connect,
não provisiona credenciais, não inicia requests sozinha e não constitui
um player. Bibliotecas externas continuam fora da aplicação do produto.

## ADR-105 — PCM de música no dono existente do ES8311

**Decisão (2026-10-22):** preparar entrada PCM limitada para o futuro player,
sem criar outro dono de codec/I2S. Preservar PCM16 mono/22.050 Hz e os pinos
atuais. Converter estéreo/44.100 Hz fora da UI por downmix e FIR Q15 de 127
taps antes de decimar por dois. Estado do filtro pertence ao produtor e é
reiniciado por faixa; nenhuma alocação por bloco. Rejeitar formato diferente
até haver conversor específico. O guard do decoder não substitui a validação
de mudanças de formato durante o stream, ainda pendente no candidato.

Usar fila de PCM mono de 256 KiB, alocada explicitamente em PSRAM somente
quando solicitada e sem fallback para SRAM. A exceção é para amostras já
decodificadas; TLS, JSON e corpos HTTP continuam internos. O worker atual
de controles aloca/libera a fila e consome no máximo 256 amostras por bloco.
Produtor copia no máximo 256 amostras por chamada, com backpressure sem
descarte silencioso. Geração invalida callbacks de faixa anterior após
flush/stop; pausa retém buffer. Volume Spotify será ganho de software volátil,
subordinado ao volume geral, sem escrita de preferências por atualização.

Alertas existentes conservam prioridade; a música só usa o codec quando
o sintetizador estiver livre. Nenhum acesso a I2S em callbacks do decoder,
toque ou rede. Sem consumidor Spotify iniciado, não alocar a fila no boot.
Testar downmix, antialias, fragmentação, limites da fila, gerações, pausa e
prioridade. Medir PSRAM/stack, underruns, alarmes e flash/SD em concorrência
antes de declarar música contínua validada. A entrada PCM não equivale a
pareamento ou reprodução Spotify e não fecha os gates de ADR-103/104.

Ensaio opt-in NP2_MUSIC_AUDIO_BENCH (OFF por padrão): uma única prova de
3 s após 45 s, na task de manutenção existente, prazo máximo10s. Workspace
interno limitado, fila real PSRAM e sinal estéreo sintético baixo passam por
conversor e owner do codec; nenhum segredo/HTTP/arquivo. Após medir, compilar
OFF e gravar a imagem normal. Evidência em SPOTIFY-AUDIO-PIPELINE.md.

## ADR-106 — Runtime Connect experimental e fronteiras de entrada

**Decisão (2026-10-23):** reutilizar os pins de ADR-103 e as adaptações de
ADR-104 em uma recipe CMake comum ao harness e ao produto. A integração
continua opt-in, desabilitada por padrão. Antes de ativar o receiver, limitar
pacotes AP/Shannon a 8 KiB, mailbox a dois pacotes e Mercury a 16 partes,
recusando fragmentação, truncamento, tamanho incoerente e MAC inválido.
Saturação deve encerrar a sessão, nunca crescer sem limite. Sockets têm
conexão e transferência limitadas; EOF é falha imediata. As tasks externas
usam a API WithCaps do IDF para alocação e liberação, pilhas PSRAM explícitas,
prioridade até 3 e captura de exceções na entrada. Não usar timers para
liberar TCB/pilha de task encerrada. Objetos de workers permanecem vivos
até reboot; não oferecer recriação ilimitada após falha.

Credenciais do app passam somente pela manutenção USB física já existente,
em mailbox privada e RAM. Nenhum segredo em CMake, binário, estado público,
logs ou anúncio mDNS. Pareamento LAN só durante janela explícita; só anunciar
receiver após inicialização real. Logs externos são silenciados, expondo
apenas fases/resultados numéricos locais. HTTP permanece no executor único
com os pisos de SRAM de ADR-104. Buffers de áudio decodificado continuam em
PSRAM conforme ADR-105; qualquer exceção adicional exige medida própria.

**Motivo:** compilar a biblioteca e ouvir PCM sintético não valida entrada
Connect, ciclo de vida de tasks nem decoder. Separar testes de parsing,
autenticação, decoder e reprodução física, registrando precisamente o que
foi executado. Manter a imagem normal recuperável e não promover o ensaio
a uma integração qualificada sem áudio real e concorrência medidos.

Primeiro incremento executável: `NP2_SPOTIFY_PAIRING_BENCH`, OFF por padrão,
vincula somente o necessário para validar app, assinatura RSA/SHA-1 do AP,
pareamento Zeroconf e autenticação de conta. Não inicia Mercury/TrackPlayer,
não anuncia reprodução validada e apaga app/conta ao terminar. Servidor LAN
em 8080, URI fixa, POST máximo 4 KiB, até seis tentativas e janela de 180 s.
mDNS continua pertencendo ao serviço LAN existente. HTTPD e worker de ensaio
usam pilhas explícitas em PSRAM; corpos e workspace JSON ficam internos.
O build de laboratório habilita exceções C++ e pool emergencial de 2 KiB;
`-fexceptions` sozinho com o Kconfig desligado faria a libstdc++ abortar.
O perfil normal permanece sem essa alteração. Verificação do AP usa a
âncora pública id 0 também utilizada pelo librespot, antes de derivar a
sessão; nenhuma credencial de usuário é necessária para esse pré-teste.

## ADR-107 — Decoder Vorbis com orçamento e saída controlada

**Decisão (2026-10-23):** antes de ativar streaming, envolver o Tremor privado
em uma fronteira C com um único decoder, até 2 MiB de dados e 2.048 alocações
vivas, explicitamente em PSRAM. A memória de codec/PCM e buffers de áudio
decifrados é exceção medida; TLS, JSON e corpos HTTP continuam internos.
Não substituir malloc global nem alterar o checkout externo. Compilar cópias
com `_ogg_malloc/calloc/realloc/free` direcionados ao ledger do decoder.

O candidato não checa todos os retornos de malloc. Uma falha de recurso
dentro de Tremor deve voltar por `setjmp/longjmp` exclusivamente entre frames
C do wrapper/codec, liberar o ledger inteiro e invalidar o arquivo. Não
lançar exceção C++ através do decoder nem retornar NULL para pontos que o
desreferenciam sem verificar. Callbacks C++ de I/O continuam capturando suas
próprias exceções antes de retornar a C; não podem executar alocador do codec
ou saltar sobre RAII. Fora da chamada protegida, esse alocador não é usado.
Um decoder por vez, nenhuma task de UI/rede toca esse contexto.

Validar Vorbis estéreo/44.100 Hz, leitura incremental, seek/EOF, formato
incompatível, truncamento e falha em cada ponto de alocação com fixture Ogg
gerada localmente. Medir bytes/pico/limpeza e PCM comparado ao decoder de
referência antes de acoplar ao Spotify e ao owner do ES8311. Este gate não
equivale a faixa Spotify, throughput de CDN ou qualificação do codec.

**Motivo:** colocar o decoder original em SRAM consome a margem de HTTPS;
apenas trocar malloc por PSRAM deixaria caminhos de falha capazes de derrubar
o painel. O limite e a fronteira de erro precisam ser exercitados antes do
primeiro stream, preservando a imagem de pareamento já aprovada na placa.

## ADR-108 — Sessão Connect nativa e entrega PCM com backpressure

**Decisão (2026-10-23):** após o pareamento/autenticação física de ADR-106 e
o gate Vorbis de ADR-107, habilitar somente por opt-in um runtime Connect.
Credenciais continuam privadas/em RAM, uma sessão por boot, sem reconnect
automático. Recusar startup sem país recebido do AP; congelar esse campo
antes de iniciar a fila e assinar Mercury. Callbacks/eventos são vinculados
antes das tasks. Assinatura/app/decoder precisam passar antes do anúncio.

Serializar estado Spirc por mutex recursivo privado; proteger mapas pendentes
sem invocar callbacks sob o mutex. Máximos: 16 respostas, duas assinaturas,
quatro chaves e 128 referências de faixa. Callbacks de faixa usam weak_ptr;
estado compartilhado da fila é atômico. Reader usa mailbox de dois pacotes,
com espera de até 200 ms antes de encerrar por saturação. Socket espera até
130 s pelo primeiro byte do cabeçalho de sessão ociosa, até 15 s por pacote
parcial/conexão/escrita. Falha termina a sessão, sem tempestade de retries.
No preflight sem conta, erro de socket permite até três tentativas com500ms
entre elas; cada tentativa conserva prazos limitados. Outros erros encerram.
Diagnóstico enumera somente mensagens fixas conhecidas, sem imprimir exceções.
O mesmo limite vale para a conexão inicial após receber o blob, antes de
enviar autenticação; recriar MercurySession por tentativa para descartar
cipher/DH parcial. EOF/DNS/connect/read/write têm códigos numéricos próprios,
sem imprimir destino. Pacote/endpoint inválido não conta como socket retryable.

Tarefa de player única permanece criada em idle, stop não faz join segurando
o mutex Spirc. Objetos dos workers permanecem vivos até reboot para não
liberar `this` durante o epílogo da task. Worker principal e decoder usam
48 KiB de stack PSRAM; fila 32 KiB, reader 4 KiB, prioridade máxima 3.
Liberar estado DH/auth duplicado depois de autenticar, antes de iniciar mídia.

PCM é entregue ao owner do ES8311 existente, estéreo44.100→mono22.050,
fila256KiB PSRAM, volume Spotify volátil inicialmente20% subordinado ao geral.
Callback só converte/copia; nunca faz I2S, UI ou rede. Só reconhecer bytes de
entrada quando o PCM pendente estiver enfileirado, sem converter duas vezes
em retries. Quatro marcadores limitados relacionam ID da faixa à amostra
consumida; notificar Spirc fora do mutex PCM. Prefill de1s, pausa retém fila,
flush/seek reinicia geração, fim drena. Alertas continuam prioritários.

O ensaio inicial usou Vorbis96kbps; após recusa da chave desse arquivo na
placa, testar preferência160kbps, formato também presente nos metadados.
Isso é hipótese de compatibilidade, não solução confirmada; há relato de
falha em96kbps em librespot-org/librespot#1433. Se o servidor recusar também
160kbps, registrar a limitação em vez de repetir indefinidamente a solicitação.
Ranges/cache de áudio de8KiB reduzem SRAM por
requisição. A política HTTP aloca a extensão validada do range, no máximo
14KiB, em vez de reservar sempre o máximo; conservar piso80KiB/envelopeTLS64KiB.
Medir mínimo local de heap por HTTPS e registrar só contadores/resultados,
bytes/latência/stack. Nenhum segredo/URL/corpo em diagnóstico ou estado público.
Após medir falta de SRAM na sessão ativa, containers das duas listas de
referências (até128faixas cada) e seus textos URI/context passam a allocator
privado em PSRAM, assim como PCM1KiB. Depois do ensaio seguinte, GID de faixa
também usa PSRAM, com callback protobuf próprio; credenciais não mudam de allocator.
Callbacks protobuf de texto conhecem explicitamente o tipo da string privada.
Sem alterar alocação global nem transferir HTTP/JSON/TLS para PSRAM.
O job HTTP passa a alocar em SRAM interna somente os tamanhos já validados
de URL/header/corpo, em um bloco contíguo privado, preservando caps e limites.
Isso evita reservar buffers máximos de autorização/URL em requests pequenos;
refs, prazo único e limpeza de todo o bloco conservam a semântica anterior.
Após o primeiro ensaio físico, limitar respostas resolve/token a4KiB;
storage conserva8KiB. Isso reduz reserva e rejeita respostas maiores, sem
alterar o piso80KiB nem envelopeTLS64KiB. O resolve da sessão havia sido
recusado antes de TLS com154.619B disponíveis e reserva genérica155.648B.

Encerrar por desconexão/falha, 300s sem primeiro áudio ou pausa de10min.
O prazo inicial foi ampliado de120s no laboratório para permitir a interação
do operador durante a captura. Carregamento por faixa espera até60s, em
fatias de100ms interrompíveis por stop/reset, em vez do prazo externo5s que
era menor que request20s/backoff30s. Não abandonar a faixa enquanto a primeira
tentativa de rede ainda está dentro do prazo. Estados de frame/fila/metadata/
chave/formatos e status Mercury são apenas numéricos no diagnóstico privado.
Sessão tocando não tem limite artificial de duração. Token pode renovar
somente pelo executor HTTPS único. Persistência de credenciais, retomada
após reboot/rede e UI de player ficam fora deste ensaio até reprodução física.
No ensaio de áudio, a janela de seleção é de5min; pareamento isolado conserva
180s. Exigir o perfil XiP/stack PSRAM já qualificado no baseline; não habilitar
novos flags de flash para viabilizar tasks externas. Falta qualificar música
concorrente com escrita de cache/preferências no mesmo perfil.
**Motivo:** conectar os caminhos já medidos sem criar dono de áudio/TLS
concorrente, evitando deadlock de stop, áudio duplicado e crescimento de filas.

## ADR-109 — Encerrar reprodução Spotify nativa e usar controle remoto oficial

**Decisão (2026-10-24):** por solicitação do responsável, substituir no produto
o experimento cspot/Connect por uma tela de controle remoto usando Web API
oficial. Áudio fica no celular, PC ou outra saída Spotify. Build normal mantém
pareamento/reprodução nativos OFF; fontes e evidências dos ADR-106–108 ficam
como histórico de laboratório, sem afirmar que reproduziram música.

OAuth PKCE no helper local, apenas Client ID/refresh token no vault privado,
persistência e rotação pelo FlashCoordinator. Access token somente em SRAM.
UI publica intenção, app_loop publica estado e o executor HTTPS existente
executa token/API/capa sequencialmente; não criar worker/TLS paralelo.
Polling de 5 s apenas na tela aberta, 429 com Retry-After, 401 pede nova
autorização. Nenhuma retentativa automática de comando com efeito remoto.

Capa opcional de até 64 px e 8 KiB JPEG, origem i.scdn.co estritamente
validada e sem autorização de conta no download. Decoder TJpgDec privado
derivado do LVGL fixado usa pool de 8 KiB; dois slots de 8 KiB RGB565 em
PSRAM com leases são exceção explícita para dados visuais, sem mover TLS,
JSON ou corpo HTTP para PSRAM. Falha de capa preserva controles/placeholder.

**Motivo:** o servidor recusou a chave de áudio `00 01` nos testes nativos,
sem PCM musical. A API oficial atende o novo objetivo sem decoder musical,
Mercury/AP ou saída de áudio Spotify no painel. Uso pessoal exige Premium e
conta autorizada no app; a UI deve expor indisponibilidade/restrições. Detalhes,
limites e procedimentos em [SPOTIFY-REMOTE.md](SPOTIFY-REMOTE.md). A decisão
não fecha qualificação física, OTA, segurança ou estabilidade gráfica.

## ADR-110 — Card da Home com widgets Clima/Spotify e escolha manual persistida

**Decisão (2026-10-24):** a pedido do responsável, usar o card esquerdo de
560×376 da Home como área de widgets. Seletor Clima/Spotify manual, lembrando
o último escolhido; nenhuma troca automática ao iniciar música. O player
remoto dispensa tela própria. Saídas, volume e vinculação ficam em modal.

A UI mantém as duas árvores limitadas, pausa a animação do clima oculto e
só pede polling quando o widget Spotify está visível. Outras páginas e
quadro de fotos suspendem a leitura. Sliders/modal não iniciam swipe de
navegação ou idle do quadro de fotos. O conteúdo de mercado permanece.

A intenção percorre EventBus/app_loop. A escolha usa um registro independente
`np2_home`, duas gerações/CRC, debounce de5s e intervalo mínimo60s pelo
FlashCoordinator; não reutilizar bytes reservados do perfil de foto/som,
porque firmware antigo os rejeitaria no rollback. Default Clima sem registro
válido. Cortes/CRC e restauro precisam de evidência própria.
**Motivo:** acesso ao player sem navegar, ampliação futura da área de widgets
e preservação das configurações existentes. Sem promessa de widgets novos.

## ADR-111 — Widget por deslize e player equilibrado na Home

**Decisão (2026-10-24):** implementar a proposta visual 1 selecionada pelo
responsável: capa 160×160 à esquerda, título em até duas linhas à direita,
artista/saída, progresso com tempos separados e transporte centralizado.
Pausa/reprodução em círculo verde; anterior/próxima usam glyphs Material
próprios. Opções ficam em ícone, atualização dentro da modal. A superfície,
os demais cards e o header real continuam seguindo os tokens do produto.

Substituir os botões Clima/Spotify por dois pontos e gesto horizontal dentro
do card: esquerda abre Spotify, direita abre Clima, sem rotação automática.
Exigir pelo menos 72px e predominância horizontal de 2:1. Gestos nos limites
pertencem ao card e não navegam páginas. Sliders, transporte e modal mantêm
o próprio gesto; consumir o clique residual de um swipe reconhecido para
não abrir a página de clima. Navegação fora do card permanece existente.

Reutilizar intenção e persistência do ADR-110; não escrever no toque. Troca
atômica entre árvores existentes, sem animar duas superfícies completas.
Capa mantém orçamento até64px/8KiB e é ampliada visualmente para160px.
Mensagens de falha/vinculação permanecem disponíveis; omitir a frase normal
redundante e o estado de polling do layout principal.
**Motivo:** priorizar música e capa, reduzir ruído visual e conservar controles
com áreas de toque de48/64px no espaço fixo da Home.


## ADR-112 — Capa Spotify como fundo do card

**Decisão (2026-10-24):** atender ao pedido de usar a capa como fundo da área
Spotify na Home. Aplicar COVER centralizado em560×376, sem distorcer a
proporção, com clipping no raio do card e camada preta190/255 (~75%).
Texto e controles ficam acima da camada; não repetir a miniatura no layout
padrão. Sem imagem, usar a superfície gráfica existente.

Reutilizar o mesmo descriptor/lease até64px, sem segundo download, bitmap
ampliado persistido, worker ou mudança de orçamento TLS/JSON. Alinhamento
COVER do LVGL mantém pivot0,0 e centraliza o corte. Sem animação de capa,
blur dinâmico ou troca automática de widget. Fontes e áreas48/64px mantidas;
labels de tempo usam text_2 para contraste sobre capas claras. Atualização
continua condicionada à mudança de dados e geração da capa.

**Motivo:** dar identidade à música no card mantendo legibilidade, fluxo de
controle remoto e concorrência. Exigir teste com pool LVGL comparável à placa,
recorte/cantos, ausência de capa, descarte e medição física; host/build não
fecham estabilidade gráfica ou soak.


## ADR-113 — Capa em meia área com degradê e resolução até300px

**Decisão (2026-10-24):** implementar o mock4 aprovado: arte à direita,
viewport360×376 emx200 no card560×376, máscara horizontal de superfície opaca
até aproximadamente metade do card e transparente à direita, mais fade
vertical para superfície na região dos controles. Texto em coluna224/240px,
título em até duas linhas, sem miniatura. Descriptors de degradê pertencem
à view final, não à variável temporária de construção. Viewport impede que
COVER pinte fora da área devido à ampliação/corte; cantos permanecem limitados.

Preferir a maior variante anunciada até300×300, mesma allowlist HTTPS do
ADR-109, com fallback anunciado até64px se download/decode/reserva falhar.
JPEG até32KiB, sem redirect/Bearer, mesmo executor/prazo18s. Dois slots RGB565
300×300 fixos em PSRAM (360016B), substituem os dois slots64px (16400B):
exceção visual medida; pool decoder continua8KiB. TLS/JSON/HTTP permanecem
em SRAM. Corpos maiores que8KiB usam buffer SRAM transitório somente após
headers/TLS, limitado ao Content-Length ou32KiB+sentinela, exigindo reserva
livre de80KiB após alocação e maior bloco suficiente. Alternativa pequena
usa o buffer HTTP8KiB existente. Liberar a extensão em sucesso, erro, fallback
e encerramento. Não aumentar o corpo JSON nem criar conexão concorrente.

Cache considera a identidade preferida mesmo se entregou fallback, evitando
tentar baixar a grande em cada poll; tentar de novo quando a identidade
mudar. 429 conserva Retry-After sem fallback imediato. Sem foto, superfície
padrão; falhas de capa não desabilitam controle remoto. Campanha física deve
medir qualidade300px, heap mínimo, stack, CPU e convivência com agenda.
**Motivo:** aumentar definição no recorte aprovado sem penalizar segurança de
memória, legibilidade, fluxo ou serialização dos requests.


## ADR-114 — Restaurar capa integral e recuperar resolução maior

**Decisão (2026-10-24):** após relato de imagem ausente/degradê insatisfatório,
o responsável pediu voltar à imagem anterior e melhorar só a qualidade.
Restaurar o fundo integral aprovado no ADR-112: COVER560×376, preto190/255,
texto em largura512px, sem viewport lateral, gradientes ou miniatura. Preservar
300px/32KiB/reserva80KiB e demais limites de download/decode do ADR-113.

Fallback64 temporário será reavaliado no máximo uma vez por minuto enquanto
o widget estiver visível, aproveitando o job de metadata. Durante essa nova
tentativa manter capa/geração atuais; se falhar, não limpar a foto nem baixar
a mesma miniatura de novo. Ao obter a variante maior, substituir via slots/
leases existentes e parar retentativas. 429 continua respeitando Retry-After.
Ao receber204/sem item, zerar também a identidade de tentativa e seu prazo:
a retomada do mesmo álbum deve baixar novamente sem herdar o backoff de60s.
Exceção: Retry-After explícito de429 permanece válido após204.
Não criar task, conexão simultânea ou persistência de capa. Validar o render
integral com fontes64 e300px no pool194592B, não apenas o decoder.
**Motivo:** conservar o layout aceito, melhorar nitidez e evitar que uma falha
transitória fixe a baixa resolução até trocar de álbum.


## ADR-115 — Stride e pixels da capa compatíveis com alinhamento64 do painel

**Decisão (2026-10-24):** após relato físico de capa300px ausente, reproduzir
o render com LV_DRAW_BUF_STRIDE_ALIGN=64 e LV_DRAW_BUF_ALIGN=64 já existentes
no firmware. Não mudar Kconfig/BSP/adapter. O host anterior usava alinhamento
padrão e não exercitava o mesmo caminho de decoder/postprocessamento.

Uma linha300px RGB565 ocupa600B, mas o decoder LVGL exige stride640B nesse
perfil. O descriptor sem padding provocava cópia192000B para ajustar stride;
com pool194596B já contendo a árvore UI (maior bloco inicial131064B na
placa), essa cópia falha e a capa não desenha.
Fixture64 passava porque stride128B já era múltiplo64. A regressão foi
reproduzida no host:64 passa,300 falha na asserção de pixels visíveis.

Alinhar início dos pixels e cada slot em64B; usar stride por imagem arredondado
para64B e gravar cada linha nesse stride desde o TJpgDec. Descriptor informa
stride/tamanho reais. Slots fixos máximos640×300 com cabeçalho alinhado,
192064B cada/384128B no total em PSRAM. Padding zerado a cada decode. Pool
JPEG8KiB, limites300px/32KiB e reserva interna80KiB permanecem. Não marcar
imagem como modificável para empurrar uma cópia oculta ao renderizador.

Host passa a usar alinhamento64 e pool194592B, testa64/300px, pixels visíveis,
cores/últimas linhas e verifica que o decoder devolve o mesmo ponteiro de
pixels, sem repack. Assert de compilação garante offset/tamanho alinhados.
**Motivo:** o objetivo inclui imagem desenhada, não apenas download/decodificação
concluídos. Preservar o fundo integral pedido e permitir a maior resolução
sem crescimento grande do heap LVGL nem alterar configuração global.

## ADR-121 — Ensaio de decoders H.264 Main/CABAC para a Tapo C200

**Estado:** ensaio de laboratório; nenhum decoder foi adotado no produto.

**Contexto:** `/stream2` da C200 é H.264 Main, CABAC, 640×360, progressivo, sem
weighted prediction (`entropy_coding_mode_flag=1`, medido com FFmpeg). O
TinyH264 do `esp_h264` aceita somente Constrained Baseline. Trocar o
`profile_idc` no SPS não resolve, porque os slices continuam em CABAC.

**Decisão:** comparar `edge264` e Sub0h264 antes de alterar
`camera_stream_service.c`. O Sub0h264 entra em `firmware/components/sub0h264`,
fixado no commit `15421eeade48774929bc0221ea0418a25462c222` (MIT), e só compila
com `-DNP2_H264_DECODE_BENCH=ON`. Com a flag OFF o componente fica vazio. O ensaio
decodifica uma amostra embutida, fora do Git, uma vez após o boot, com LVGL e rede
ativos, sem câmera, credencial ou persistência. Ele registra tempo por slice,
conversão RGB565, memória e MD5 dos quadros para comparar com o FFmpeg.

**Limites conhecidos do candidato:** sem B-frames, I_PCM CABAC e
`disable_deblocking_filter_idc=2`; sem ocultação de erro; `std::vector` dentro de
`noexcept` aborta sem memória, contrariando a política de falha recuperável; sem
fase de robustez a streams malformados. O número publicado de 30,2 fps vem de uma
amostra de 27.190 B em 119 quadros (cena quase parada), com a aplicação de teste
isolada. Isso não vale para a C200 nem para o painel.

**Gate para adoção:** C200 real com MD5 igual ao FFmpeg, fps sustentado acima do
fps da câmera na UI máxima, sem regressão de render (G2), piso interno preservado
e teste de stream corrompido. Se nenhum decoder passar, as alternativas são
decodificar só IDR ou usar um bridge na LAN. Qualquer delas exige novo ADR.

**Resultado de 2026-11-04 (ver BRINGUP-EVIDENCE):** `edge264` descartado, com 1,58 fps
na amostra de 512 kbit/s. Sub0h264 fica como candidato **não adotado**: 26,5 fps e MD5
igual na amostra C110 quase estática, mas corrupção silenciosa em streams x264, inclusive
Baseline/CAVLC, com causa não determinada. Só a amostra real da C200 com movimento decide.
A conversão RGB565 custa cerca de 55 ms por quadro e precisa de solução própria, seja qual
for o decoder.

**Atualização de 2026-11-04, amostra real da C200:** a corrupção anterior era um defeito de
wrap de `frame_num` no DPB, com causa raiz identificada e corrigida pelo patch
`firmware/components/sub0h264/patches/0001-frame-num-wrap.patch`, que substitui a conclusão
acima de causa não determinada. Com ele, a C200 fica com MD5 igual ao FFmpeg no PC e na placa,
a 23,03 fps de decode com vetores em PSRAM, contra cerca de 14,4 fps da câmera. Estado dos gates:
MD5 da amostra aprovado; fps da cadeia completa **reprovado** (10,06 fps com RGB565); vetores em
SRAM reprovados no piso interno; G2, stream corrompido e OOM não testados. Sub0h264 segue
**não adotado**, porém é o único candidato viável. Riscos a resolver ou limitar antes da adoção:
QP abaixo de cerca de 17 diverge em silêncio, com causa não determinada; vários slices por quadro
e predição ponderada também divergem, sem ocorrer na C200 medida. Como o patch é local, atualizar
o commit de origem exige reaplicá-lo ou confirmar que o upstream o substitui.

**Atualização de 2026-11-04, integração:** o Sub0h264 substitui o `esp_h264` em
`camera_stream_service.c`, ainda como **candidato em bancada, não aprovado para release**.
Gates pendentes: G2 com vídeo, stream corrompido, soak, QP baixo e aborto por falta de
memória. A admissão exige 96 KiB internos livres e bloco de 3 MiB em PSRAM. O limite de 640×368
e 4 referências delimita a alocação. Os vetores ficam em PSRAM pelo patch 0002. A publicação
foi limitada a 5 fps porque o decode ao vivo oscilou entre 42 e 85 ms por quadro. Com atraso acima
de 1 s pelo relógio RTP, o serviço pula até o próximo IDR. `esp_h264` e `edge264` seguem no build,
sem uso pelo serviço, até a remoção em mudança própria.

**Conta da câmera:** salva depois de `PLAY 200` pela tarefa de vídeo, via `FlashCoordinator`,
em NVS (`np2_camera`) com duas gerações e CRC, vinculada ao IPv4 para não enviar a senha a outro
host. A UI só consulta se há conta e nunca a lê. Usa a mesma política de retenção do Wi-Fi e do
Spotify: em `NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=ON` fica em NVS **sem criptografia**;
produção exige NVS Encryption com Flash Encryption e sem isso a conta não é salva. Trocar o IP
da câmera pede a conta de novo.

**Cadastro de câmera (2026-11-04):** uma tela só. O IP é validado primeiro: o painel pede o vídeo sem
conta; se a câmera responde `401` aparecem usuário e senha, se responde `200` basta adicionar. A conta só
é salva depois de uma sessão RTSP completa (`PLAY 200`). O cofre guarda até 4 contas, uma por IPv4, e uma
câmera sem senha fica com conta vazia. A lista de câmeras (endereço e modelo, sem segredo) fica em NVS
(`np2_camera`, chaves `inv0` e `inv1`) com duas gerações e CRC, escrita pelo `FlashCoordinator` a partir da
tarefa ONVIF, nunca de callback de toque. O registro compartilhado só aceita `schema_version=1`; a mudança de
formato do cofre é distinguida pelo tamanho do payload.

**Descoberta:** o multicast WS-Discovery sozinho não achou a C200. A busca varre o /24 do painel por RTSP na
porta 554 em lotes de 6 (lwIP tem 10 sockets), confirma com um pedido RTSP e marca ONVIF pela porta 2020.
Tem custo de cerca de 55 s no pior caso e ocupa a tarefa ONVIF; por isso roda só ao abrir o cadastro.

**Pilha do LVGL:** 20 KiB, com folga mínima medida de 4.000 B com vídeo e tela de cadastro; o log de
recursos passa a trazer `lvgl_stack_min`. A margem é estreita e deve ser acompanhada.

**Layout do visualizador (2026-11-04):** o cartão da câmera não tem cabeçalho; o vídeo ocupa o cartão, e
reconectar, trocar conta e fechar flutuam sobre a imagem, com fundo translúcido. O modal compartilhado é
reutilizado (título, subtítulo e divisor são ocultados) para manter o comportamento de fechar e de parar o
vídeo ao sair. A tela cheia por trás do cabeçalho do painel foi testada e reprovada. O nome da câmera deixou
de aparecer; se houver várias câmeras, reavaliar.

**Fluidez do vídeo (2026-11-04), tentativa revertida:** o caminho com PPA mostrou que a conversão e a escala
cabem no hardware (layout `O_UYY_E_VYY` confirmado) e que a tela precisa receber quadros com passo de linha
múltiplo de 64 bytes. A fila de apresentação com atraso fixo e o consumo pela tela não deram um resultado
confiável: houve panic no alocador do LVGL com a fila ativa, sem causa conhecida, e depois entrega de poucos
quadros. O firmware voltou à versão commitada (5 fps, conversão pela CPU). Se o tema for retomado, o menor
passo plausível é PPA assíncrono com passo de linha de 864 pixels e exibição do quadro mais recente, sem fila,
com a causa do panic investigada antes de reintroduzir qualquer fila de apresentação.
