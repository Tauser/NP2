# Serviço de notificações

**Estado:** implementado para preferências de sessão; persistência e interface
de Settings permanecem etapas posteriores.

`notification_service` possui uma task própria e uma mailbox coalescida por
notificação de tarefa. Ele controla três preferências: notificações gerais,
som e alertas do sistema. Todas começam ligadas. Alterações repetidas preservam
somente o último valor e incrementam uma geração observável.

O serviço não chama LVGL, NVS, rede, sensores ou drivers. A UI deve enviar
somente as intenções públicas `notification_service_set_*`; persistência futura
passará pelo `FlashCoordinator`, nunca por callback de toque. O componente
`np_feedback` continua separado: ele exibe toast, OSD ou banner e não contém
regra de negócio.

Próximos passos: projetar o status no `app_loop`, ligar os toggles do modal de
Notificações e validar em placa alterações rápidas sem watchdog.
