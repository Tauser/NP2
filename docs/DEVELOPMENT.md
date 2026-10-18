# Desenvolvimento local

## Ambiente suportado

Use VS Code com a extensão **Espressif IDF**, ESP-IDF **5.5.4** e a toolchain
instalada pela própria extensão/instalador Espressif. Para comandos, monitor e
depuração da extensão, abra `D:\Projetos\NP2\firmware` como a pasta do
workspace: é onde está o `CMakeLists.txt` do projeto ESP-IDF. A raiz
`D:\Projetos\NP2` pode continuar aberta em outra janela para documentação e
tarefas compartilháveis, mas não é um projeto ESP-IDF diretamente.

Para manter as duas visões na mesma janela, prefira abrir
`D:\Projetos\NP2\NP2.code-workspace`. Ele inclui a raiz do repositório e
`firmware/` como pastas separadas, com `firmware/` primeiro; a extensão
Espressif e CMake Tools devem usar a primeira para descobrir o projeto P4.

No VS Code, selecione a instalação ESP-IDF 5.5.4 e abra um terminal ESP-IDF.
Não versionar caminhos como `C:\esp\...`: cada desenvolvedor configura o seu
próprio ambiente na extensão. As tarefas `NP2: Build P4` e
`NP2: Verify partition table` pressupõem esse terminal preparado.

Confirme no terminal:

```text
idf.py --version
idf.py --list-targets
```

O primeiro comando deve indicar 5.5.4; o segundo deve listar `esp32p4` e
`esp32c6`.

## Primeiro build do P4

```text
cd firmware
idf.py set-target esp32p4
idf.py fullclean
idf.py partition-table
idf.py build
```

O arquivo `firmware/dependencies.lock` já está versionado e faz parte do
baseline. Preserve os pins e hashes; qualquer nova resolução que altere o lock
precisa de revisão e da decisão correspondente antes de ser incorporada.
`sdkconfig` e `managed_components` são saídas locais ignoradas. Confirme que o
`sdkconfig` efetivo preserva os valores de `sdkconfig.defaults` antes de
prosseguir para a placa.

O firmware atual possui fechamentos G0–G4 com escopo documentado; G5 continua
em andamento. O [roadmap](ROADMAP.md) reúne as entregas e pendências atuais.
Antes de qualquer nova gravação, confira a fase afetada,
o hash da imagem e as evidências pendentes; build verde não substitui bancada.

## Verificações das entregas recentes

Na raiz do repositório, em PowerShell:

```powershell
./tools/run_settings_system_ui_host_test.ps1
./tools/run_coingecko_quota_host_test.ps1
python tools/coingecko_admission_host_test.py
python tools/coingecko_storage_host_test.py
./tools/run_app_event_bus_host_test.ps1
```

O teste de Sistema compila a tela real, as fontes e LVGL 9.5.0, incluindo
rollover de CPU, dados ausentes, histórico limitado, manutenção e 100 ciclos
de navegação. Requer LVGL resolvido em `firmware/managed_components/`.
O script de Sistema usa caminhos locais de CMake, Ninja e GCC; os demais
testes exigem GCC disponível conforme seus scripts. Confira a instalação e o
PATH antes de executá-los em outra máquina. As saídas ficam em
`firmware/build/` ou no diretório temporário indicado pelo script, fora do Git.
Critérios físicos,
comandos e hashes do P4 estão em [Sistema](SYSTEM-RESOURCES-VALIDATION.md) e
[CoinGecko](COINGECKO-QUOTA.md).

Um `sdkconfig` gerado anteriormente deve ser conferido explicitamente:
`sdkconfig.defaults` contém agora as estatísticas FreeRTOS com ESP Timer
exigidas pela coleta de CPU (ADR-093). A receita com diretório de build e
SDKCONFIG separados está no registro de Sistema. Preserve target `esp32p4`,
três framebuffers e auto-suspend desabilitado.

Para o gabinete, comece pelo [índice mecânico](../mechanical/README.md) e pelo
guia de impressão V11. Scripts em `tools/cad/` que importam `adsk` executam no
Fusion; as verificações geométricas registradas não substituem o protótipo.

## C6

Construa o exemplo oficial do ESP-Hosted 3.0.6 depois que ele for resolvido no
P4. Use o perfil em `coprocessor/sdkconfig.defaults`, aplique o patch oficial
do IDF e registre a configuração efetiva. A imagem precisa caber no staging
`c6_ota` de 2 MiB; o arquivo medido no guia excede 1 MiB.

## Regras de configuração

- Nunca habilite auto-suspend de flash nesta placa.
- Não habilite `ESP_HOST_WIFI` junto de Wi-Fi remoto.
- Não altere layout de partições após existir unidade de campo.
- Antes de flash, siga o roteiro de evidência em
  `RESTART-HARDWARE-BRINGUP.md` e os gates do plano.
