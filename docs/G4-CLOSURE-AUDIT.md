# Auditoria de fechamento — G4 Dados Offline

**Data:** 2026-09-12
**Estado:** parcialmente implementado; gate aberto.

Esta auditoria separa validações de fonte e build das evidências físicas que
realmente podem fechar G4. A ausência de uma linha de evidência abaixo não é
uma falha presumida: é requisito ainda não provado.

| Critério do gate | Implementação/evidência atual | Estado |
|---|---|---|
| Escritor único e UI sem I/O | `app_state`, `app_event_bus` (32 eventos), `offline_dashboard`; build P4 candidato `0x173160` | Implementado; precisa observação da UI na bancada |
| Modelo e parser limitados | `offline_snapshot.v1`, validação semântica, leitor JSON estrutural (8 níveis/768 B) e regressões de truncamento/duplicatas/objeto pai | Implementado; adapters HTTPS de produto ainda não ligados |
| Cache versionado, CRC e duas gerações | `cache_record` + `FlashCoordinator`; corrupção da geração mais nova e reboot já registrados em `BRINGUP-EVIDENCE.md` | Parcialmente provado na placa |
| Idade, schema e estados offline | card mostra geração, schema e idade; stale em 2 h (clima) e 30 min (mercado) | Implementado; falta snapshot real e reboot offline |
| Quota e cadência | duas gerações de tamanho fixo; escrita de snapshot limitada a uma a cada 30 min | Implementado; falta ensaio físico cheio |
| Filesystem cheio/limpeza | Checkpoints duráveis do ADR-020; `ESP_OK` em 154.535 ms e reboot em `geracao 137 integra | cache v1 dados v0` | Passe físico parcial; faltam repetição, continuidade visual e logs |
| Corte após `fsync`, antes de `rename` | comando físico `CACHE_CUT_BEFORE_RENAME` e protocolo de 10 s | Pendente de bancada |
| Corte após `rename` | comando físico `CACHE_CUT_AFTER_RENAME` e protocolo de 10 s | Pendente de bancada |
| Sem credencial em estado/log/dump | modelos e eventos não têm campos de segredo; parsers não aceitam URL/header | Revisão estática parcial; evidência de operação ainda pendente |

## Evidência de software atual

- Quatro testes host passaram com `-std=c11 -Wall -Wextra -Werror`: cache,
  codec/faixas, parser JSON e formatação de sinal fixo.
- Os casos adicionais da auditoria agora rejeitam objeto pai errado, Bitcoin
  ausente, JSON truncado, inteiro decimal e duplicata; preço/variação com casas
  extras seguem arredondamento definido.
- Build limpo em `build/audit-fix-final-20260912` passou para `esp32p4`;
  candidato `0x173160`, com `0x68cea0` bytes (82%) livres. SHA-256:
  `8D5164425F3F70B40A3EFFB0B04D3421DD3DB80F5B13D778A16F9C9803CA5EA6`.
- O candidato foi gravado no P4 v1.3 pela COM8; o `esptool.py 4.12.0`
  verificou todos os blocos e executou hard reset. NVS, `storage`, C6 e eFuses
  não foram escritos. Os ensaios funcionais desta imagem ainda estão pendentes.

## Bloqueios reais para fechamento

1. O primeiro `CACHE_FULL_PROBE` corrigido passou e o reboot preservou a
   geração 137; falta uma repetição observada com log de `ENOSPC`/limpeza e
   continuidade visual/touch.
2. Os dois cortes exigem desligamento físico dentro da janela de 10 s agora
   indicada diretamente no painel.
3. O snapshot real exige uma localidade de produto explícita; não há GPS nem
   coordenada definida no repositório, portanto não foi inventada. CoinGecko
   também deve ter uma política de credencial aprovada antes de qualquer uso
   que a exija.

4. O HTTPS controlado precisa de HIL com chunks contínuos, excesso de corpo,
   prazo total e liberação do executor para a operação seguinte.

Após cada ensaio, acrescente no `BRINGUP-EVIDENCE.md` a geração antes/depois,
o log bruto, a imagem, a observação visual e o resultado de reboot. Só então
esta auditoria pode mudar para `done` e o roadmap para `Complete`.
