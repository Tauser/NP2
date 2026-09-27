# CredentialVault — ativação de produção

**Estado:** preparação implementada; não ativada em hardware.

Este procedimento transforma a retenção Wi-Fi em uma propriedade de produto. Ele
não registra senhas, chaves privadas, saídas de eFuse ou artefatos assinados no
repositório. Use um AP de laboratório descartável durante todos os ensaios.

## O que a imagem faz

O driver Wi-Fi continua com `WIFI_STORAGE_RAM`. Depois de 30 segundos
contínuos com IP, o worker de conectividade envia a credencial para a fila do
`FlashCoordinator`. O cofre mantém duas gerações com CRC no namespace NVS
`np2_credentials`. No boot, só uma cópia privada segue para a mailbox da
conectividade. Não há getter para UI, estado global, eventos, logs ou console.

Uma senha candidata rejeitada é descartada e preserva a última rede
confirmada. `FORGET` remove as duas gerações persistentes e a configuração
ativa do driver.

O cofre recusa todas as operações quando NVS Encryption não está compilada ou
quando Flash Encryption não está ativa no P4. Assim, uma imagem de bancada
comum permanece sem retenção de senha.

## Perfil de desenvolvimento com retenção

Para evitar novo provisionamento em cada reboot na P4 desbloqueada usada pelo
fluxo normal de desenvolvimento, a opção
`NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION` tem padrão `ON`. Ela conserva o
mesmo cofre, mailbox privada, duas gerações CRC, espera de 30 segundos e
limpeza por `FORGET`, mas usa a NVS local sem a proteção física de produção.
Usar somente uma rede e credencial de laboratório. Esse perfil não grava
chaves, não muda a tabela de partições e não toca em eFuses; a transição para
produção exige limpar/provisionar a credencial outra vez pelo perfil protegido.

O pipeline de produção deve passar explicitamente
`-DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=OFF`; não pode depender do
padrão de desenvolvimento deste repositório.

## Perfil de segurança exigido

Preparar uma imagem de produção separada, com chaves mantidas fora deste
repositório, e confirmar no `sdkconfig` efetivo:

- `CONFIG_SECURE_BOOT_V2_ENABLED=y`, com assinatura RSA-3072 protegida por
  processo de custódia externo;
- `CONFIG_SECURE_FLASH_ENC_ENABLED=y`;
- `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y`;
- `CONFIG_NVS_ENCRYPTION=y`;
- `CONFIG_NVS_SEC_KEY_PROTECT_USING_FLASH_ENC=y`.

A tabela `firmware/partitions.csv` já reserva `nvs_keys` como `encrypted`.
A partição `nvs` em si não recebe essa flag: o NVS cifra seu conteúdo usando a
chave protegida em `nvs_keys`.

Não usar o modo Development de Flash Encryption: o próprio ESP-IDF o define
como inadequado para produto. Não reutilizar a chave de assinatura em outra
unidade, não imprimir hashes de chaves, e não adicionar chaves ou credenciais
à árvore de trabalho.

## Gates antes da ativação irreversível

1. Fechar os gates de OTA/recovery P4 e C6 previstos em
   `PLANO-FIRMWARE-PREMIUM.md`, inclusive rollback e recuperação após corte.
2. Usar uma placa de qualificação dedicada, identificada na evidência, nunca a
   única unidade de desenvolvimento.
3. Fazer build limpo do perfil, registrar commit, hash dos binários P4/C6,
   tabela de partições, `sdkconfig` efetivo sem segredos e resultado de size.
4. Ensaiar a recuperação assinada e o rollback antes de permitir a queima de
   eFuses. Confirmar rota de atualização P4 e Slave OTA do C6.
5. Executar leitura de eFuses somente para conferência e revisar a sequência
   de flash completa gerada pelo ESP-IDF. A aprovação humana para o passo de
   eFuses precisa ser específica para a placa e para este perfil Release.

## Ativação e evidência

Com os gates aprovados e a autorização explícita, seguir a sequência produzida
pelo ESP-IDF para o perfil assinado: gravar a tabela e os artefatos compatíveis,
permitir a ativação de Secure Boot/Flash Encryption pelo fluxo oficial e
capturar o boot. Não há rota USB para atualizar diretamente o C6; ele continua
exclusivamente por Slave OTA via SDIO.

Registrar em `BRINGUP-EVIDENCE.md`: data, placa/BOM, commit, hashes P4/C6,
configuração efetiva redigida, comandos, resultado da verificação de segurança
e os trechos de boot relevantes. Nunca registrar SSID, senha, material de chave
ou o dump completo de eFuses.

## Critérios de aceitação

1. Provisionar WPA2 pelo painel e obter IP; aguardar 30 segundos.
2. Reiniciar a frio e a quente: o painel reassocia sem intervenção, sem senha
   em log ou UI.
3. Tentar uma senha de substituição inválida: a rede confirmada continua no
   cofre e a tentativa falha sem loop.
4. Confirmar uma nova rede por 30 segundos: ela substitui a geração anterior.
5. Executar `FORGET`, reiniciar e comprovar ausência de associação automática.
6. Repetir os casos 2 a 5 após corte de energia durante a gravação. Ao menos
   uma geração CRC válida deve sobreviver ou o dispositivo deve iniciar sem
   credencial, nunca com registro corrompido.
7. Reexecutar a campanha de OTA/recovery prevista para garantir que o perfil
   Release ainda atualiza e recupera a unidade.

Uma falha em qualquer critério mantém a ativação aberta. O build ou a foto de
um boot isolado não a concluem.
