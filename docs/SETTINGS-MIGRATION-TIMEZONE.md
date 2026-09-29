# Fuso horário e teclado global — migração em 29/09/2026

Arquivos de UI finalizados: `firmware/main/ui/core/np_keyboard.c/.h`,
`firmware/main/ui/screens/settings/np_settings_timezone.c/.h`. Integração:
`product_ui.c`, `np_tokens.h` e fontes Material 24/48. Catálogo/indexação e
verificação CMake estão no commit independente `50a39d4`; testes e este
relatório completam a entrega. A antiga `np_settings.c` não foi removida.

## Entrega

`np_settings_timezone.c/.h` já existiam como modal; foram ampliados com uma
tela própria mantendo o legado compilável. `np_keyboard.c/.h` continuam o
componente global pertencente a `product_ui_state_t`, com teclado LVGL
oficial, mapas originais, cores do template e símbolos nas fontes.

Fluxo: engrenagem → Perfil → Preferências → Fuso horário. Voltar retorna ao
hub. A Home não foi redesenhada. Primeiro acesso constrói apenas esta cena;
reentradas reutilizam a árvore. Saída esconde teclado, limpa target/foco,
cancela reconciliação async e esconde o drawer/cena. O cache de Sistema
preexistente permanece independente; nenhum modal eager foi introduzido.

Layout de dois painéis baseado no modelo: busca e seis rows visíveis à
esquerda; IANA selecionada, offset UTC padrão, estado NTP real e Aplicar à
direita. O header compartilhado usa informações reais do painel. Nomes IANA
e regras vêm do catálogo existente; países são metadados estáticos IANA
documentados em `TIMEZONE-CATALOG-INDEX.md`. Não copiar dados fictícios da
imagem. O offset é identificado como padrão por não inferir DST atual.

## Limites e lifecycle

- 462 registros imutáveis, array de 462 índices filtrados, viewport_start.
- Pool de sete rows para seis posições visíveis; scroll não cria/apaga objetos.
- Textos das rows usam buffers fixos e `lv_label_set_text_static` em CLIP.
  Filtro usa buffers de stack limitados, sem malloc próprio por tecla.
  A textarea mantém o gerenciamento normal de texto interno do LVGL.
- Sem novos timers. Busca síncrona sobre dados em flash com lookup indexado,
  sem serviço, driver, rede ou NVS no callback.
- 103 objetos LVGL por cena, incluindo header, independentemente de filtro.
- Construção e reentrada registram callbacks uma vez em endereços estáveis.
- Teclado tem oito bindings; DELETE devolve slots. Rebind verifica todos os
  campos antes de usar um slot vazio. Destroy remove callbacks de campos vivos.
- DEFOCUSED usa lv_async_call. Reconciliação retargeta campo registrado visível
  com foco; interação do teclado mantém sessão; ausência de target esconde.
  Hide/clear/destroy cancelam o async antes de destruir/inativar targets.

## Aplicar e persistir

Tocar numa row altera somente o rascunho. Aplicar reutiliza o callback em
product_ui para `onboarding_service_request_timezone_update`, seguido de
refresh. app_loop aplica a política em time_service e publica a projection.
FlashCoordinator continua serializando a persistência existente com CRC e
duas gerações. Nenhum schema, NVS ou serviço novo foi criado.

Rascunho novo durante a publicação do pedido anterior é preservado; uma
projection antiga não desfaz submit. “Salvando preferência” permanece até
o serviço publicar conclusão. Falha permite repetir; escolhas durante uma
escrita reutilizam coalescing existente. Confirmar o fuso atual não gera
outra escrita. Teclado fecha no submit e na saída da tela.

## Validação automática

```powershell
python tools/generate_timezone_index.py --check
./tools/run_settings_timezone_ui_host_test.ps1
./tools/run_onboarding_timezone_host_test.ps1
./tools/run_settings_wifi_ui_host_test.ps1
```

PASS: equivalência do CSV/índices legados, offsets e países, busca nas 462
zonas com acentos/país/GMT, vazio, scroll até última zona, textos estáticos,
seleção/Aplicar, projection antiga, novo rascunho durante submit, erro/retry,
100 ciclos em cache sem aumento de objetos ou callbacks, fechar/deletar
target com async pendente. Testes do teclado verificam A→B, interação com
tecla, retarget antes do callback de B, rebind após slot livre, destruir dono
com campo sobrevivente, largura 1024 e ancoragem no rodapé. Regressão Wi-Fi
também passou.

Teste de onboarding confirma coalescing e hidratação após restart simulado
com armazenamento stub. Isso não comprova reboot/NVS físico. Busca média
observada no host ~3–4 ms, sem extrapolar latência para o ESP32-P4.

Memória estática P4: `s_ui` passou de 0x16a0 a 0x2300 (+3168 bytes), incluindo
1344 bytes de texto fixo; fallback de fonte do botão usa +36 bytes. Total
adicional medido: 3204 bytes (~3,1 KiB). Índice, países e regras ficam em
flash. Heap LVGL do cache/fragmentação/stack precisam ser medidos na placa.

## Aceite físico pendente — flash manual

1. Conferir layout e foco com touch real, incluindo teclado sobre a cena.
2. Buscar, arrastar até o fim, trocar seleção e Aplicar rapidamente enquanto
   outro write está em voo; confirmar relógio, indicador de salvamento e falha.
3. Após conclusão, reboot e conferir fuso/relógio restaurados.
4. Abrir/fechar repetidamente, inclusive com teclado e async pendentes, alternar
   Wi-Fi/Fuso e confirmar ausência de target antigo/callback duplicado.
5. Registrar heap interno/PSRAM, maior bloco, stack LVGL, tempos e WDT com
   placa/BOM, hashes P4/C6, commit e logs sem segredos. WDT/configuração de
   display não foram alterados. Nenhum gate físico é declarado concluído.

## Build final

ESP-IDF 5.5.4, target explícito esp32p4: build inicial limpo (2149 etapas) em
`build/timezone-clean`, seguido de revalidações desse diretório e `idf.py
-D IDF_TARGET=esp32p4 build` no diretório normal. Todos concluídos, sem
warnings de compilação relevantes. Verificação do índice pelo CMake passou.

Binário final `firmware/build/np2_p4.bin`: `0x2b5230` bytes; partição de
aplicação `0x800000`, 66% livres. Bootloader `0x5a50`. SHA256:
`944f54716e2498e56a84cf363a8ead395e29994c692b954ccab73580403210d0`.
Descrição de origem `50a39d4-dirty`, incluindo as alterações de UI deste
commit. Nenhum flash foi executado, conforme preferência de flash manual.

## Ajuste temporário — somente Brasil (29/09/2026)

Por decisão de produto, a lista e a busca agora oferecem somente as 17
escolhas brasileiras do catálogo, incluindo o alias legado Brasília. O
mesmo filtro vale para a cena dedicada e o modal legado; o dropdown legado
mostra somente Brasil. O texto de busca passa a “Buscar cidade ou GMT”.

O catálogo de 462 registros e seus índices persistidos permanecem intactos.
Apagá-lo agora exigiria migrar os índices de perfis já gravados; os 15 KiB do
CSV estão em flash mapeada, sem 462 objetos LVGL. Uma preferência estrangeira
já gravada não é substituída automaticamente; continua ativa até aplicar uma
escolha brasileira. O buffer da lista foi reduzido de 462 para 24 índices
(17 usados hoje), e o mapeamento de regiões obsoleto foi removido.

A mensagem “Preferência indisponível no momento” vinha do serviço de
onboarding: ele recusava o pedido quando ainda não havia perfil concluído,
situação possível em instalações já configuradas. Agora a primeira escolha
em Preferências gera esse perfil pelo mesmo FlashCoordinator, sem NVS na UI
nem mudança de schema; a conclusão só é confirmada após a gravação. Selecionar
um fuso fecha o teclado global para expor o botão Aplicar. O teste host cobre
primeira gravação, restauração simulada após reboot e teclado aberto na
seleção. Persistência e WDT em placa ainda precisam de validação após flash.

Teste LVGL atualizado passou: subset brasileiro, cidades, GMT-5, rejeição
de Londres/GMT+5:45, seleção/Aplicar, scroll até a última escolha e 100
ciclos com 103 objetos constantes. Validação física aguarda flash manual.
