# Imagem do coprocessador ESP32-C6

O C6 é construído a partir de `examples/ota/coprocessor_ota/cp` do
**ESP-Hosted 3.0.6** com ESP-IDF 5.5.4, não a partir desta pasta. Esta pasta
versiona somente o perfil mínimo e a receita que precisa acompanhar o firmware
P4.

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

Receita reproduzível, após resolver a fonte fixada pelo lock:

```text
cd <esp_hosted-3.0.6>/examples/ota/coprocessor_ota/cp
idf.py set-target esp32c6
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32c6;<repo>/coprocessor/sdkconfig.defaults" build
```

Arquive o `sdkconfig` efetivo, `dependencies.lock`, SHA-256 de
`build/network_adapter.bin`, `idf.py --version`, hash/diff do IDF patchado e o
hash do componente acima. Sem esse conjunto, o pacote C6 permanece pendente.

O C6 é atualizado apenas por Slave OTA via SDIO. Não usar a USB conectada ao
P4 para tentar gravá-lo. O layout real de slots, Secure Boot e rollback do C6
ainda precisa de validação de bancada antes de qualquer OTA de campo.
