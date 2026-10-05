# Auditoria do NP2 — 04/10/2026

## Conclusão

O NP2 já é um protótipo funcional de painel: inicializa o P4 e o display,
opera Wi-Fi via C6, busca dados reais, mantém cache offline e oferece uma UI
de produto com configurações, perfil, Pomodoro e integrações IoT. Ainda não
há evidência suficiente para release ou produção. O caminho crítico é
estabilizar a carga atual, concluir OTA/recovery P4/C6 e qualificar segurança,
energia, memória e operação prolongada.

As fases 0–4 foram encerradas **com escopo limitado**, principalmente numa
unidade. Esse encerramento permite desenvolvimento; não equivale ao aceite
integral G0–G4 do plano premium. A interface e as integrações de outubro
mudaram a carga de memória, rede e render e precisam de regressão própria.

Nesta auditoria, o build limpo P4 passou. Foram executadas 29 verificações de
teste: 25 passaram e quatro falharam. Uma verificação adicional do índice de
fusos passou. Também foram encontrados caminhos de falha por inspeção de
código, descritos abaixo. Não houve alteração de firmware, flash, atualização
C6, corte de energia ou escrita de eFuses nesta atividade.

## Escopo e fotografia examinada

- Base Git: `cf335d9f9c20773f0e3b599d22c85290de83f247`, de 02/10/2026.
- 182 commits e 411 arquivos rastreados no início da análise.
- Árvore inicial: 29 arquivos rastreados modificados e 12 entradas não
  rastreadas, incluindo diretórios. O diff rastreado tinha 4.442 inserções e
  183 remoções; os arquivos novos não entram nessa contagem.
- Incluídos na revisão: instruções, guia de bring-up, plano premium,
  roadmap, ADRs, registros de bancada, manifestos/locks/defaults/partições,
  composição e serviços, UI compilada, contratos e ferramentas de teste.
- Conferidos os registros recentes de bancada de 03–04/10 e as distinções
  entre build, gravação, captura serial e confirmação física do operador.
- Inspeção do código identifica defeitos possíveis sem afirmar que todos
  foram reproduzidos em hardware. Não é uma prova exaustiva de ausência de
  bugs, vazamentos ou exposição de segredos.

Os arquivos recentes de eWeLink, centro de notificações e `edge264` ainda
estavam fora do Git. Um checkout somente do commit citado não reproduz a
árvore examinada. As alterações do usuário foram preservadas; esta entrega
acrescenta somente o presente relatório.

## O que foi implementado

| Área | Entrega presente | Validação e limite |
|---|---|---|
| Base P4 | ESP-IDF, manifesto/lock, defaults, partições e configuração do editor | Novo build limpo passou; pipeline completo de release ainda falta |
| Base C6 | Recipe fixada, lock, patch oficial auditado, verificadores e build determinístico | Dois builds com hashes iguais registrados em 13/09; não substituem identificação do firmware instalado |
| Hardware/display | P4 v1.3, PSRAM, EK79007, GT911, backlight e display RGB565/180°/3 FB/TRIPLE_PARTIAL | Boots e ensaios anteriores registrados; carga atual precisa de regressão |
| Flash/render | FlashCoordinator e variante XiP em PSRAM; NVS e LittleFS sob render | Campanha limitada G2 passou; XiP continua condicionado ao gate OTA/produto |
| Estado e eventos | `app_loop`, projeção de UI, EventBus de 32 itens e políticas portáveis | Escritor central presente; contratos de resultados/ownership ainda incompletos |
| Wi-Fi/C6 | Inicialização assíncrona, scan, associação, DHCP, backoff, cooldown e recovery limitado | C6 3.0.6/RPC v2/SW_AGGR observado; falha física de rádio ausente/travado foi excluída |
| Provisionamento | Touch, teclado, senha privada, tentativas e canal USB de manutenção | Teste host de máscara/limites/limpeza passou; retenção de desenvolvimento é desprotegida em repouso |
| Hora/fuso | NTP, aplicação de TZ e catálogo de 462 entradas | Índice e políticas passaram; retenção/deriva RTC e hora hostil não qualificadas |
| Dados de produto | Clima/previsão, Bitcoin e detalhes/histórico, altcoins, PTAX, Fear & Greed e índices | Parsers e scheduler passaram; há falhas DNS registradas em endpoints e política de idade a consolidar |
| Offline | Snapshot binário, migração de versões, CRC, duas gerações, cache e throttle | Codec e integridade passaram; cortes/corrupção/cheio foram aprovados no escopo histórico da unidade |
| Interface | Boot, Home, Clima, Mercado, Dispositivos, Perfil, Preferências, Wi-Fi, Fuso, Sistema, display/som e notificações | Navigation Manager, construção lazy e preservação de quadro implementados; campanha prolongada aberta |
| Identidade/preferências | Nome, avatar, tela inicial, brilho, volume, modo noturno e preferências persistidas | Políticas portáveis e testes de serviços passaram; validação física das combinações atuais falta |
| Pomodoro | Timer monotônico local, presets, pausa/reset, estatísticas diárias e solicitação de som | Teste portável passou; qualificação física do áudio/concorrência e política após reboot faltam |
| Notificações | Preferências persistidas, fontes locais e histórico de oito itens com leitura | Teste do serviço passou; histórico é volátil e integração com OTA ainda futura |
| eWeLink | Login/fetch sob demanda no executor HTTPS, reconciliação e inventário NVS A/B | Login e fetch de três devices registrados; sucesso completo de persistência/reboot na imagem final ainda precisa de evidência |
| Sonoff LAN | mDNS, estado criptografado e comandos por canal, até oito devices/três canais | Três devices online/estados registrados; relés e retorno visual ainda aguardavam confirmação específica |
| Câmeras | Descoberta ONVIF/manual, autenticação SOAP/Digest e RTSP | Tapo C200 chegou a PLAY; vídeo Main ainda é rejeitado pelo decoder em uso |
| OTA P4 | Manifesto canônico, RSA-PSS, keyring, hash streaming, anti-replay, journal, writer e supervisor | Testes passaram; fluxo de laboratório opt-in, sem três ciclos físicos aplicar/reverter |

Agenda, Alarmes e o Timer do catálogo visual são referências aspiracionais:
não têm serviços funcionais e não constam da lista de telas compiladas no
`main/CMakeLists.txt`. O Pomodoro compilado é uma implementação separada.
Não existe entrega de calendário, alarmes gerais, Matter/BLE ou automações
de casa apenas porque existem mockups ou o BSP suporta periféricos.

O C direto em `ui/core` e `ui/screens` é a UI compilada. Os XMLs/exportações
em `ui/lvgl` e as referências de design não são automaticamente a fonte do
binário atual. O decoder `edge264` tem componente e compilação experimental,
mas não há chamadas `edge264_*` no código de aplicação: o stream continua
usando `esp_h264_dec_sw`. Portanto, adicionar essa biblioteca ainda não
resolveu a incompatibilidade da C200.

## Situação dos gates

| Gate | Situação real | O que ainda impede aceite completo |
|---|---|---|
| G0 | Base P4 reproduzida; reprodução C6 documentada | Consolidar recipe de ambiente, pacote imutável, BOM e identificação do C6 instalado; P4 sem modo determinístico |
| G1 | Boot/display mínimo aprovado no escopo da unidade | 100 boots por unidade, medição de primeiro frame e orçamento atual de recursos |
| G2 | Render/flash histórico aprovado com XiP | Repetir UI máxima atual, TLS/SD/IoT, p95/p99, GC, margens de pilha e zero artefatos |
| G3 | Rede aprovada com exclusões | Fault injection atual, rádio físico travado/ausente, novos consumers e prazos eWeLink |
| G4 | Cache/offline histórico aprovado | Sincronizar contratos v7, reparar teste cheio e validar registros novos de perfil/inventário com cortes |
| G5 | Em andamento | Aplicar/confirmar/reverter P4 por HTTPS, cortes e recuperação; implementar e validar transação C6 |
| G6 | Aberto/bloqueado para aceite | Soak 24/72/168 h instrumentado, múltiplas unidades, gabinete, energia, térmica e fault injection |
| G7 | Aberto/bloqueado | Segurança nos dois chips, fábrica, chaves, assistência, SBOM e rollout |

O registro de 13/09 já comprova recipe C6 com dois builds iguais, mas o
roadmap ainda contém a exclusão antiga dizendo que ela não existe. Atualizar
o estado documental, mantendo o histórico. Isso não fecha recovery do C6:
o exemplo/recipe continua sem rollback autônomo qualificado.

## Achados prioritários

### A1 — P1: falha anterior ao HTTPS pode deixar eWeLink ocupado indefinidamente

Fontes: [np_ewelink.c](../firmware/main/np_ewelink.c#L221),
[network_validation_service.c](../firmware/main/network_validation_service.c#L1068).

`np_ewelink_sync_begin()` guarda usuário/senha na mailbox e marca `busy`.
Depois de aceitar o pedido, a task de rede só chama
`np_ewelink_service_process_sync()` se DNS, NTP e orçamento restante passarem.
Se qualquer precondição falhar, ela publica apenas o resultado de rede. Não
há finalização eWeLink nesse ramo: `busy` e credenciais pendentes permanecem,
e uma nova sincronização é recusada. Basta a rede falhar entre o aceite e a
execução, mesmo com IP válido no momento do clique.

Correção necessária: completar/cancelar o pedido em todos os ramos, limpar a
mailbox privada e expor falha recuperável. Testar DNS/NTP falhos, perda de AP,
deadline e nova tentativa sem reboot. Achado por inspeção, sem reprodução HIL.

### A2 — P1: limite e timeout eWeLink não interrompem o consumo HTTP

Fonte: [np_ewelink_cloud.c](../firmware/main/np_ewelink_cloud.c#L30).

O callback deixa de copiar após 48 KiB, marca overflow e retorna erro, mas
`http_request()` usa `esp_http_client_perform()` e verifica overflow só depois
do retorno. No fonte local IDF 5.5.4, `http_on_body()` ignora o retorno de
`http_dispatch_event()` e `perform()` continua consumindo o corpo por chunks.
Assim, o armazenamento do corpo é limitado, mas o tráfego e a duração não
são abortados ao ultrapassar o teto.

O timeout de 10 s é de operação, sem deadline monotônico da transação. O modo
eWeLink também foi excluído da verificação global de 20 s. Um servidor que
envia dados continuamente pode ocupar o único executor e atrasar os demais
domínios. É o mesmo tipo de problema que motivou a remediação HTTP em setembro,
agora num cliente novo.

Correção necessária: streaming com orçamento total, teto por bytes aplicado
antes de continuar leitura, fechamento efetivo, redirects limitados e ensaio
de servidor lento/corpo excedido. O cancelamento deve liberar o executor.

### A3 — P1: intenção Sonoff aceita pelo EventBus pode não ter conclusão para a UI

Fontes: [product_ui.c](../firmware/main/ui/screens/product_ui.c#L2941),
[app_state.c](../firmware/main/app_state.c#L664),
[np_devices.c](../firmware/main/ui/screens/np_devices.c#L848).

Após publicar a intenção, a UI mostra `Ligando...`/`Desligando...` e bloqueia
novos comandos. Se o serviço recusar posteriormente — por exemplo, o device
ficou offline entre clique e consumo — o `app_loop` apenas registra warning.
Não publica completion correspondente. O desbloqueio depende de observar
pending do serviço, mudança do resultado ou confirmação do estado desejado.
Quando nenhuma dessas condições ocorre, não existe deadline local que encerre
a espera. Um pending visual pode bloquear todos os canais até reconstruir a
tela ou alguma mudança posterior.

Correção necessária: command ID, ACK/resultado de recusa e timeout; testar
perda de device, busy, falha repetida com o mesmo código, resposta rápida entre
projeções e retorno à tela. Achado por inspeção, sem novo acionamento físico.

### A4 — P1 para qualificação: orçamento de memória precisa ser refeito

A evidência de 03/10 registra 68.987 B internos livres durante falha eWeLink,
abaixo do piso de 80 KiB do guia, com fragmentação e falha AES-DMA. Há também
falha SDIO por alocação DMA, estouros de pilha e WDTs no histórico recente.
Mudar buffers para PSRAM, adicionar pool LVGL de 128 KiB, usar AES em software
e reduzir RX TLS para 8 KiB permitiu novos boots, mas não comprova que a carga
final voltou a cumprir o orçamento.

Medição complementar no host dos tipos atuais: snapshot 488 B, evento 616 B,
fila de 32 eventos com 19.712 B só de payload, projeção UI 4.464 B e
inventário privado eWeLink 12.164 B. A projeção inclui ponteiro e pode ter
layout diferente no P4; estes valores não substituem ELF/medição da placa.
O plano propunha eventos de até 128 B e orçamento conjunto de estado/filas de
24 KiB; o crescimento atual exige atualização e aferição explícitas.

Não há admissão baseada em heap/maior bloco antes das requisições normais
revisadas. O JSON eWeLink usa árvore cJSON com alocador padrão, sem arena
limitada ou garantia explícita de SRAM interna. Zeroizar o corpo/token local
não demonstra limpeza das cópias alocadas pela árvore JSON.

Necessário: medir piso, maior bloco interno/DMA, todas as pilhas, custo do
parser, mil navegações, dez mil requests e carga combinada; preservar os três
FB. Testar endpoints que enviam TLS records acima de 8 KiB antes de aceitar a
configuração menor como perfil de produto.

### A5 — P2: quatro testes estão quebrados/desatualizados

- `run_cache_full_probe_host_test.ps1`: compilação falha por tipos/constantes
  ausentes no recorte gerado (`onboarding_profile_t` e limites de API keys).
- `run_profile_ui_host_test.ps1`: link falha por `np_btc_tilted_icon`.
- `run_settings_timezone_ui_host_test.ps1`: mesma dependência ausente no link.
- `weather_asset_catalog_host_test.c`: assertion espera 500×462; cabeçalho
  atual define 560×376.

As falhas não demonstram defeito equivalente no firmware, que compilou; elas
demonstram que essas verificações não validam hoje o comportamento alegado.
Reparar os harnesses preservando a finalidade dos testes, sem apenas remover
assertions ou considerar a suíte verde por seleção parcial.

### A6 — P2: contratos compartilhados ficaram atrás do firmware

O firmware usa schema **v7** e payload codificado de 364 B; `shared/schemas`,
`shared/examples` e seus READMEs chegam somente à **v4**. A documentação ainda
afirma que Ibovespa está indisponível. O protocolo promete descarte de
request/geração de rede antigos, mas o evento atual carrega só o snapshot e
não transporta esses identificadores.

Sincronizar schemas v5–v7, exemplos, limites e migrações. Implementar a
validação de request/geração ou revisar explicitamente o contrato: sem esses
campos não se deve afirmar que resultados antigos são descartados por geração.

### A7 — P2: política de frescor e evidência precisam de consolidação

Índices têm refresh de 24 h no scheduler, mas são classificados stale após
30 min no `app_state`. Isso pode ser uma política válida, porém a UX precisa
explicar a frequência de consulta e a idade, sem expectativa de atualização
constante. A localização de clima permanece fixa em Brasília no worker;
definir se o produto terá escolha de localidade, separada da identidade/fuso.

README/roadmap e auditoria de 12/09 misturam fotografias antigas com estado
atual. Criar inventário de validações atuais por funcionalidade e firmware.
Os registros recentes repetem `commit + alterações locais`; anexar diff/hash
de fonte ou commit completo para tornar os binários reproduzíveis.

## Verificação executada nesta auditoria

Além dos achados, há diferenças entre a arquitetura proposta e a entrega
atual. O firmware é modular em C, com políticas/modelos portáveis, e preserva
o padrão central de estado/UI; não é necessário reescrevê-lo em C++ para
reconhecer essas entregas. Entretanto, as portas injetadas, resultados de
comando com ID/ACK, executor com fila/prioridades/cancelamento e políticas de
circuit breaker por origem do plano ainda não estão completos. A task de rede
atual combina diagnóstico, agendamento, aquisição, eWeLink e OTA, com um
pedido explícito pendente. Sua serialização é útil, mas não entrega todas as
garantias do RequestOrchestrator proposto.

O supervisor de saúde revisado é o confirmador de boot OTA; não comprova um
supervisor contínuo de todos os serviços durante a operação normal. Logs
ESP_LOG e contadores locais também não equivalem ao ring buffer estruturado,
histogramas e exportação controlada previstos. Retenção/qualidade temporal
com estados RTC holdover/suspect e caracterização de deriva continuam
pendências. Essas diferenças precisam virar requisitos rastreados, com
prioridade por risco, em vez de serem tratadas como implementação implícita
do plano.

### Build P4

Build novo em `firmware/build/audit-20261004`, com configuração isolada,
`IDF_TARGET=esp32p4` e ESP-IDF 5.5.4. A primeira tentativa de exportação usou
um venv inexistente; o ambiente foi ajustado para o Python instalado. A
escrita em `build/` exigiu autorização do sandbox. O build final terminou
com código zero; não foi executado flash.

| Artefato | Resultado |
|---|---|
| App | `0x34b1f0` B = 3.453.424 B, aproximadamente 3,29 MiB |
| Menor slot OTA | `0x800000` B = 8 MiB |
| Folga | `0x4b4e10` B, 59% reportados pelo verificador IDF |
| SHA-256 app | `AF1DFAC8F38A5264AC9BC0E1F1CD36DBB263162AA835B67C12AE09A746FCDA05` |
| SHA-256 ELF | `F5E43FDE94930B4B2848111F033B2614EDD1C15D58EC5B96C181DBA30AD3C6F0` |
| SHA-256 sdkconfig | `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8` |
| SHA-256 tabela | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |

Configuração efetiva conferida: P4/revisão mínima 100, flash 32 MiB, RGB565,
três FB, alinhamentos 64 B, XiP, reset54 ativo baixo, SDIO 18/19/14–17,
rollback P4, TLS interno, RX TLS 8 KiB, AES hardware desligado, Hosted DMA
preferindo PSRAM e expansão LVGL de 128 KiB. Auto-suspend, ESP_HOST_WIFI,
Secure Boot, Flash Encryption e NVS Encryption permanecem desligados.
Retenção Wi-Fi de desenvolvimento foi compilada ligada.

Não foram identificados warnings de compilação C/C++ na busca do log. Houve
warning de geração do `gdbinit` por ausência de `ESP_ROM_ELF_DIR` no ambiente
de auditoria; isso limita depuração ROM e não impediu o binário. Log completo:
`firmware/build/audit-20261004-build.log` (ignorado pelo Git).

### Testes

| Grupo | Resultado novo |
|---|---|
| 15 runners PowerShell `run_*host_test.ps1` | 12 PASS; cache cheio, Perfil e Fuso falharam |
| 9 executáveis C adicionais | 8 PASS; catálogo de assets falhou |
| Recipe C6 | 11 casos Python PASS |
| Gerador de configuração OTA | 3 casos Python PASS |
| Supervisor de boot OTA | 7 cenários determinísticos PASS |
| Vetor RSA-PSS | Assinatura válida aceita e alteração rejeitada |
| Vetor SHA-256 | Hash por chunks PASS |
| Índice de fusos | 462 entradas estáveis e 427 mapeamentos PASS |
| `git diff --check` | Sem erros de whitespace; avisos de normalização LF/CRLF |

Os oito executáveis adicionais aprovados foram cache record, scheduler,
codec offline, parser de providers, formatação de valores, Pomodoro, perfil
e condição meteorológica. Compilados com GCC, `-O2 -Wall -Wextra -Werror
-pedantic`. Os testes Wi-Fi passaram paginação, seleção, máscara e 100 ciclos
de lifecycle em LVGL host. Isso não equivale a 100 ciclos de navegação na placa.

Os scripts de vetores criptográficos usam OpenSSL/.NET para validar vetores;
não executam o verificador MbedTLS do firmware inteiro. Não foram realizados
fuzz/sanitizers, HIL novo nem build C6 novo. Os 25 resultados PASS contam
execuções de verificações, não quantidade total de assertions ou casos.

## Trabalho que falta, em ordem de execução

1. **Congelar uma fotografia reproduzível.** Revisar e versionar as mudanças
   locais em commits pequenos; incluir todos os novos fontes e a procedência
   do decoder. Consolidar docs atuais, recipe do ambiente e identificação do
   par P4/C6. O manifesto P4 aceita uma faixa 5.5.x embora o baseline seja
   5.5.4: impor/verificar a versão exata na entrada de build/CI.
2. **Fechar defeitos de conclusão e limites.** Corrigir A1–A3, reparar os quatro
   testes e sincronizar o contrato v7. Criar testes negativos dos novos fluxos
   eWeLink/Sonoff/notificações, hoje sem cobertura dedicada equivalente.
3. **Revalidar o produto atual na placa.** Perfil/tela inicial, swipes, teclado,
   modais, Dispositivos, importação cloud, reboot com inventário, relés TX1C/
   TX2C/TX3C e perdas de rede durante cada operação. Repetir navegação além de
   19 minutos e 100 ciclos, depois a campanha maior; atribuir ou eliminar o
   WDT `np2_netcheck`/IDLE1 registrado em 03/10. Medir memória e render da carga
   combinada e não promover pelos boots curtos posteriores.
4. **Resolver o vídeo da C200 como incremento separado.** Integrar ou escolher
   decoder compatível com Main; medir FPS, atraso, memória, banda e parada/
   reconexão. Exercitar streams malformados, limites de resolução e consumo
   junto de TLS/UI. Persistir endereço/inventário ONVIF por política explícita:
   hoje o IP manual é volátil. Não declarar vídeo entregue enquanto houver
   somente autenticação/PLAY.
5. **Fechar G5 P4.** Disponibilizar origem HTTPS de laboratório e pacote
   assinado, executar preflight/apply, verificar PENDING_VERIFY→VALID, crash/
   rollback, assinatura inválida/truncamento/replay, cortes em download,
   escrita, seleção e primeiro boot; completar três ciclos aplicar/reverter.
6. **Fechar G5 C6/conjunto.** Inventariar layout/bootloader realmente instalados,
   provar fallback autônomo do rádio, implementar staging verificado e Slave
   OTA via SDIO, journal de pares e compatibilidade dos estados intermediários.
   Ensaiar cortes em begin/write/end/activate/reboot e retorno P4 com C6 novo.
   Reserva de partição e recipe de build não entregam essa transação.
7. **Qualificar G6.** Campanhas 24/72/168 h com logs e métricas contínuos,
   gabinete/fonte/cabo finais, temperatura, brownout, ≥5 unidades, cortes,
   heap/pilhas, requests e navegação. SD+Wi-Fi precisam ser qualificados ou
   excluídos da release; timeouts de montagem do SD continuam no histórico
   de 04/10, embora o fallback estático permita seguir operando.
8. **Preparar G7.** Credenciais e devicekeys protegidas em repouso nos chips,
   NVS/Flash Encryption, Secure Boot/anti-rollback, dumps e manutenção,
   rotação/revogação e serviço de assinatura, SBOM, fixture de fábrica,
   golden package, assistência e canário. eFuses continuam dependendo de
   autorização humana explícita em amostras dedicadas.

## Limites e riscos de produção

A retenção Wi-Fi local está ligada por padrão no desenvolvimento e o
inventário eWeLink persiste devicekeys na NVS sem criptografia ativa. Não
exibir chaves em AppState/logs é uma proteção diferente de impedir extração
física. O código atual usa macros de provider com valores vazios por padrão;
o comentário sobre chave hardcoded não comprova segredo embutido na árvore
atual. Atualizar o comentário e manter injeção/armazenamento privado de chaves
fora do Git. A validação presente não constitui auditoria forense de todo o
histórico, arquivos binários ou dumps.

O adapter LVGL é um override local versionado, com hooks de ciclo. Há ajuste
de espera mínima em ticks na composição. A procedência do upstream está
documentada; auditar o delta a cada atualização. `edge264` encontra `clang`
por PATH, sem verificar versão no CMake, portanto a versão real do compilador
também precisa entrar no pacote reproduzível. O P4 está com
`CONFIG_APP_REPRODUCIBLE_BUILD` desligado; hashes diferentes de builds não
devem ser interpretados automaticamente como mudança funcional.

Não foi encontrado pipeline CI versionado em `.github`. Os runners dependem
de toolchains/caminhos locais e não há execução agregada que impeça promover
um commit com verificações quebradas. Padronizar um comando de validação e
uma CI ajuda a manter as garantias já construídas. `firmware/main.zip` é um
arquivo rastreado de backup: revisar utilidade/procedência e eventual remoção
posterior; esta auditoria não apagou nem examinou seu conteúdo como fonte
compilada.

O avanço mais útil agora é fechar os caminhos de falha e a qualificação da
fotografia atual, com OTA recuperável como próximo gate estrutural. Novas
telas podem aumentar a superfície funcional antes de a base atual estar
comprovada sob uso contínuo.

## Atualização posterior — prioridade de refresh e primeira redução de SRAM

Após a auditoria, o operador priorizou o Bitcoin amarelo e a atualização dos
dados. A investigação reproduziu timeout TLS/HTTP com scheduler, entrega e
projeção ainda funcionando. Corrigiu-se a configuração AES efetiva divergente
e adicionou-se um gate de build/diagnóstico por etapa (ADR-087). AES por
software também reproduziu falha, portanto a causa não foi encerrada.

Na primeira melhoria de memória (ADR-088), o EventBus conservou as 32 vagas
e adotou item privado de 56 B com quatro snapshots de propriedade do bus.
Fila + pool passaram de 19.712 B para 3.776 B no P4: economia de 15.936 B.
O worker mantém/reenvia o dado mais recente após rejeição. O novo teste host
do código real passou, incluindo saturação, reciclagem, concorrência e reenvio.
Build P4 limpo e gravação COM8 passaram; boot e renovações BTC foram registrados.

Os tamanhos e o resumo de 29 verificações anteriores descrevem a fotografia
inicial, antes dessas alterações. Esta validação posterior foi dirigida ao
EventBus/refresh e não recompõe a auditoria completa nem resolve os quatro
checks que já estavam falhando. A margem de memória, recuperação após
timeout e funcionamento por horas continuam pendentes. Evidência detalhada:
[diagnóstico de refresh](DATA-REFRESH-DIAGNOSIS.md).
