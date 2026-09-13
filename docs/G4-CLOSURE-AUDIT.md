# Auditoria de fechamento — G4 Dados Offline

**Data:** 2026-09-12
**Estado:** Complete — scoped close em 2026-09-12.

Esta auditoria separa as validações de fonte/build da confirmação física do
operador. A tabela conserva a origem de cada prova; o fechamento tem escopo
de uma unidade e não substitui a qualificação futura.

| Critério do gate | Implementação/evidência atual | Estado |
|---|---|---|
| Escritor único e UI sem I/O | `app_state`, `app_event_bus` (32 eventos), `offline_dashboard`; navegação diagnóstico→painel compilada | Validado pelo operador na bancada |
| Modelo e parser limitados | `offline_snapshot.v1`, validação semântica, leitor JSON estrutural (8 níveis/768 B) e regressões de truncamento/duplicatas/objeto pai | Validado pelo refresh Brasília |
| Cache versionado, CRC e duas gerações | `cache_record` + `FlashCoordinator`; corrupção da geração mais nova e reboot | Validado nesta unidade |
| Idade, schema e estados offline | cards mostram geração, schema, idade e confiança da hora | Validado após refresh e reboot offline |
| Quota e cadência | duas gerações de tamanho fixo; escrita de snapshot limitada a uma a cada 30 min | Validado pelo ensaio cheio |
| Filesystem cheio/limpeza | Checkpoints duráveis do ADR-020; `ESP_OK` em 154.535 ms e reboot íntegro | Validado nesta unidade |
| Corte após `fsync`, antes de `rename` | comando físico `CACHE_CUT_BEFORE_RENAME` e protocolo de 10 s | Validado nesta unidade |
| Corte após `rename` | comando físico `CACHE_CUT_AFTER_RENAME` e protocolo de 10 s | Validado nesta unidade |
| Sem credencial em estado/log/dump | modelos e eventos não têm campos de segredo; parsers não aceitam URL/header | Revisão estática e operação confirmada pelo operador |

## Evidência de software anterior ao fechamento

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
  não foram escritos. Naquele momento os ensaios funcionais ainda estavam
  pendentes; eles foram posteriormente confirmados pelo operador.

## Escopo fechado e riscos transferidos

O operador confirmou que todos os ensaios físicos acima, inclusive o reboot
offline com Brasília, funcionaram corretamente na unidade P4 v1.3. O gate G4
está fechado para avançar em G5. A confirmação foi declarada pelo operador;
logs seriais brutos completos e a identificação exata de cada repetição não
foram anexados nesta interação.

Isso não é qualificação de release. Repetição multiunidade, servidor HTTPS
lento/excedido, fault injection de armazenamento/rede e métricas prolongadas
continuam obrigatórios em G6.
