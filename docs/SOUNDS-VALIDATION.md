# Melodias do NovaPanel — 21/10/2026

Implementadas nove composições originais locais, selecionáveis em
Configurações → Notificações, com três botões Ouvir e silêncio por categoria.
Volume permanece em Tela e som. Não há arquivos de música, downloads,
decodificador, dependências novas ou rede para áudio.

| Categoria | Melodias / duração aproximada |
|---|---|
| Notificação | Cristal 309 ms, Gotas 299 ms, Dueto 439 ms |
| Alarme | Aurora 1549 ms, Sinos 1459 ms, Pulso 1414 ms |
| Inicialização | Boas-vindas 889 ms, Ascendente 929 ms, Minimal 500 ms |

Padrões: Cristal/Aurora/Boas-vindas. Variante 0 significa Sem som. A síntese
PCM16 mono a 22.050 Hz tem LUT de seno interpolada, harmônico a 1/8,
ataque/decaimento/release e pico medido de 6.308/32.767 no caso mais alto.
Startup usa amplitude menor. São frases finitas, sem reprodução contínua.

## Integração e limites

O codec ES8311/I2S mantém seu dono em `device_control_service`, sample rate
e ganho existentes. O worker gera/escreve até 256 samples (512 B de pilha)
por passagem. Enquanto há reprodução, o I2S regula o ritmo; os controles,
volume, mute, status e persistência continuam sendo atendidos entre blocos.
Sem alocação por som ou nova task. Há três slots coalescidos, um por categoria,
rate limit de 350 ms/categoria e prioridade alarme/notificação/boot. Alarme
pode interromper outra categoria entre blocos. Master off cancela; volume
zero também limpa a reprodução. Preview do slider de volume conserva Cristal.

`notification_service` guarda as escolhas e aplica a política. Seu master
de som controla as três categorias, inclusive prévias. A entrega de avisos
gerais controla notificações; alarmes/Pomodoro e boot independem desse switch,
respeitando o master, a categoria e o volume. A Agenda conserva seu gate de
entrega. Avisos do sistema usam notificação; lembretes e Pomodoro usam alarme.
Startup é solicitado uma vez após sair da cena de boot, quando preferências
e controles estão restaurados. Não toca ao navegar, reacender ou reconectar.

Registro de notificações v2: seis bytes, schema próprio 2, duas gerações NVS
e CRC. Leitor aceita v1 de três bytes, preserva switches e aplica 1/1/1 sem
escrever. Seleções passam pelo debounce e ACK do coordenador de flash.
Firmware antigo não lê v2; rollback requer preservar cópia v1 íntegra.
Sem alteração de partições, Kconfig, vault, rede, C6 ou pipeline de display.

## Testes reproduzíveis

- `tools/run_sound_melody_host_test.ps1`: nove PCM com igualdade entre
  chunks de 1 e 1024 samples, ganho/pico/DC, duração limitada, início/fim
  zero, silêncio e índices inválidos. Produtor/consumidor reais de áudio
  extraídos: limite de chunk, prioridade, rate limit, cancelamento, volume
  zero e falha de codec passaram. Rotinas NVS reais com backend simulado:
  v1/defaults, v2, commit falho e fallback por CRC passaram.
- `tools/run_notification_preferences_host_test.ps1`: restauração, escolha
  de melodias, edição durante write em voo, coalescing, ACK, erro, silêncio
  por categoria/master e preview passaram.
- `tools/run_sounds_ui_host_test.ps1`: seletores/prévias, dropdowns, limites
  de área, glyph de relógio e 100 lifecycles LVGL sem perda passaram.
  Render `firmware/build/sounds-ui-host-20261021/sounds-20261021.png` inspecionado.
  O render usa fixtures; não é uma foto da placa.
- Regressão `tools/run_google_agenda_sync_host_test.ps1` (inclui storage,
  vault e mailbox) e `tools/run_app_event_bus_host_test.ps1` (8.000 snapshots)
  passaram. `git diff --check` sem erros.

Os `.pcm`, `.wav` e a prévia `previa-novapanel.wav` são gerados pelo mesmo
renderizador portável em `firmware/build/sounds-host-20261021/`. A prévia
encadeia inicialização/notificação/alarme com pausas de 500 ms. Artefatos
locais ignorados; não incluem gravação física de microfone.

## Build e bancada

Base Git `777abde7069c91cfb6aeb915891cf00bc249f9df` e working tree modificado.
Diretório novo `firmware/build/sounds-20261021`, ESP-IDF 5.5.4, `esp32p4`,
build final sem warnings. App 3.517.680 B (`0x35acf0`), 58% livres no slot
de 8 MiB. Dados internos estáticos 63.800 B (+40); BSS externa 133.320 B
(+16) sobre a versão mensal. Pool LVGL e credenciais conservam suas regiões;
nenhuma nova reserva de PSRAM para áudio.

| Artefato | SHA256 |
|---|---|
| P4 bin | `9CDA778B0F150B872A80EA26349CC9099AB0A50780689E1DD1A108D252DF137A` |
| ELF | `7E5FAAAEBD98D48ADF45CAA91C1001EF06A74A542AB4AFFC5839054AE636E056` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições geradas | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto de 66 fontes públicos, `sources.sha256` | `E8FA08AA06932F8AE52A9ECCC2A6B50113F4D855FBD260682749D0B2CF9F5F05` |

Mesma Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3/revision 103, flash/PSRAM
32 MiB. App-flash COM8/460800 somente em `0x20000`, hash verificado.
Configuração e tabela de partições idênticas por hash. Logs locais ignorados:
`build.log`, `build-final.log`, `app-flash.log` e `boot-sanitized.log`.

Boot restaurou quatro itens de Agenda e controles existentes. Codec concluiu
startup `category=2 result=0` em 22,7 s e notificação `category=0 result=0`
em 25,5 s. Google real: token/Tasks/Calendar HTTP 200 e resultado 0. Mínimo
interno 120.883 B, maior bloco 102.400 B, pilha de rede mínima livre 4.192 B,
app_loop 2.280 B e integridade 1. C6 reportou 3.0.6; não regravado e hash
instalado desconhecido. Sem mudança do volume salvo para forçar o teste.
Coleta encerrada após 100 s e COM8 liberada; em 93,8 s, quatro itens,
`pending=0 linked=1 sync_busy=0 sync_result=0`, sem fault capturado.

Não houve audição instrumental, gravação de saída analógica ou ensaio de
ganho/alto-falante em gabinete. Reprodução de alarme e persistência v2 foram
exercitadas no host; codec físico foi observado para startup/notificação.
Não há qualificação de clipping analógico, qualidade subjetiva, continuidade
durante erase/GC, power cut, tearing ou soak a partir dessa captura.
