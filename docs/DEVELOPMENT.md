# Desenvolvimento local

## Ambiente suportado

Use VS Code com a extensão **Espressif IDF**, ESP-IDF **5.5.4** e a toolchain
instalada pela própria extensão/instalador Espressif. Abra `D:\Projetos\NP2`
como a pasta raiz do workspace. As recomendações de extensões e tarefas
compartilháveis estão em `.vscode/`.

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

O gerenciador de componentes deve gerar `firmware/dependencies.lock` na
primeira resolução. Revise-o e o adicione ao Git: ele é parte do baseline.
`sdkconfig` e `managed_components` são saídas locais ignoradas. Confirme que o
`sdkconfig` efetivo preserva os valores de `sdkconfig.defaults` antes de
prosseguir para a placa.

O scaffold atual não deve ser gravado nem testado na placa como se fosse
bring-up. Primeiro feche a verificação de partições, dependências e tamanho.

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
