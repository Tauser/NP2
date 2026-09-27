# Imagem do coprocessador ESP32-C6

O C6 é construído com ESP-IDF 5.5.4 a partir da recipe versionada nesta pasta.
Ela preserva o bootstrap do exemplo oficial
`examples/ota/coprocessor_ota/cp` do **ESP-Hosted 3.0.6**, sem copiar o
componente Hosted. Os arquivos de origem do exemplo que determinam a recipe
estão em `main/`, `upstream/` e `partitions_eh_cp_ota_4m.csv`.

O lock P4 fixa `espressif/esp_hosted` 3.0.6 com hash do registry
`1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879`.
Esse hash identifica a fonte da receita; o hash da imagem C6 e o diff do IDF
continuam artefatos obrigatórios de cada build C6.

Antes do build, aplique o patch oficial e idempotente:

```text
python <esp_hosted>/tools/eh.py patch-idf --idf-path <esp-idf-5.5.4>
```

O patch remove o teto de 4092 bytes de `sdio_slave.c`, necessário para SDIO
SW_AGGR. Registre o hash do ESP-Hosted, o diff do patch e o hash da imagem C6
na evidência do teste. A configuração resultante deve incluir os símbolos de
[`sdkconfig.defaults`](sdkconfig.defaults).

## Build reproduzível

Em terminal ESP-IDF 5.5.4, a partir da raiz do repositório:

```powershell
./tools/build_c6.ps1 `
  -ReferenceSource coprocessor/build/reference/sdio_slave.original.c `
  -BuildName c6-repro-<data-hora>
```

Antes do script, baixe a versão crua de `sdio_slave.c` da tag oficial
ESP-IDF 5.5.4 para `coprocessor/build/reference/sdio_slave.original.c`. O
arquivo é ignorado porque é uma cópia de terceiro; o script confere seu
SHA-256 antes de usá-lo. Ele compara o arquivo inteiro do SDK instalado,
aplica duas vezes o `eh.py patch-idf` oficial e falha se houver qualquer
diferença além da única guarda SDIO permitida. O diff e o relatório auditável
ficam em `patches/`.

O build versiona `dependencies.lock`, mas mantém `build/`, `sdkconfig` gerado,
ELF, binários e logs ignorados. O relatório produzido inclui hashes de app,
bootloader, tabela, lock e configuração, além dos slots reais validados.

Para a prova de reprodução, faça dois builds com `BuildName` diferentes e
compare os relatórios:

```powershell
python tools/verify_c6_reproducibility.py `
  --left coprocessor/build/<primeiro> `
  --right coprocessor/build/<segundo>
```

O C6 é atualizado apenas por Slave OTA via SDIO. Não usar a USB conectada ao
P4 para tentar gravá-lo. O layout real de slots, Secure Boot e rollback do C6
ainda precisa de validação de bancada antes de qualquer OTA de campo. Portanto,
o script deliberadamente não oferece `flash`: a COM8 pertence ao P4 e esta
imagem não substitui o rádio funcional até o gate de recuperação ser fechado.
