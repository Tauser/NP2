# Auditoria técnica do NP2 — 2026-09-12

## Conclusão e escopo

Na fotografia original auditada, o P4 compilava e os três testes host então
existentes passavam, mas G4 continuava aberto com os defeitos A1–A6 abaixo.
A remediação posterior corrigiu esses achados e acrescentou um quarto teste
host; G4 permanece aberto somente porque as provas físicas e a integração de
produto listadas ao final ainda não foram executadas.

A análise considerou o histórico recente, os dois documentos normativos,
roadmap, ADRs, evidências, configuração, código de aplicação e fontes locais
relevantes do ESP-IDF/adapter. Base: `1d3ca02`, com alterações locais anteriores
à auditoria. Havia 10 arquivos rastreados modificados e 19 entradas não
rastreadas no status (algumas representam diretórios). Esse código novo não é
reproduzido por um checkout apenas desse commit.

A coleta da auditoria original não alterou o firmware. A remediação posterior
alterou código e documentos e, após o build limpo, gravou somente o P4 pela
COM8. Não houve corte de energia, atualização C6 ou escrita de eFuses.
Resultados físicos anteriores abaixo são registros históricos; os resultados
novos são de build, testes host, inspeção de fonte e confirmação do flash.

## Atualização de remediação — 2026-09-12

Os achados A1–A6 foram corrigidos na árvore de trabalho após esta auditoria:

| Achado | Correção aplicada | Validação atual |
|---|---|---|
| A1 | HTTPS passou de `perform()` para `open`/`fetch_headers`/`read`, com orçamento monotônico recalculado, timeout ocioso de leitura de 5 s, corpo GET limitado a 512 B e fechamento imediato por limite | Build P4 PASS; HIL lento/excedido pendente |
| A2 | Somente `ENOSPC` aprova rejeição; snapshot real de 25 B; limpeza e uso recuperado obrigatórios; nenhuma geração ativa recebe `rename` | Build P4 PASS; repetição física pendente |
| A3 | Leitor JSON estrutural limitado, documento completo, objeto pai explícito e duplicatas recusadas | Regressões host PASS |
| A4 | Inteiros recusam decimal; fixos arredondam de forma definida com contador `size_t` | Regressões host PASS |
| A5 | Faixas, presença e timestamps são validados em entrada/encode/decode; schema exige campos quando disponível | Regressões host PASS |
| A6 | Sinal é formatado separadamente da magnitude | Casos `-0.5` e `-0.25` PASS no host |

O build limpo final em `build/audit-fix-final-20260912` gerou um candidato P4
de `0x173160`, com `0x68cea0` bytes (82%) livres no menor slot. SHA-256 da
imagem: `8D5164425F3F70B40A3EFFB0B04D3421DD3DB80F5B13D778A16F9C9803CA5EA6`;
SHA-256 do `sdkconfig`:
`078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A`.
Esta atualização não transforma build em evidência física: G4 permanece
aberto e os parágrafos históricos abaixo continuam válidos para a revisão
auditada.

O candidato final foi gravado no P4 v1.3 pela USB Serial/JTAG COM8. O
`esptool.py 4.12.0` verificou o hash de bootloader, tabela de partições,
`otadata` e aplicação e executou hard reset. NVS, partição `storage`, imagem C6
e eFuses não foram escritos. Essa gravação habilita, mas não substitui, os
ensaios HIL pendentes de A1/A2.

## Validações executadas na fotografia original

| Verificação | Resultado |
|---|---|
| Build P4 em diretório novo e sdkconfig próprio, a partir dos defaults | PASS; ESP-IDF `v5.5.4-dirty`, target `esp32p4`, GCC RISC-V 14.2.0 |
| Tamanho e tabela gerada durante o build | App `0x173230` = 1.520.176 B; slot de 8 MiB, 82% livre; bootloader `0x5a50` |
| Baseline efetivo principal | 32 MiB flash, revisão mínima 100, RGB565, 3 buffers, XiP PSRAM 200 MHz, SDIO 4-bit 18/19/14–17, reset54 baixo |
| Restrições de configuração | Auto-suspend, ESP_HOST_WIFI e restart automático do host por falha de transporte desabilitados; TLS em memória interna; rollback habilitado |
| `cache_record_host_test` | PASS com `gcc -std=c11 -Wall -Wextra -Werror` |
| `offline_data_codec_host_test` | PASS com os mesmos flags |
| `offline_data_provider_host_test` | PASS com os mesmos flags |
| Casos adicionais de auditoria | Seis comportamentos incorretos reproduzidos; detalhados abaixo |

Com terminal IDF preparado, o comando usado, dentro de `firmware`, foi:

```text
idf.py -B build/audit-20260912 -D SDKCONFIG=build/audit-20260912/sdkconfig -D IDF_TARGET=esp32p4 build
```

O build terminou com código 0. O log local está em
`firmware/build/audit-20260912-build.log`; houve dois avisos de geração de
`esp_rom gdbinit`, um para cada configuração relevante. Isso limita a
preparação da depuração de ROM neste ambiente; não impediu compilação/link.
Não apareceram warnings de compilação C/C++ no log. O acesso inicial ao
diretório de build foi negado pelo sandbox; o build autorizado foi executado
com a permissão ampliada, preservando a pasta de build anterior.

Identificação dos artefatos originais da auditoria, não gravados na placa:

```text
np2_p4.bin SHA256:
ED22959F4FD0F40C19EE94F4164B20971F5BE4FDABED50FAC7FD97DF3F171C07
sdkconfig da auditoria SHA256:
078934494FB1C145BE140A22B96A3B3D00A32B76BDBCDA6FC5D14B1D3463C78A
firmware/dependencies.lock SHA256:
245A1338773E0D2759F21DC72D1F314958FFE02061D0823817DB17EB71976773
```

O build limpo prova compilabilidade com os componentes locais resolvidos;
não é reprodução em uma máquina virgem, build C6, assinatura de release ou
aprovação física da imagem nova.

## Defeitos atuais, por prioridade

### A1 — P1: HTTPS não interrompe leitura por tamanho ou prazo total

**Fonte:** `firmware/main/network_validation_service.c:198`, `:249` e `:327`.

O callback apenas liga `exceeded` e retorna `ESP_OK`. A verificação dessa flag
ocorre depois de `esp_http_client_perform()` retornar. O prazo total também é
avaliado depois da operação bloqueante. Nos fontes locais do IDF,
`esp_http_client.c:1464` e `:1546`, a leitura continua por chunks, usando
timeout de transporte em cada leitura, sem consultar o deadline da aplicação.

Assim, uma resposta grande é consumida antes de receber erro; um servidor
que entrega pequenos trechos antes de cada timeout pode ocupar o único worker
por mais de 20 s. Não há indicação de armazenamento ilimitado do corpo neste
diagnóstico, mas duração e tráfego não estão limitados conforme anunciado.
A prova anterior de 2 KiB em 2,3 s demonstra erro no resultado, não aborto
streaming nem deadline sob entrega lenta contínua.

**Correção recomendada:** leitura controlada/assíncrona com orçamento monotônico
remanescente e encerramento efetivo do cliente ao exceder bytes ou deadline.
Só retornar erro no callback não é correção comprovada: o caminho local do
IDF deve ser conferido. Ensaiar chunks contínuos, corpo excedido e liberação
do executor para a próxima operação. Achado por inspeção; sem novo HIL.

### A2 — P1: ensaio de filesystem cheio pode aprovar falha de outra natureza

**Fonte:** `firmware/main/flash_coordinator.c:635–675`.

Qualquer erro em `write_cache_record_at_path()` define `proposal_rejected`,
inclusive falha de leitura/verificação, CRC, abertura, fechamento ou I/O.
Não se preserva a causa tipada para exigir falta de espaço. Além disso, o
retorno da limpeza final é ignorado. Se a geração permanecer selecionável,
o ensaio pode retornar `ESP_OK` mesmo com temporários que não foram removidos.

Há ainda uma diferença de cobertura: o ensaio usa payload de 4.096 B
(registro de 4.120 B), enquanto o snapshot de domínio usa 25 B (registro de
49 B). Rejeitar o primeiro não prova a rejeição do segundo; a reserva de
alocação/arquivos inline do filesystem precisa ser exercitada no formato real.

**Correção recomendada:** preservar a causa da falha, distinguir ENOSPC de
I/O/corrupção, exigir limpeza confirmada, verificar espaço recuperado e testar
o snapshot real. Manter as gerações ativas preservadas. Só depois repetir a
bancada. A versão atual é candidata; as tentativas anteriores não passaram.

### A3 — P1: parser não respeita a estrutura do JSON ou o domínio dos campos

**Fonte:** `firmware/main/offline_data_provider.c:16`, `:133` e `:170`.

`find_json_value()` procura a primeira ocorrência textual de uma chave em
todo o corpo. Não valida objeto pai, fechamento do documento, escapes ou
duplicatas. Casos reproduzidos com os próprios parsers compilados no host:

| Entrada | Resultado atual | Resultado necessário |
|---|---|---|
| `current_units.temperature_2m="C"` antes de `current.temperature_2m=23.7` | Rejeita a leitura numérica válida | Ler somente `current` |
| Objeto `ethereum` com `usd=2500`, variação e timestamp, sem `bitcoin` | Retorna OK e preenche Bitcoin com 250000 centavos | Rejeitar ativo ausente |
| Objeto de clima sem as duas chaves finais `}}` | Retorna OK | Rejeitar JSON truncado |

**Correção recomendada:** tokenizer/parser JSON limitado, com validação do
documento inteiro e seleção explícita de `current`/`bitcoin`; manter limites
de corpo e memória. Adicionar fixtures com metadados, objetos em outra ordem,
campos duplicados, escapes, ativo ausente e truncamento. Esses adapters ainda
não são chamados pelo fluxo de produto, mas o defeito bloqueia sua integração.

### A4 — P2: conversão numérica aceita valores inválidos por truncamento

**Fonte:** `firmware/main/offline_data_provider.c:76–115`.

`parse_u32()` reutiliza o parser decimal com escala zero: umidade `100.9`
retorna OK como `100`. Decimais são descartados antes da validação da faixa.
O parser também não exige dígito após o ponto; o contador de casas decimais
é `uint8_t`, embora o corpo aceite até 768 B.

**Correção recomendada:** inteiros devem ter sintaxe inteira e faixa validada
sem truncamento; fixar a política de precisão dos campos decimais, exigir
sintaxe válida e limitar/usar contador que não transborde. O caso `100.9`
foi reproduzido; os demais decorrem da inspeção do mesmo caminho.

### A5 — P2: codec não valida as faixas do contrato de domínio

**Fonte:** `firmware/main/offline_data_codec.c:45–58`; comparação com
`shared/schemas/offline_snapshot.v1.schema.json`.

Foi possível codificar e decodificar um snapshot com temperatura de 200,0 °C
e umidade de 255%, apesar dos limites de -100,0 a 100,0 °C e 0–100% do schema.
A validação atual cobre versão, origem, flags e presença, mas não esses
valores. Um produtor defeituoso pode persistir conteúdo inválido com CRC
correto; CRC não substitui validação semântica.

**Correção recomendada:** centralizar a validação de domínio e aplicá-la na
entrada, encode e decode; definir também coerência dos timestamps e campos
obrigatórios quando `available=true`. Revisar o schema JSON, que atualmente
permite `available=true` sem os campos de medição.

### A6 — P2: UI perde o sinal de números negativos menores que uma unidade

**Fonte:** `firmware/main/offline_dashboard.c:71–96`.

Divisão inteira produz zero e o resto é convertido para positivo. Usando as
expressões exatas de formatação, -0,5 °C aparece como `0.5 C` e variação
-0,25% aparece como `0.25%`. O segundo caso apresenta uma queda como alta.

**Correção recomendada:** formatar o sinal separadamente da magnitude;
verificar zero, valores entre -1 e 0 e limites. A aritmética foi reproduzida
no host; a apresentação visual não foi ensaiada na placa nesta auditoria.

## O que já foi construído e o que ainda falta

| Área | Entrega observada | Limite atual |
|---|---|---|
| G0 | Lock P4, defaults, partições e build limpo reproduzido | Fechamento documentado cobre P4; não foi reproduzido C6 nesta auditoria |
| G1 | Boot, PSRAM, EK79007, GT911, backlight e orientação | Fechamento com escopo limitado, sem qualificação de produto |
| G2 | Touch, carga gráfica, NVS/LittleFS e soak de 30 min registrados com XiP | Teste FS cheio recente falhou; XiP ainda depende dos gates de OTA/produto |
| G3 | Associação, DHCP, DNS/NTP/TLS, recovery/cooldown e WPA2 RAM-only | Exclui C6 fisicamente ausente/travado; revisar A1 antes de assumir limites completos |
| G4 | EventBus de 32 eventos, projeção pelo app_loop, cache CRC/duas gerações, codec e cards | Providers HTTPS não ligados, dados reais ausentes e ensaios de corte/cheio pendentes |
| G5–G7 | Planejamento e partições reservadas | OTA assinada, recovery conjunto, qualificação e produção não entregues |

`flash_coordinator_request_offline_data_write()` e os dois parsers não possuem
chamadores de produto em `firmware/main`. O dashboard hoje deriva os dados do
cache; não existe o ciclo completo adquirir → atualizar RAM → persistir →
reabrir offline. O app_loop é uma projeção periódica dos serviços, ainda não
o conjunto completo de redutores/intenção de produto previsto no plano.

Rollback habilitado no Kconfig não equivale a OTA recuperável: não há no código
de aplicação o health-check/confirmador de `PENDING_VERIFY`. Isso pertence à
entrega ainda aberta do G5, não a uma funcionalidade aprovada.

## Dificuldades e erros históricos

| Ocorrência documentada | Causa/tratamento registrado | Situação |
|---|---|---|
| Falha no primeiro display | EK79007 não implementa `disp_on_off`; chamada removida | Corrigida e boot repetido |
| Boot loop por proteção de pilha | Primeiro refresh excedeu pilha de bootstrap; ampliada para 16 KiB | Corrigido e validado no escopo inicial |
| Touch acionava canto oposto | Rotação duplicada no GT911 e adapter | Corrigido; cinco alvos retestados |
| Flashes durante manutenção NVS | Transição de UI/backlight não resolveu contenção de flash/display | Variante XiP passou o escopo G2; OTA ainda pendente |
| Métricas de carga desapareciam/eram contaminadas ao pausar | Retenção e instante de amostragem incorretos | Instrumentação corrigida no histórico |
| C6 não retomava após cooldown | Timestamps antigos causavam nova entrada imediata no bloqueio | Correção e campanha posterior aprovadas |
| Senha incorreta entrava em recuperação contínua | Classificação não cobria todos os motivos de autenticação | Corrigida e retestada |
| Tela preta em 12/09 | Reset restaurou UI; COM8 estava ocupada por monitor residual | Recuperação observada, causa da tela preta não demonstrada |
| FS cheio em 12/09 | `cache.tmp`, tamanho do preenchimento e reserva de alocação produziram resultados inválidos; uma versão chegou a promover geração | Tentativas de ~113–228 s falharam; candidata atual sem passe físico |

O atraso de flash enquanto a carga sintética fica ativa também foi observado:
o worker executou após pausar a carga. Isso exige medição de escalonamento e
prazo de fila; não basta atribuir todo o atraso ao LittleFS. Faltam fixture
segura para isolar o C6, cortes físicos observados, localidade explícita e
política de acesso dos providers. Essas são dificuldades diferentes de bugs
de parser ou persistência.

## Qualidade da evidência e documentação

- `ROADMAP.md` marca G0 completo, mas o plano premium exige também cadeia C6,
  transitivas, patch e artefatos. `coprocessor/` contém somente README e defaults.
  Registrar explicitamente esse escopo e completar o pacote reproduzível C6.
- O soak G3 de 24 h é relato do operador, sem série contínua de heap/serial.
  É evidência útil, mas não substitui o ensaio instrumentado de 168 h do G6.
- A evidência G2 reporta 28.169 B livres na pilha LVGL, enquanto o código do
  commit indicado configura 12.288 B. A API IDF retorna bytes e `StackType_t`
  neste port RISC-V é `uint8_t`; portanto não foi confirmado o suposto erro
  comum de multiplicar por quatro. A discrepância do registro precisa ser
  reconciliada com captura e imagem exatas antes de usar a margem como prova.
- `G4-CLOSURE-AUDIT.md` ainda descreve a imagem `0x1724c0` de 11/09 e não as
  falhas posteriores; o histórico de 12/09 em `BRINGUP-EVIDENCE.md` é mais novo.
- Partes de `DEVELOPMENT.md`/roadmap ainda falam em scaffold ou passos já
  superados. A linha de G5 continua citando bloqueio por G3/G4 apesar do
  fechamento limitado de G3. Consolidar um estado atual sem apagar o histórico.
- “C6 não regravado” é informação de operação, não identificação criptográfica
  do par. Relatos sem hash/log bruto completo não atendem sozinhos ao DoD final.

## Ordem recomendada de correção e revalidação

1. Corrigir A1 e A2 para que os próprios ensaios tenham limites e critérios
   confiáveis. Registrar causas de falha e limpeza; preservar gerações ativas.
2. Corrigir parsers, validação de snapshots e sinal numérico com regressões
   dos casos acima antes de ligar providers reais.
3. Integrar aquisição por executor único, estado em RAM independente da
   persistência, localidade e política de atualização/idade explícitas.
4. Repetir FS cheio com registro real e cortes antes/depois de rename;
   demonstrar dados válidos após reboot sem rede e continuidade de UI.
5. Consolidar commits pequenos, hashes e documentos. Revalidar os limites de
   G3 afetados por A1; fechar G4 somente com as provas faltantes.
6. Avançar então para OTA/recovery conjunto e qualificação. O estado atual
   não sustenta declaração de release, segurança de produção ou estabilidade
   completa sob todas as falhas.
