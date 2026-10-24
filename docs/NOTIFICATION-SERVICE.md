# Serviço de notificações

## Estado atual

O serviço controla as preferências globais de notificações e é a fonte de
verdade para a UI:

```text
Settings (intenção) → notification_service → FlashCoordinator
                                      ↓
                         app_loop / app_state projection → UI
```

`notification_service` controla as preferências e não chama LVGL, NVS, rede,
sensores ou drivers. A task LVGL envia intenções de preferência e consome a
projeção publicada por `app_loop`. O `app_loop` também mantém o histórico de
eventos reais em uma projeção limitada a oito itens; a UI envia as intenções de
marcar um item ou todos como lidos pela fila de eventos.

As preferências são valores não secretos:

- `general_enabled`: permite notificações não críticas.
- `sound_enabled`: master de som de notificações, alarmes e inicialização.
- `system_alerts_enabled`: permite alertas da categoria sistema.
- `melodies[3]`: escolha de notificação/alarme/inicialização, 0 para silêncio
  e 1–3 para as três melodias originais de cada categoria.

Alertas críticos ainda precisam ser definidos por política de produto; quando
existirem, a regra deverá declarar explicitamente quais preferências eles podem
ignorar.

## Persistência

O `FlashCoordinator` grava um `notification_profile_t` em duas gerações NVS,
com cabeçalho e CRC. O schema de dados offline não participa desse registro.
Na inicialização, o serviço restaura a geração válida mais nova. Sem perfil
gravado, os três switches começam ativos e as melodias são 1/1/1. O leitor
aceita o registro v1 de 3 B e o v2 de 6 B. Ao ler v1, mantém os switches e
aplica as melodias padrão sem escrever. v2 usa schema próprio 2, CRC e duas
gerações; firmware antigo exige cópia v1 íntegra para voltar a ler preferências.

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

## Feedback sonoro

Configurações → Notificações oferece três seletores com `Sem som` e três
melodias por categoria, cada qual com botão `Ouvir`. O preview passa pelo
master de som e volume; independe do switch de entrega de avisos gerais.
O serviço entrega a solicitação ao `device_control_service`; somente sua task
possui codec/I2S. Há três slots coalescidos, um por categoria, rate limit de
350 ms/categoria e um único som em reprodução. Alarmes podem interromper
notificação/boot na fronteira de bloco. Silenciar o master cancela a reprodução.

O gerador portável produz PCM16 mono/22.050 Hz em blocos de 256 samples,
sem arquivos, rede ou alocação por melodia. Controles/mute são atendidos
entre blocos. As nove composições originais têm ataque, decaimento e release;
inicialização é mais suave. Categorias e durações em
[SOUNDS-VALIDATION.md](SOUNDS-VALIDATION.md).

Avisos do sistema usam a melodia de notificações; Agenda e Pomodoro usam
a de alarmes, uma vez por disparo. Inicialização toca uma vez após sair do
boot e restaurar preferências/controles; trocar de página/reconectar não toca.
Alarmes e inicialização respeitam o master independentemente do switch de
avisos gerais. Agenda mantém seu próprio gate de entrega de lembretes.
São frases curtas; não há reprodução contínua ou soneca nesta implementação.

O preview ao soltar o slider de volume também é executado pela task de controle
de dispositivo, depois que o novo ganho foi aplicado. Ele não depende das
preferências de entrega de notificações, pois confirma uma alteração local do
controle de áudio. Com volume em zero, não há tom.

## Histórico de eventos

O header abre o centro de notificações sobre a cena atual. O sino mostra a
contagem de não lidas e a tela lista os eventos mais recentes, com os novos no
topo. Abrir um item o marca como lido; “Marcar todas como lidas” faz o mesmo
para os demais. Itens lidos permanecem no histórico até serem expulsos pela
capacidade fixa de oito eventos. Esse histórico é volátil e começa vazio após
reinício.

Cada item mostra título, explicação curta, idade do evento e estado visual de+leitura. A idade usa o horário do próprio evento e o relógio projetado pelo+`app_loop`: `Agora`, `há N min`, `há N h` ou `há N d`. Se o evento não tiver um
horário válido, a UI diz `Horário indisponível` em vez de inventar uma data. O
resumo usa contagem legível, como `1 nova | 3 no histórico`.

O histórico deve conter ocorrências relevantes para a pessoa: falhas, alertas,
recuperações e sucessos pontuais. Respostas normais de consulta e atualizações
rotineiras dos provedores não geram notificações.

Fontes atualmente projetadas pelo `app_loop`:

- Rede desconectada/restabelecida, quando há credenciais de estação ativas.
- Erro/recuperação do armazenamento e falha ao solicitar reinício.
- Lembretes de eventos e tarefas da [Agenda](AGENDA-GOOGLE.md), com título,
  toast e chime sob as preferências gerais/som e volume. A avaliação acontece
  no `app_loop`, somente com hora confiável; o título é dado do usuário e não
  é registrado em logs.

Eventos de rede respeitam notificações gerais; erros e recuperações do sistema
respeitam alertas do sistema. Uma recuperação é exibida como sucesso porque
representa a resolução de uma condição anterior, não uma atualização periódica.
O header Wi-Fi navega para a configuração da rede.

## Próximos incrementos

- Integrar sincronização, OTA e diagnóstico ao histórico onde existirem
  transições/resultados confiáveis, sem chamar LVGL.
- Definir toast para evento transitório, banner para condição persistente e
  OSD para alterações externas de brilho e volume.
- Definir persistência opcional do histórico após reboot e política explícita
  para remoção manual dos itens antigos.
- Executar a matriz física: reboot com preferências persistidas, três switches,
  transições online/offline, falha e recuperação de storage, OTA e soak sem
  WDT.
