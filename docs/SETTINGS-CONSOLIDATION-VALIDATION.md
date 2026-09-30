# Validação física — consolidação de Preferências

Esta checklist valida a remoção da tela Settings legada. A placa deve executar
o commit testado, com o target `esp32p4`, mantendo o log serial durante todo o
roteiro. Registre em `BRINGUP-EVIDENCE.md` a data, placa/BOM, commit, hashes,
configuração efetiva, porta, comando e resultado.

## Roteiro

1. Reinicie a placa e confirme boot, primeiro frame e ausência de panic/WDT.
2. Abra Home e confirme que os dados locais continuam renderizando.
3. Abra Perfil, confira a saudação e edite nome/avatar; reinicie e confirme a
   persistência.
4. Abra Preferências pelo fluxo de perfil e confirme as cinco linhas do hub.
5. Em Tela e som, altere brilho, volume e modo noturno; confira bubble local,
   OSD apenas para mudança externa e retenção após reboot.
6. Em Wi-Fi, faça scan, selecione rede aberta e protegida, use cancelar,
   mostrar/ocultar senha, conectar e esquecer; confirme que senha não aparece
   em UI fora do campo, logs ou feedback.
7. Troque o foco entre campos com o teclado global aberto, feche a tela e
   confirme que teclado e alvo são descartados sem reabrir.
8. Em Fuso horário, filtre fusos brasileiros, role a lista virtualizada,
   aplique uma escolha e confirme a persistência após reboot.
9. Em Notificações, altere os três switches suportados, reinicie e confirme
   projeção e persistência; teste som apenas quando o backend estiver ativo.
10. Em Sistema, confira somente valores reais, ação de atualização desabilitada
    quando indisponível e reinício com a confirmação destrutiva.
11. Repita navegação Home → Perfil → Preferências → cada tela por pelo menos
    20 ciclos, observando que nenhum botão fica inativo, não há flash branco e
    não surgem callbacks duplicados.
12. Faça reboot em cada uma das telas, inclusive com teclado aberto.
13. Revalide persistência de perfil, controles, notificações, Wi-Fi e fuso.
14. Monitore pelo serial `task_wdt`, panic, heap corrompido e avisos LVGL.
15. Colete heap interno/PSRAM e high-water das tasks no boot, após o ciclo de
    navegação e após 30 minutos de uso; investigue tendência monotônica.

Build verde não substitui esta evidência de bancada. O fluxo de onboarding
continua usando `wifi_setup_view`; esta checklist cobre o gerenciamento de
Wi-Fi depois da configuração inicial.
