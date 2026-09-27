# G5 — Atualização recuperável: execução e evidência

Iniciada em 2026-09-12 após o fechamento G4 com escopo de bancada. Este
documento não fecha G5 nem promove XiP ao baseline. Fontes de verdade:
`ROADMAP.md`, seção 10 do plano premium e o guia de bring-up.

## Primeiro incremento e limites

`firmware/main/update_policy.c` contém comparação tipada de metadados P4 e
política de saúde do primeiro boot. Não usa heap, I/O, IDF ou LVGL, não cria
tasks, filas, eventos ou conexões e não altera o comportamento de boot atual.
Os structs são contratos internos de tamanho fixo, sem ponteiros externos ou
segredos; não são formato persistido nem formato de rede.

`update_metadata_check` verifica identidade local não nula, target P4,
intervalo de revisão, tamanho completo da app incluindo assinatura/padding,
schema atual legível, security version inalterada, app atual confirmado,
transação ociosa e C6 fixo 3.0.6/RPC v2/SW_AGGR. Resultado `MATCH` não verifica
assinatura, hash, imagem real, versão de release/replay ou compatibilidade de
bootloader; não pode autorizar escrita/ativação sozinho. Pacotes que mudam C6,
bootloader ou partições são recusados. IDs de produto/PCB, formato canônico,
registro de chaves e política de versão serão definidos no incremento de
manifesto; os IDs dos testes são fictícios.

`update_boot_policy_poll` modela os 15 s contínuos e deadline de 60 s usando
milissegundos monotônicos. Progresso real da UI/app deve ser fornecido pelo
adapter futuro, com observação a cada 250 ms e intervalo máximo de 500 ms.
Sem AP/C6/NTP/Internet, a saúde local pode confirmar normalmente. Cache
indisponível com defaults e setup utilizáveis deve ser avaliado como estado
local degradado permitido, não confundido automaticamente com app defeituosa.
O consumidor deverá tratar erro de confirmação e verificar novamente a
existência do fallback antes da API de rollback. Sem alternativa, entrar em
recuperação local sem reinícios repetidos.

## Sequência de implementação

| Incremento | Entrega e dono | Condição de avanço |
|---|---|---|
| 1 — políticas | Domínio: metadados e saúde local, testes host | Primeiro corte implementado; build/evidência registrados separadamente. |
| 2 — reprodução C6 e inventário | Plataforma/build: recipe fixada, patch oficial idempotente, lock, hashes, layout/bootloader C6 instalado | Resolver exclusão G0 antes da transação conjunta; build novo não identifica automaticamente o binário instalado. |
| 3 — manifesto e autenticação | Atualização: bytes canônicos ou exatos com assinatura destacada, RSA-PSS, key ID público, SHA-256, headers e limites | Assinatura boa/ruim, chave desconhecida, alteração de campo/byte, duplicatas, truncamento, replay e excesso; chaves privadas fora do Git. |
| 4 — saúde local integrada | Plataforma/app: progresso real e estado do slot; app_loop publica resultado sanitizado; FlashCoordinator escreve otadata | Boot offline confirma em 15 s; UI/app sem progresso recusa; deadline/fallback/erro de escrita ensaiados. |
| 5 — OTA somente P4 | Executor HTTPS existente + FlashCoordinator: streaming no slot inativo, manutenção, journal e ativação após verificação completa | Mesma linha Hosted/C6 e schemas compatíveis no atual/candidato/fallback; três ciclos assinados aplicar/reverter P4. |
| 6 — OTA conjunta | UpdateCoordinator: staging C6, begin/write/end/activate por SDIO e reconciliação | Rollback C6 autônomo e matriz de pares comprovados; três ciclos conjuntos e cortes. |

Integração de saúde deve anteceder a primeira seleção OTA. Criar supervisor
antes do bring-up para que falha/travamento inicial não elimine o deadline.
Escrita OTA usa chunks limitados (ponto inicial 4 KiB), nunca a imagem inteira
em RAM; tamanho assinado deve caber nos slots reais. O executor de rede atual
mantém exclusividade HTTPS também na manutenção. Nenhum callback de toque
grava flash ou chama a API OTA. O journal guarda transições significativas,
nunca progresso de cada chunk.

### Resultado do incremento 2 — 2026-09-13

O projeto `coprocessor/` agora reproduz o bootstrap do exemplo oficial em uma
recipe local mínima. `dependencies.lock` fixa Hosted 3.0.6 no hash
`1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879` e IDF
5.5.4. `tools/audit_c6_idf_patch.py` aceita somente o arquivo completo
`sdio_slave.c` da tag 5.5.4, com SHA-256
`32041dcbbd0e1f4db26af901c68a3e0804b5d01142b291314c5016ca0ea0a6aa`, e
recusa qualquer modificação fora da guarda SDIO documentada. O patch oficial
foi chamado duas vezes no IDF instalado e ficou byte-idêntico entre chamadas.

Os builds limpos `c6-repro-det-a-20260913` e `c6-repro-det-b-20260913`, em
diretórios diferentes, produziram os mesmos hashes de app, bootloader, tabela,
configuração e lock. App `0x10e8d0` (1.108.176 B), SHA-256
`cfedfc093a1bc28e70042e659d3c5274aa27fefd125c429b014e5b9ac76cb136`;
bootloader SHA-256 `744aa05020f479aa2906f2d2d4da5f21d6b281bb040529da8441b60641b6dd5e`;
tabela SHA-256 `73e7f5c6f83081380dbc044da3da17e97b4259524d6abf772f7905ee2f54a57d`.
O slot C6 é `0x1c0000`, com folga `0xb1730` (726.832 B); o staging P4 de 2 MiB
tem folga de 988.976 B. A tabela preservou NVS, otadata, phy e dois slots OTA
de 1.835.008 B. A configuração efetiva confirmou C6, Hosted, RPC v2, Wi-Fi e
SDIO SW_AGGR; Secure Boot, Flash Encryption, anti-rollback, rollback C6,
Bluetooth e auto-suspend ficaram desabilitados.

Os testes negativos de patch, Kconfig, tabela e comparação passaram (11 casos).
Não houve flash: a COM8 é do P4, e bootloader/layout instalado no C6, recuperação
autônoma e recovery por Slave OTA ainda não foram demonstrados. O binário
compilado não é identificado como o binário hoje no rádio. Esta entrega fecha
apenas a reprodução de software; o incremento 2 continua aberto para
inventário físico antes de OTA conjunta.

### Inventário C6 disponível pelo P4 — 2026-09-13

O boot físico do P4 confirmou o C6 presente como `esp32c6`, Hosted 3.0.6,
RPC v2 e SDIO SW_AGGR. A API pública `esp_hosted_get_coprocessor_app_desc()`
do Hosted 3.0.6 foi auditada antes de integrá-la: no host ela reaproveita
`Req_GetCoprocessorFwVersion`, preenchendo somente `version` e deixando
projeto, IDF e SHA-256 vazios. O RPC `Req_AppGetDesc` completo existe no
handler C6, mas não é exposto pelo host público desta versão.

Não foi adicionado acesso a headers privados nem alterado o componente travado
para contornar essa lacuna. Mesmo a resposta completa não informaria tabela de
partições ou bootloader. Portanto, ela não provaria o layout A/B real, a
recuperação autônoma ou a identidade hash da app C6 instalada. Inventário C6
para OTA conjunta continua bloqueado até procedimento físico/recovery que
observe esses estados sem rota USB/UART para o C6.

### Resultado parcial do incremento 3 — manifesto canônico P4 — 2026-09-13

`firmware/main/update_manifest.c` introduz um envelope binário de 104 bytes,
versão 1, de tamanho fixo e little-endian. Ele inclui ID não nulo, produto,
PCB, revisão, versão de app, security version, tamanho completo, schemas,
perfil Hosted C6 e SHA-256 da app. Todos os bytes reservados devem ser zero;
não há JSON, campos repetidos, normalização ou serialização posterior. O
validador expõe exatamente os bytes recebidos para a futura assinatura
destacada, que deverá verificar RSA-PSS antes de chamar qualquer operação de
download ou flash.

O formato recusa destino C6 e qualquer flag de troca de C6, bootloader ou
tabela. Ele também não realiza verificação criptográfica, de hash de imagem
ou I/O: `OK` significa somente que o envelope canônico está bem formado. A
assinatura, chave pública/key ID, SHA-256 da imagem recebida e vinculação com
`update_metadata_check` são tratados nos subincrementos 3b/3c; ainda não há
autorização de escrita/ativação OTA.

### Resultado parcial do incremento 3b — RSA-PSS destacada — 2026-09-13

`firmware/main/update_signature.c` verifica a assinatura destacada somente
após o parser canônico aceitar o manifesto. Exige envelope de 388 bytes
(`key_id` little-endian e assinatura de 384 B), chave pública
SubjectPublicKeyInfo em DER, RSA exatamente 3072 bits, SHA-256, MGF1 SHA-256 e
salt de 32 B. O adapter fornece a chave pública confiável pelo `key_id`; o
firmware não contém chave privada, chave de produção ou cópia persistida da
chave. Chave sem suporte, key ID desconhecido, manifesto inválido ou assinatura
alterada falham fechados.

O vetor independente foi gerado com chave efêmera fora do repositório e
validado pelo OpenSSL: a assinatura válida foi aceita e a mudança de um byte
foi recusada. A chave privada temporária foi removida depois do teste. A
compilação P4 aceitou a API MbedTLS disponível no IDF 5.5.4; não foi necessária
alteração de Kconfig, dependência ou lock. Ainda falta um keyring de produção
com rotação/revogação, a verificação do SHA-256 calculado no stream e a
vinculação do resultado à política/flash. Este corte não baixa, grava, ativa
ou confirma nenhuma imagem.

### Resultado parcial do incremento 3c — hash streaming da imagem — 2026-09-13

`firmware/main/update_image_hash.c` define uma sessão incremental SHA-256 sem
I/O nem alocação da imagem inteira. Após `begin`, ela aceita somente blocos de
até 4 KiB, contabiliza rigorosamente o tamanho declarado no manifesto e em
`finish` compara o digest com o SHA-256 assinado. Bloco nulo, bloco vazio,
sessão fora de estado, excesso, tamanho final divergente, falha criptográfica
ou digest divergente encerram a sessão sem produzir autorização de flash.

`tools/run_update_image_hash_vector_test.ps1` calculou o vetor fixo em blocos
de 7 B e obteve `2f283ef73d59a91a41236708b6764fe0058e6c18c8cb4bff48165650ba0e28e7`.
O build P4 compilou a sessão contra MbedTLS do IDF 5.5.4. Ainda falta conectá-la
ao único executor HTTPS e ao FlashCoordinator, de modo que nenhum byte é
baixado, gravado, ativado ou confirmado por este incremento.

### Resultado parcial do incremento 3d — journal e anti-replay persistíveis — 2026-09-13

`update_journal.c` modela `IDLE → P4_STAGED → P4_PENDING → ACCEPTED`, sela
cada estado por CRC e preserva o ID e a versão do manifesto já autenticado. A
geração não é controlada pelo chamador: o `FlashCoordinator` grava apenas
transições seladas de geração zero no namespace NVS `np2_update`, alternando
`ota0` e `ota1`, e atribui a próxima geração depois de selecionar a última
cópia válida. No boot, o status do coordenador expõe somente o journal de CRC
válido de maior geração. Apenas `ACCEPTED` pode produzir o registro usado pela
política anti-replay; staging e pending não bloqueiam uma release futura.

Os testes host cobrem CRC, transições inválidas, geração e a ponte para
anti-replay. O P4 compilou e foi gravado, mas não foi emitida uma transação de
journal na unidade: não há manifesto/chave/release de produção neste
repositório. Ainda faltam o executor HTTPS único, `esp_ota_*`, reconciliação
do slot real e os ciclos físicos assinados. O C6 permanece bloqueado pelos
gates de layout e recovery.

## Auditoria inicial do exemplo C6 fixado

Inspeção dos fontes locais resolvidos pelo lock P4 de ESP-Hosted 3.0.6,
hash de componente `1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879`:

- `examples/ota/coprocessor_ota/cp/main/idf_component.yml` usa Hosted `*`
  e IDF `>=5.5`. A recipe NP2 precisa fixar 3.0.6/5.5.4 e versionar o lock
  C6; executar o exemplo sem essa correção pode resolver outra versão.
- `cp/sdkconfig.defaults` seleciona flash 4 MiB, RPC v2 e tabela
  `partitions_eh_cp_ota_4m.csv`. A recipe deve carregar esse default comum,
  depois `sdkconfig.defaults.esp32c6` e por último o perfil NP2; verificar
  o resultado efetivo em vez de inferir que todas as opções foram herdadas.
- A tabela do exemplo contém `ota_0` em `0x10000` e `ota_1` em `0x1D0000`,
  cada slot com `0x1C0000` (1.835.008 B). Esse limite é menor que os 2 MiB
  de staging P4. Ambos devem ser conferidos incluindo padding/assinatura.
  A tabela fonte não prova o layout atualmente gravado no rádio da unidade.
- Não foi encontrada configuração de rollback nem chamada de confirmação de
  app no exemplo/componente pesquisado. O `cp/main/main.c` inicia NVS/event
  loop e usa erase de NVS em dois erros. Antes de portar a recipe, revisar
  saúde autônoma, identidade e recuperação; não declarar rollback C6 existente.
- O default do exemplo permite NVS Wi-Fi. A auditoria deve provar o uso
  RAM-only no rádio e seus estados anteriores ao comando do P4.

Nenhuma imagem C6 foi compilada, transmitida ou ativada neste incremento.
Capturar patch/diff/hash por comparação com o IDF 5.5.4 original, inclusive
quando a instalação local não dispõe de histórico Git. Aplicar somente o
utilitário oficial idempotente. Não corrigir fontes instalados manualmente.

## Matriz de bancada obrigatória

Preparar fallback compatível antes de selecionar candidato. O `idf.py flash`
usual grava também `otadata`; não usá-lo como substituto de aplicar/reverter
OTA, pois modifica os marcadores cujo comportamento se deseja observar.
Usar o mecanismo de atualização implementado, capturar estado real dos slots
e manter os artefatos exatos de recuperação.

| Ensaio | Critério de aceite |
|---|---|
| Assinatura/hash/target/revisão/schema/tamanho inválidos | Recusa; nenhum slot parcial selecionado; UI local operável. |
| Manifesto ambíguo, repetido, truncado ou chave desconhecida | Falha fechada antes da ativação, sem retry rápido ou segredo em logs. |
| P4 primeiro boot sem rede | Confirmação após 15 s contínuos de saúde local; estado persistido verificado. |
| Falha inicial de display/serviços ou UI/app parados | Sem confirmação; em até 60 s, fallback bootável ou modo seguro sem loop. |
| Crash/corte antes da confirmação e falha de escrita otadata | Retorno ao app anterior ou recuperação comprovada; não relatar aceitação por resultado em RAM. |
| Sem slot anterior válido | Recuperação local, sem sequência infinita de reinícios. |
| Corte durante download/erase/write/seleção P4 | Slot ativo preservado; download parcial nunca selecionado. |
| Falha tardia depois de VALID | Retorno explícito para par compatível; não presumir rollback automático. |
| Corte em staging/journal e journal corrompido | Reconciliar slots/hashes/versão reais; recusar decisão baseada só no marcador. |
| Corte em begin/write/end/activate/reboot C6 | Recuperação autônoma do rádio e SDIO restaurado, sem rota UART C6. |
| Par incompatível, C6 sem resposta ou sem end válido | Não avançar P4 nem ativar C6; preservar UI offline. |
| OTA sob XiP/render e pausa/retomada da manutenção | Display/touch/memória dentro dos gates; medir antes de promover XiP. |
| Três ciclos reais assinados aplicar/reverter | Hashes/estados de ambos chips e fallback comprovados em cada ciclo. |

Journal previsto: `IDLE → STAGED → C6_TRANSFER → C6_VERIFIED → P4_STAGED →
P4_PENDING → ACCEPTED`. Caminho somente P4 omite transferência C6, mas confirma
compatibilidade do rádio antes da aceitação do pacote. A confirmação local
de boot e a aceitação conjunta são decisões distintas.

Para cada execução: data, unidade/BOM/revisão, commit com diff quando houver,
hash P4/C6 real, SDK/patch, defaults/configuração efetiva, slots/bootloader,
comandos, logs brutos, instantes de corte, memória e observação de UI/touch.
Artefatos/logs ficam ignorados; o registro sanitizado vai para
`BRINGUP-EVIDENCE.md`. eFuses, Secure Boot, Flash Encryption e anti-rollback
continuam fora deste início.

## Reprodução de software

```powershell
./tools/run_update_policy_host_test.ps1
# Em terminal ESP-IDF 5.5.4, a partir de firmware/, diretório novo:
idf.py -B build/g5-policy-20260912 -D SDKCONFIG=build/g5-policy-20260912/sdkconfig -D IDF_TARGET=esp32p4 build
idf.py -B build/g5-policy-20260912 size
```

O teste host cobre intervalos e limites de metadados, C6 incompatível,
operações não suportadas, imagem pendente/ocupada, 15 s contínuos, amostra
ausente, cada fonte local defeituosa, timestamps antigos/futuros, relógio
regressivo, limite 60 s e falta de fallback. Não testa assinatura ou flash,
pois esses adapters ainda não existem.

### Resultado parcial do incremento 3e — writer P4 e sequência persistida — 2026-09-13

`update_p4_writer.c` não possui mais chamadas diretas de `esp_ota_*`: pede
begin/write/end/abort/seleção ao `FlashCoordinator`, que é o único dono da
task serializada de flash. Os blocos de até 4 KiB participam do SHA-256
streaming antes de o coordenador os gravar no slot inativo. Falha de hash,
stream ou flash aborta a sessão. A seleção exige `esp_ota_end` bem-sucedido,
journal `P4_PENDING` e geração persistida idêntica à cópia NVS. O requisito
`app_update` é componente nativo do ESP-IDF 5.5.4, sem lock externo novo.

O journal agora rejeita cópias válidas com a mesma geração mas conteúdo
distinto, dados inválidos/corrompidos e estouro de geração. Para cada escrita
aceita somente a próxima transição: início em `P4_STAGED`, depois
`P4_PENDING`, `ACCEPTED` e `IDLE`; uma nova release só começa a partir do
`IDLE` persistido. Estados ativos exigem ID e versão de manifesto. O teste
host cobre essa sequência e a criação de uma nova release após `IDLE`.

Todos os testes host de manifesto, assinatura, hash, keyring, anti-replay,
política e journal passaram. O build P4 de 2026-09-13 produziu
`0x1771c0` B, com `0x688e40` B livres no menor slot OTA. O flash em COM8
verificou bootloader, app, tabela e `otadata`; a captura posterior confirmou
P4 v1.3, 32 MiB de flash/PSRAM, EK79007, GT911, dashboard local e Hosted C6
3.0.6 com RPC v2 e SW_AGGR. Esta operação não executou uma transação OTA nem
escreveu NVS de produto, `storage`, `c6_ota`, C6 ou eFuses.

Ainda não há URL permitida, manifesto e assinatura de produção, chave DER
pública, nem imagem candidata para submeter ao fluxo. Portanto esta evidência
não valida download HTTPS, escrita de slot por release, seleção efetiva,
rollback ou recuperação após corte de energia. O C6 continua bloqueado pelos
gates de layout e recovery.

### Resultado parcial do incremento 3h — executor HTTPS P4 de desenvolvimento — 2026-09-13

`network_validation_service_request_p4_update_apply` recebe uma solicitação
explícita de desenvolvimento: três endpoints HTTPS fixados no mesmo host,
keyring público imutável de até quatro chaves e o ambiente de compatibilidade
local. Ela não aceita chave privada, credencial ou URL persistida. O worker
único executa DNS/NTP, baixa manifesto (104 B) e assinatura (388 B), valida
assinatura, metadados e replay, persiste `P4_STAGED`, abre o slot inativo pelo
`FlashCoordinator` e baixa a imagem em blocos de 4 KiB. O `Content-Length`, o
tamanho assinado e SHA-256 precisam coincidir. Só então persiste
`P4_PENDING`, seleciona o slot e reinicia em `PENDING_VERIFY`.

O supervisor confirma saúde local por 15 s. Antes de marcar a imagem válida,
o coordenador sela `ACCEPTED`; após a confirmação grava `IDLE` preservando a
identidade aceita para anti-replay. O caminho de falha antes de `P4_PENDING`
aborta o slot e retorna o journal a `IDLE`; após `P4_PENDING` ele preserva a
intenção para recuperação. A implementação ainda não é prova de ciclo físico:
não há artefato assinado de desenvolvimento servido por HTTPS, Wi-Fi conectado
ou ensaio de corte/rollback. O C6 continua inteiramente fora deste fluxo.

Os nove testes host G5 passaram. O build ESP-IDF 5.5.4 para `esp32p4` em
`build/g5-keyring-20260913` produziu `0x17e740` B, com `0x6818c0` B (81%)
livres no menor slot, SHA-256
`DE3612A2C215F45E1B82DD29D688B6D1549C2086455779B957733FE16B1EAF3D`.
O flash obrigatório em COM8 foi tentado após o build, mas falhou com
`PermissionError(13): Acesso negado` porque a porta estava ocupada. Por isso
não há boot desta revisão nem alegação de gravação concluída.

O utilitário versionado `tools/create_p4_development_ota.py` reproduz o
conjunto de laboratório a partir de uma app P4: imagem candidata, manifesto,
envelope de assinatura e DER público. Em 2026-09-13 ele gerou o conjunto local
ignorado `artifacts/g5-development-20260913` com IDs de laboratório
`0x4e503250`/`0x50343742`, revisão 3, versão 1 e key ID 1. A assinatura
RSA-3072 PSS foi verificada pelo OpenSSL e o hash da candidata coincide com o
binário de build. A chave privada existe somente nesse diretório ignorado e
nunca é carregada pelo firmware.

### Resultado parcial do incremento 3f — confirmação local de boot P4 — 2026-09-13

`update_boot_supervisor` só é criado quando o slot em execução estiver em
`PENDING_VERIFY`. Ele mede o frame renderizado pelo LVGL, o heartbeat do
`app_loop` e o `FlashCoordinator`; depois de 15 s contínuos de saúde local,
enfileira a confirmação de `otadata` no coordenador. Ao atingir o limite de
60 s, enfileira rollback apenas se o outro slot estiver em estado `VALID`; sem
fallback comprovado entra em recuperação, sem reinício em loop. A API OTA não
é chamada por UI nem por callbacks LVGL.

O dashboard publica seu estado a cada 250 ms durante a janela de saúde para
produzir progresso de UI observável dentro de 500 ms. Esse aumento de taxa
precisa de medição de render durante um ciclo OTA real antes de ser promovido
como desempenho qualificado.

O teste host de política passou. O build P4 de 2026-09-13 gerou `0x178030` B,
com `0x687fd0` B livres no menor slot OTA; SHA-256
`D7145967C7B1407F9818E951E3D101F19EC98F0EFDC6A7DBBE86CF8DF9F39D59`.
O flash COM8 foi verificado pelo esptool. No boot posterior o P4 v1.3 iniciou
display, touch, dashboard e Hosted C6/RPC v2/SW_AGGR sem panic ou WDT. Como a
imagem atual está confirmada, o supervisor não foi iniciado e não houve
mudança de `otadata` por OTA. Confirmação, rollback e recuperação ainda
requerem ensaio físico com imagem assinada pendente.

Referência primária consultada: [OTA/rollback ESP-IDF 5.5.4 para
P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/ota.html).
Os tempos de saúde e demais critérios são políticas NP2; os resultados físicos
devem ser produzidos na unidade, não inferidos da documentação da API.

### Preparação do primeiro ciclo P4 — 2026-09-15

O acionamento de laboratório foi ligado à janela USB de manutenção já armada:
`OTA_STATUS`, `OTA_PREFLIGHT` e `OTA_APPLY`. O firmware não aceita URL, chave
ou credencial por serial. `configure_p4_development_ota.py` gera, fora do Git,
um header com host, três URLs HTTPS e DER público RSA-3072; se o header não é
passado à build, preflight e aplicação retornam `ESP_ERR_NOT_SUPPORTED`.
`create_p4_development_ota.py` cria `public/` contendo somente imagem,
manifesto e assinatura, mantendo a chave privada fora da raiz a servir.

O executor coleta novamente os fatos locais antes da admissão: revisão P4,
tamanho do slot inativo, security version, slot corrente `VALID` e versão C6
3.0.6. RPC v2/SW_AGGR são o perfil de transporte conhecido, não atestação de
hash, bootloader ou layout do C6. Uma correção necessária resealou o journal
com a geração devolvida pela NVS antes da seleção do slot; alterar a geração
sem novo CRC bloqueava a ativação. A escrita P4 usa erase incremental e continua
sob o `FlashCoordinator`.

O supervisor e o coordenador agora iniciam antes do bring-up gráfico. Saúde
exige primeiro frame e sinais do `app_loop`; um timer na própria task LVGL
mantém o heartbeat de tela estática sem forçar redraw. O supervisor só trata
confirmação como concluída depois de observar `otadata`; ao falhar, aguarda o
deadline e valida fallback `VALID` antes de pedir rollback. O caso de worker de
flash que não responde é registrado como recovery bloqueado, não como sucesso.

Testes host aprovados: políticas, manifesto, assinatura, hash, keyring,
anti-replay, journal, recovery e endpoints HTTPS; adicionalmente sete cenários
determinísticos do supervisor e testes do gerador de configuração pública. O
P4 foi compilado limpo com IDF 5.5.4 para `esp32p4`: app `0x27b000` B,
`0x585000` B livres no menor slot (69%), SHA-256
`E530BC43071B684F1A03B2461E1FB801DF70B624382E2B762B65CB2E4E0D9F0F`.
Também compilou a variante configurada, com host fictício, sem flash:
`0x27b8d0` B, SHA-256
`FB73E71EEDC250C6A5D58D3566A1C223EDAAF89D4DC8A0793DB8CC4B03DDA9B6`.

Na unidade Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3, COM8, antes do
flash foi salvo o slot `ota_0` e lido `otadata`: `ota_0` estava `VALID` com
sequência 1 e CRC correto. A tabela instalada coincide byte a byte com a build.
Foi gravada somente a app P4 normal, preservando bootloader, tabela e
`otadata`; leitura posterior confirmou `otadata` byte a byte idêntico. O boot
de 40 s confirmou PSRAM 32 MiB, EK79007, GT911, RGB565/180°/TRIPLE_PARTIAL/3 FB,
C6 Hosted 3.0.6/RPC v2/SW_AGGR, scan RAM-only de 33 APs e Home aos 7,984 s, sem
panic ou WDT observado. Warnings existentes do adapter sobre
`on_frame_buf_complete` e gesto de toque continuam pendentes de qualificação
gráfica e não foram tratados como aprovação de render.

O pacote assinado de laboratório versão 2 foi gerado e verificado por OpenSSL,
com revisão 103, tamanho e SHA-256 iguais à candidata. Ele ainda não foi servido
por HTTPS, aplicado, confirmado, revertido nem submetido a corte de energia.
O C6 não foi escrito. Portanto G5 permanece aberto: falta um host HTTPS
confiável e alcançável pelo painel para o primeiro ciclo P4 e, depois, os
ensaios de rollback/recovery/cortes e o gate independente do C6.

### Resultado parcial do incremento 3g — preflight HTTPS OTA P4 — 2026-09-13

`network_validation_service` ganhou um pedido de preflight de desenvolvimento
na sua única task HTTPS. Antes de enfileirar a operação, a política exige três
URLs HTTPS distintas, sem porta, query, fragmento ou credenciais, todas no host
explicitamente autorizado. A task continua a executar DNS e NTP e, dentro do
mesmo orçamento total de 20 s, baixa o manifesto com exatamente 104 bytes e a
assinatura destacada com exatamente 388 bytes. Qualquer corpo truncado ou maior
falha fechado. Assim não há um segundo handshake TLS concorrente.

A URL da imagem é apenas validada estruturalmente nesta etapa. A imagem não é
baixada, nem há validação criptográfica, journal, escrita de slot, selecção de
boot, alteração de `otadata`, NVS de produto, C6 ou eFuses. Esses passos só
podem receber uma configuração de desenvolvimento que una keyring, ambiente de
metadados e transação persistente ao worker.

O teste host da política HTTPS passou, junto aos vetores de manifesto,
assinatura, hash, keyring, anti-replay, journal, recovery e política de boot.
O build ESP-IDF 5.5.4 gerou `0x1780b0` B, com `0x687f50` B livres no menor slot
OTA (82%); SHA-256 P4:
`A0AE6D2CB566183F5234EF7EF6FD5D675A52D4C6D5E94246E98EDA72521A3120`.
O flash COM8 verificou bootloader, app, tabela e `otadata`. No boot, P4 v1.3,
32 MiB flash/PSRAM, EK79007, GT911, dashboard e C6 Hosted 3.0.6/RPC v2/SW_AGGR
subiram sem panic ou WDT; a varredura Wi-Fi sem credenciais encontrou 45 APs.
