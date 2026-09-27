# Serviço de notificações

## Estado atual

O serviço controla as preferências globais de notificações e é a fonte de
verdade para a UI:

```text
Settings (intenção) → notification_service → FlashCoordinator
                                      ↓
                         app_loop / app_state projection → UI
```

`notification_service` não chama LVGL, NVS, rede, sensores ou drivers. A
task LVGL apenas envia uma intenção `notification_service_set_*()` e consome a
projeção publicada por `app_loop`.

As preferências são três valores não secretos:

- `general_enabled`: permite notificações não críticas.
- `sound_enabled`: permite som das notificações que forem exibidas.
- `system_alerts_enabled`: permite alertas da categoria sistema.

Alertas críticos ainda precisam ser definidos por política de produto; quando
existirem, a regra deverá declarar explicitamente quais preferências eles podem
ignorar.

## Persistência

O `FlashCoordinator` grava um `notification_profile_t` em duas gerações NVS,
com cabeçalho e CRC. O schema de dados offline não participa desse registro.
Na inicialização, o serviço restaura a geração válida mais nova. Sem perfil
gravado, os três valores começam ativos.

Mudanças próximas são consolidadas pelo serviço por 500 ms. A confirmação de
gravação é observada pelo status do `FlashCoordinator`; até ela ocorrer,
`persistence_pending` fica ativo na projeção. Assim nenhum callback LVGL grava
flash e a UI só informa “preferências salvas” após a confirmação.

## UI

Settings mostra o estado projetado nos três switches e na linha resumida
`Ativadas` ou `Silenciadas`. O ícone de sino do header usa o estado geral. Uma
falha de submissão restaura o switch e mostra toast de erro. As mensagens de
som e alertas do sistema são específicas, em vez de reutilizar o texto de
notificações gerais.

`np_feedback` mantém timers separados para toast e OSD, e expõe os métodos de
ocultar cada elemento e todos os elementos. Ele só apresenta feedback; não
contém regra de entrega de notificação.

## Próximos incrementos

- Definir eventos de domínio, prioridade, categoria, deduplicação, TTL e
  limite de fila no serviço.
- Integrar rede, storage, sincronização, OTA e diagnóstico publicando eventos
  no serviço, sem chamar LVGL.
- Definir toast para evento transitório, banner para condição persistente e
  OSD para alterações externas de brilho e volume.
- Substituir a demonstração em `np_notifications.c` por histórico real,
  não-lidas, marcar como lida e limpar.
- Executar a matriz física: reboot com preferências persistidas, três switches,
  transições online/offline, falha e recuperação de storage, OTA e soak sem
  WDT.
