# NP2

Firmware do smart panel premium baseado na Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B. O ESP32-P4 executa a aplicação; o ESP32-C6 opera
como coprocessador Wi-Fi por ESP-Hosted sobre SDIO.

O projeto iniciou a Fase 5 — atualização recuperável. As fases 0–4 têm
fechamentos com escopo e exclusões registrados no roadmap; o firmware possui
display/touch, conectividade e dados offline. G5 já inclui o caminho P4 de
laboratório para manifesto assinado, hash incremental, journal, slot inativo e
saúde local de boot; o ciclo HTTPS físico e a recuperação conjunta P4/C6 ainda
estão em validação.

## Estado consolidado — 18/10/2026

- Preferências > Sistema exibe CPU por core, histórico limitado, heap interno,
  PSRAM, temperatura, uptime e ocupação de LittleFS/SD. Implementação em
  `3ed6276`; build, flash e boot de 10/10 estão registrados em
  [SYSTEM-RESOURCES-VALIDATION.md](docs/SYSTEM-RESOURCES-VALIDATION.md).
- CoinGecko usa quota mensal persistente e consulta a cada 5 min, ou 20 min
  a partir de 80% do orçamento. Política, recuperação e limites em
  [COINGECKO-QUOTA.md](docs/COINGECKO-QUOTA.md).
- Gabinete atual: V11, com dutos de microfone removíveis. Modelos, histórico,
  pacotes de impressão e scripts CAD foram versionados em `57b3433`.
  Consulte o [índice mecânico](mechanical/README.md).

A nova tela ainda requer interação e medições físicas de render/SD. A margem
interna foi recuperada para a leitura Google em 20/10; a carga máxima e o
soak continuam em validação. O gabinete
aguarda fatiamento, impressão e ensaio de montagem/áudio. Essas entregas não
encerram G5, G6 ou G7 nem qualificam uma release.

## Agenda — 20/10/2026

A tela Agenda tem eventos e tarefas locais, edição, conclusão, exclusão
confirmada e lembretes no centro de notificações, com som. Limite inicial de
64 itens e eventos do mês corrente inteiro. A integração Google Calendar/Tasks tem autorização inicial por
computador e procedimento em [AGENDA-GOOGLE.md](docs/AGENDA-GOOGLE.md).
Conta vinculada, leitura real e ida/volta de evento criado na UI confirmadas
na placa; exclusão do teste retornou HTTP 204. Piso interno preservado e
pilha de rede corrigida. Medidas e limites em
[AGENDA-MEMORY-VALIDATION.md](docs/AGENDA-MEMORY-VALIDATION.md).
A admissão continua bloqueando Google quando a memória é insuficiente.

## Quadro de fotos — 22/10/2026

Preferências → Quadro de fotos oferece galeria de miniaturas e seleção das
fotos do cartão SD, prévia, entrada por inatividade de 30 s a 1 h e troca
de 10 s a 1 h. Voltar após permite retornar à página anterior em 30 s a 1 h,
ou manter Não sair. Bitcoin e data/hora usam widgets com fonte de 48/64 px.
As fotos ficam restritas ao quadro e à prévia manual.
Preparação do cartão, ajustes e evidência em
[PHOTO-FRAME-VALIDATION.md](docs/PHOTO-FRAME-VALIDATION.md).

## Referências do projeto

Configurações → Notificações oferece três melodias originais para cada
categoria (notificação, alarme e inicialização), prévias e silêncio individual.
Implementação e bancada em [SOUNDS-VALIDATION.md](docs/SOUNDS-VALIDATION.md).

- [Bring-up e restrições de hardware](docs/RESTART-HARDWARE-BRINGUP.md)
- [Plano de arquitetura e gates](docs/PLANO-FIRMWARE-PREMIUM.md)
- [Roadmap e estado das fases](docs/ROADMAP.md)
- [Execução e validação G5](docs/G5-VALIDATION.md)
- [Recursos do sistema e evidências](docs/SYSTEM-RESOURCES-VALIDATION.md)
- [Quota e cadência CoinGecko](docs/COINGECKO-QUOTA.md)
- [Agenda, tarefas, lembretes e configuração Google](docs/AGENDA-GOOGLE.md)
- [Margem de memória e TLS Google em bancada](docs/AGENDA-MEMORY-VALIDATION.md)
- [Gabinete e arquivos de impressão](mechanical/README.md)
- [Configuração de desenvolvimento](docs/DEVELOPMENT.md)
- [Instruções para colaboradores e agentes](AGENTS.md)

## Estrutura

- `firmware/`: aplicação ESP-IDF do P4.
- `coprocessor/`: configuração e receita da imagem C6; o fonte do C6 vem do
  exemplo oficial fixado de ESP-Hosted, não de uma cópia local não rastreada.
- `docs/`: decisões, procedimentos e evidências de bancada.
- `mechanical/`: gabinete, referências, modelos editáveis e pacotes de impressão.
- `tools/`: verificações reproduzíveis sem segredos.
  `tools/cad/` contém os scripts de construção e inspeção mecânica no Fusion.

O baseline é ESP-IDF 5.5.4, BSP Waveshare 3.0.1, LVGL 9.5.0,
`esp_lvgl_adapter` 0.6.4, ESP-Hosted 3.0.6 e `esp_wifi_remote` 1.6.4. A
configuração efetiva e os artefatos de P4 e C6 precisam ser registrados em
cada evidência de bancada.
