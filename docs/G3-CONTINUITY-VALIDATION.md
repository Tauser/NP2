# G3 — protocolo rígido de continuidade e recuperação

## Objetivo

Validar que o painel continua utilizável e recupera a conectividade de forma
limitada e observável. AP, DHCP, DNS, Internet ou C6 podem impedir dados
remotos, mas não podem parar display, touch, estado local ou provocar reset do
P4. Este protocolo implementa o
[ADR-010](DECISIONS.md#adr-010--continuidade-local-e-recuperação-limitada-de-conectividade)
e complementa o [plano premium](PLANO-FIRMWARE-PREMIUM.md#12-critérios-objetivos-e-definition-of-done).

Não fechar G3 com scan, foto isolada, build verde ou uma conexão bem-sucedida.

## Pré-condições por execução

- Registrar data, unidade/PCB/BOM, alimentação, AP de bancada, firmware P4 e
  C6, commit, hashes P4/C6, `sdkconfig`, `dependencies.lock` e patch Hosted.
- Confirmar P4 v1.3, C6 3.0.6, RPC v2, SDIO quatro bits e SW_AGGR antes de
  induzir falhas. Nunca gravar o C6 por USB nesta campanha.
- Usar SSID e senha de laboratório descartáveis. Capturar serial em arquivo
  protegido, conferindo depois que ele não contém SSID ou senha.
- Rodar com EK79007 RGB565, rotação 180°, três framebuffers,
  `TRIPLE_PARTIAL`, draw buffer de 50 linhas e
  `CONFIG_SPI_FLASH_AUTO_SUSPEND=n`. Não alterar essas variáveis para fazer um
  ensaio passar.
- Exibir carga móvel e entrada por touch durante cada caso. Marcar os horários
  de cada injeção no log e filmar os casos que afetem C6, DHCP ou recuperação
  enquanto há render ativo.
- Medir no início e fim de cada rodada heap interno/PSRAM, maior bloco,
  high-water das tasks, contadores de reset e estado da FSM. A falta de uma
  métrica invalida a rodada, não é resultado neutro.

## Contrato e limites

| Item | Limite verificável |
|---|---|
| UI local | Render e touch continuam. Nenhum WDT, panic, reset P4 ou reinício de display por AP/DNS/Internet ausente. |
| Associação | Prazo de 15 s; a expiração entra em recuperação sem bloquear LVGL. |
| DHCP | Prazo de 20 s; a expiração entra em recuperação sem bloquear LVGL. |
| DNS | Prazo de 5 s; após duas falhas consecutivas, no máximo uma reassociação por minuto. |
| Retry Wi-Fi | 2/4/8/16/30 s com jitter. Senha rejeitada para em erro e requer pedido novo, sem loop. |
| C6 | Até três ciclos completos de recuperação por dez minutos; então cooldown de cinco minutos ou tentativa explícita. P4 não reinicia só por falha externa. |
| Requests futuros | Um handshake/HTTPS global; conexão/TLS até 10 s e prazo total até 20 s. |
| Recuperação | AP restaurado: IP até 90 s. Com hora confiável e backend saudável: dado válido até 120 s. |

## Casos obrigatórios

Executar cada caso no mínimo três vezes, alternando boot frio e estado já
associado quando aplicável. Repetir a rodada se a instrumentação falhar.

| Caso | Injeção e observação | Aprovação |
|---|---|---|
| Associação normal | Provisionar pelo canal restrito, confirmar associação, IP e depois esquecer. | Estados e deadlines corretos; nenhum segredo no log; UI responde durante todo o fluxo. |
| Senha inválida | Enviar credencial deliberadamente errada. | Erro explícito, sem tentativa infinita; nova solicitação válida continua possível. |
| AP ausente | Desligar o AP por 10 min durante render e toque; religar. | UI local íntegra; retry limitado; IP em até 90 s depois do retorno do AP. |
| DHCP silencioso | Associar a AP que não responde DHCP por pelo menos 25 s; restaurar DHCP. | Deadline de DHCP registrado; sem UI bloqueada; recuperação medida após restauração. |
| DNS negativo e timeout | Forçar NXDOMAIN e timeout duas vezes, depois restaurar resolução. | Falhas separadas; no máximo uma reassociação/min; sem segundo handshake. |
| C6 reiniciado | Acionar reset54 conforme procedimento da placa durante tráfego e render; repetir até o limite. | Display e P4 não reiniciam; máximo três ciclos/10 min e cooldown observado. |
| C6 ausente/travado | Inibir resposta C6 antes do boot e durante recuperação. | Supervisor registra deadline Hosted de 30 s; UI permanece responsiva; não há WDT mascarado. |
| Backend lento/TLS inválido | Quando existir executor HTTPS, atrasar resposta e forçar certificado/hostname inválido e corpo acima do limite. | Deadline total, uma conexão global, cache local preservado e nenhum retry de falha de autenticidade. |
| Reboot e retenção | Só após persistência aprovada: reboot frio/quente, corte de AP e retorno. | Credencial anterior só muda após 30 s contínuos de IP e commit íntegro; recuperação automática sem vazamento. |

## Procedimento inicial sem senha

Usar um AP de laboratório descartável, aberto e isolado. No painel, tocar
`ARMAR REDE USB (60s)` e, dentro da janela, enviar pelo USB Serial/JTAG em
115200 bps uma única linha `OPEN <ssid>` seguida de quebra de linha. A resposta
é `OK` ou `ERR <nome-esp_err>`; o erro identifica somente a operação local e
o firmware não ecoa o SSID. Para limpar a associação em RAM, repetir a
ativação física e enviar `FORGET`.

Não enviar senha por este protocolo, pelo chat, por log ou por configuração
versionada. Encerrar o monitor serial antes de abrir a porta para escrever o
comando. O AP aberto existe apenas para estes ensaios e deve ser desligado ao
fim da rodada.

Para diagnosticar um AP já associado, armar novamente a janela física e enviar
`CHECK`. Ele não aceita host, URL ou certificado fornecido pelo operador: mede
DNS, NTP e um único HTTPS `HEAD` de destino fixado. A tela mostra apenas os
códigos ESP-IDF e a duração. O teste deve ocorrer enquanto a carga de render e
o touch estão ativos; registrar memória antes/depois. `CHECK` que retorna
timeout ou erro é evidência de falha de rede, nunca motivo para reiniciar P4
ou regravar C6.

Se o monitor do VS Code for somente de saída, fechá-lo para liberar a porta e
usar `tools/send_usb_maintenance_command.ps1`. A ferramenta aceita `OPEN`,
`FORGET`, `RECOVER_C6`, `CHECK`, `CHECK_NXDNS`, `CHECK_DNS_TIMEOUT` e
`CHECK_TLS_REJECT`, `CHECK_HTTPS_TIMEOUT`, `CHECK_HTTPS_OVERSIZE`,
`CHECK_DHCP_TIMEOUT`, `RECOVER_C6_COOLDOWN` e
`RECOVER_C6_COOLDOWN_FULL`. A ferramenta mantém a porta aberta e coleta o resultado
assíncrono das duas últimas campanhas; não abrir o monitor em paralelo.
Ela envia uma linha a 115200 bps, lê a resposta imediata e sempre fecha a COM.
Abrir novamente o monitor somente após o comando. Os três últimos modos têm
alvos fixos no firmware: não aceitam host, URL, certificado ou segredo do
operador.

## Campanha e critérios de aprovação

### Estado da evidência em 2026-09-10

| Caso | Estado | Evidência atual e limite remanescente |
|---|---|---|
| Associação aberta e FORGET | Passou | Três ciclos corrigidos; configuração somente em RAM. |
| AP ausente e retorno | Parcial | Mais de dez rodadas relatadas, recuperação em cerca de 25 s; faltam captura bruta e continuidade de touch/UI por rodada. |
| DNS, NTP, TLS válido/inválido e backend lento | Passou no escopo de manutenção | Timeout DNS de 5 s, backend lento abaixo de 20 s e corpo HTTPS acima de 512 B rejeitado. |
| DHCP | Passou somente na injeção local | Deadline de 20 s, restauração e retry físico passaram; AP realmente silencioso segue pendente. |
| C6 reiniciado e limitador | Passou no escopo do lifecycle | Três resets Hosted recuperaram IP em cerca de 5,94 s, a quarta tentativa foi recusada e, após cinco minutos, a quinta recuperação foi permitida com IP adquirido. Continuam pendentes as métricas independentes de UI/touch/memória e o C6 ausente/travado. |
| C6 ausente/travado no boot | Fora do escopo desta unidade | Supervisor de 30 s existe; em 2026-09-11 a unidade não dispunha de jumper, chave ou outro meio físico seguro para isolar o C6. O operador decidiu não alterar o hardware. Não dirigir GPIO54 pela aplicação é requisito do projeto, portanto o caso não será simulado por firmware. |
| WPA2 e senha inválida | Passou no escopo RAM-only | Modal por touch associou WPA2, senha inválida terminou sem loop, senha válida recuperou IP e reboot voltou a sem credencial. Persistência cifrada permanece bloqueada pelo gate de segurança. |

Nenhuma linha `Parcial` ou `Implementado, sem bancada` autoriza o fechamento
rigoroso de G3. O roadmap registra em 2026-09-11 um fechamento **no escopo
desta unidade**, apoiado nas campanhas físicas realizadas e no soak de 24 h
relatado pelo operador. Ele exclui explicitamente C6 fisicamente
ausente/travado e não substitui a qualificação de G6.

1. Concluir os casos implementados em três repetições consecutivas sem falha.
   Hoje a implementação cobre boot, Hosted, scan, associação aberta, DHCP,
   AP-off/AP-on e o CHECK DNS/NTP/HTTPS quando seus gatilhos correspondentes
   estiverem disponíveis. Senha inválida e reset C6 ainda exigem os caminhos
   seguros e a recuperação Hosted especificados neste documento.
2. Após NTP e HTTPS, executar DNS e backend e medir recuperação. Não estimar
   tempo a partir do log de tentativa.
3. Rodar soak de conectividade por 24 h com AP ligado/desligado, C6 resetado
   de modo controlado e carga de UI. Registrar memória após 30 min de
   aquecimento e no fim. A qualificação de 72/168 h continua em G6.
4. Reprovar diante de panic, WDT inesperado, reset P4 por falha externa, tela
   branca/tearing, travamento de touch, quebra dos retries, mais de um TLS em
   voo, segredo em evidência ou crescimento de memória sem explicação.

## Evidência, rollback e promoção

Anexar em `BRINGUP-EVIDENCE.md` tabela por rodada com disparo, timestamps,
estado antes/depois, IP e dado válido, memória, reset reason, hashes e
referência para serial/vídeo protegido. Declarar os casos ainda ausentes no
firmware; nunca marcá-los aprovados por analogia.

Se um incremento falhar, parar a promoção, manter o painel no modo offline
local e restaurar o último binário P4 validado. Não reflashear o C6 como
contorno: qualquer mudança segue Slave OTA via SDIO, a matriz de
compatibilidade e um gate próprio. Revogar credenciais de bancada que tenham
aparecido em log ou dump antes da próxima rodada.
