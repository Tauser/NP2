# Limite mensal da CoinGecko

Política solicitada pelo responsável em 07/10/2026, registrada no ADR-092.

| Consumo local reservado | Intervalo mínimo de consulta |
|---|---|
| Abaixo de 8.000/10.000 | 5 min |
| A partir de 8.000/10.000 | 20 min |
| Orçamento esgotado, sem reservas em RAM | Bloqueado até o próximo mês UTC confiável |

Outubro/2026 parte de 8.010 créditos, conforme o dashboard fornecido. Na
primeira consulta, o firmware reserva oito tentativas e confirma 8.018 no
journal local; o dashboard da conta cobra apenas o request efetivamente
respondido com HTTP 200. O contador local inclui falhas e reservas ainda
não utilizadas, para evitar subcontagem em timeout/corte de energia. Pode
entrar na política de 80% até sete consultas antes do limiar real.

O limite passa pelo único helper autenticado, portanto refresh automático,
manutenção e reconexão não têm uma rota de HTTP que ignore a admissão.
Pedidos antecipados preservam o valor e sua idade, sem marcar falha de
provider. A expiração normal dos dados continua sob responsabilidade da
projeção; o cache não é apresentado como recém-atualizado pela recusa.

O registro NVS `cg_quota/budget_v1` tem 24 bytes: magic/schema, geração,
mês, créditos reservados, final da janela reservada e CRC32. Somente o
FlashCoordinator grava, confirma e verifica o registro, antes de qualquer
HTTPS da CoinGecko. Não há escrita por toque, chave/URL em logs ou contador
de consumo no AppState. A reserva reutiliza o buffer de submissão existente,
sem adicionar outra cópia de 4 KiB à pilha do worker de rede.

Oito tentativas separam gravações em pelo menos 40 min de funcionamento
contínuo no modo normal e 160 min no reduzido. No reboot, reservas pendentes
são perdidas sem devolver créditos; o mercado aguarda o final da janela
registrada, que pode estar até 40/160 min no futuro. O contador não volta a
zero no reboot, na reconexão ou ao atrasar o relógio. Corrupção, NVS
indisponível, falha de commit/verify ou hora não confiável impedem HTTP da
CoinGecko. Outros providers permanecem ativos.

Não há sincronização com consumo de outros clientes usando a mesma chave,
nem com consultas feitas entre o screenshot e a instalação desta política.
O teto protege os requests desta instalação a partir do piso informado.
Apagar NVS ou retornar a um firmware anterior que não aplica esta admissão
fica fora dessa proteção. Não apagar NVS para corrigir corrupção.

## Verificação reproduzível

Executar na raiz do repositório:

```powershell
./tools/run_coingecko_quota_host_test.ps1
python tools/coingecko_admission_host_test.py
python tools/coingecko_storage_host_test.py
./tools/run_app_event_bus_host_test.ps1
```

Testes host passaram com C11, `-O2 -Wall -Wextra -Werror -pedantic`: budgets
completos de outubro (1.990 tentativas restantes) e mês novo (10.000),
transição de 80%, intervalos normais/reduzidos, saldo parcial final,
reinício, mês regressivo, virada de ano, CRC e limites numéricos.
Os testes de integração compilam as funções reais de admissão/HTTP e
armazenamento extraídas dos serviços: não enviar HTTP antes do ACK durável,
recusar consulta manual antecipada, contar falha, não reutilizar bloco após
reboot/ACK incerto, falhas de set/commit/verify e geração obsoleta.
Também passaram o teste do CRC/cache e a regressão do EventBus com 8.000
snapshots concorrentes.

## Build e bancada

Build limpo concluído com ESP-IDF 5.5.4/esp32p4, base Git
`baf64c4` mais as alterações locais desta entrega. App `0x34ca40`
(3.459.648 B), 59% livres no slot de 8 MiB. Dependências/lock, defaults e
partições permanecem iguais. Não houve warnings de compilação C/C++.
O configure informou falta de `ESP_ROM_ELF_DIR` para gerar o gdbinit opcional;
isso não impediu compilação, validação de tamanho ou geração da imagem.

| Artefato | SHA-256 |
|---|---|
| App P4 | `E74591B2F1686751956A4D01189793EE0D380D7058F9C5455D4DD799025C4EBC` |
| ELF P4 | `51794916504B48E6D934406D106DF2A033A2ECE76EE05C8953DAAB55644A8CE2` |
| sdkconfig | `2524F49D41654D1CBA9137177B4E979726865B622A43965DBBF71C2C4DEF67B8` |
| Tabela binária | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| dependencies.lock P4 | `6F605BE935C36425D451A65EC2A700AEF5878D9DAB7EA1000C2721199C552707` |

Configuração efetiva: P4 v1.x, flash/PSRAM 32 MiB, três framebuffers,
LVGL task 16 KiB, TLS interno/RX 8 KiB, AES por software, auto-suspend
desligado, reset C6 GPIO54 ativo baixo, SDIO CLK18/CMD19/D0..D3 14..17.
Sdkconfig e tabela binária são idênticos aos da imagem anterior.

Comandos (a partir de `firmware/`, com ambiente IDF exportado):

```powershell
idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/coingecko-quota-20261007/sdkconfig -B build/coingecko-quota-20261007 build
idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/coingecko-quota-20261007/sdkconfig -B build/coingecko-quota-20261007 -p COM8 -b 460800 app-flash
python build/coingecko-quota-20261007/capture_quota_boot.py build/coingecko-quota-20261007/boot-sanitized.log 150
```

Gravação somente do app P4 em `0x20000`, 3.459.648 B, hash verificado
pelo esptool. O boot identificou app `517949165...`, execução em `ota_0`,
P4 revisão 103 (v1.3), PSRAM 32 MiB. Placa Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B da bancada habitual; PCB/lote/BOM exatos não
foram reinspecionados. Hash do C6 instalado não obtido; C6 não atualizado.
Os logs/artefatos ficam em `firmware/build/coingecko-quota-20261007/`,
ignorados pelo Git. Captura sanitizada sem envio de comandos à placa.

O boot negociou RPC V2 e SDIO SW_AGGR e chegou a `P4 local bring-up ready`.
Aos 27.473 ms de uptime, o coordenador confirmou
`month=202610 reserved=8018/10000 generation=1`; aos 27.483 ms o worker
admitiu CoinGecko com `interval=1200s`. Aos 29.213 ms a consulta terminou
HTTP 200, 3.993 B, 1.738 ms de HTTPS, snapshot aceito com mercado fresco.
Todos os cinco domínios terminaram com sucesso (sete HTTP 200, incluindo
três índices). A captura de 150 s terminou com 47 linhas sanitizadas e COM8
fechada. A amostra curta não apresentou panic/WDT nem outra consulta
CoinGecko no antigo intervalo de 60 s. O piso de memória do projeto continua
aberto: nessa consulta havia 67.239 B internos antes e 43.347 B com cliente
ativo ao final do corpo; esses pontos não medem o pior pico do handshake.

Não se considera encerrado G2/G3/G4/G6 por testes host ou boot isolado;
cortes de energia reais, intervalo completo de 20 min, rollover real,
filmagem de render e soak ainda exigem observação específica.
