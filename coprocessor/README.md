# Imagem do coprocessador ESP32-C6

O C6 é construído a partir de `examples/ota/coprocessor_ota/cp` do
**ESP-Hosted 3.0.6** com ESP-IDF 5.5.4, não a partir desta pasta. Esta pasta
versiona somente o perfil mínimo e a receita que precisa acompanhar o firmware
P4.

Antes do build, aplique o patch oficial e idempotente:

```text
python <esp_hosted>/tools/eh.py patch-idf --idf-path <esp-idf-5.5.4>
```

O patch remove o teto de 4092 bytes de `sdio_slave.c`, necessário para SDIO
SW_AGGR. Registre o hash do ESP-Hosted, o diff do patch e o hash da imagem C6
na evidência do teste. A configuração resultante deve incluir os símbolos de
[`sdkconfig.defaults`](sdkconfig.defaults).

O C6 é atualizado apenas por Slave OTA via SDIO. Não usar a USB conectada ao
P4 para tentar gravá-lo. O layout real de slots, Secure Boot e rollback do C6
ainda precisa de validação de bancada antes de qualquer OTA de campo.
