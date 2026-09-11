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
