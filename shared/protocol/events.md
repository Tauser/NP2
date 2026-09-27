# Eventos de dados offline

| Evento | Payload | Regra |
|---|---|---|
| `offline_data.updated` | `offline_snapshot.v4` validado e limitado | O `app_loop` descarta resultado de geração de rede ou request antigo e publica somente a projeção sanitizada. |
| `offline_data.persist_failed` | domínio, código sanitizado e revisão | Mantém o dado em RAM; a UI mostra stale/erro, sem corpo HTTP ou segredo. |

Eventos não transportam JSON bruto, URL, header, SSID, senha ou ponteiro de
driver.
