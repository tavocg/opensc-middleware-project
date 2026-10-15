# libjcop3p11

Middleware libre basado en [OpenSC](https://github.com/OpenSC/OpenSC) para
usar tarjetas de firma digital de Costa Rica con `firmador`.

**Una misma biblioteca admite JCOP3 y JCOP4** mediante el driver `iasecc`.
Se elige la configuración de la tarjeta al abrir el firmador.

- **JCOP3:** perfil Generic PKI/IAS-ECC probado con firma RSA SHA-256.
- **JCOP4:** perfil ChipDoc CR probado con RSA 2048, SHA-256 y PKCS#1 v1.5.
- Se probaron firmas PDF, firmas consecutivas y reinserción entre operaciones.
  RSA 3072 y otras personalizaciones siguen pendientes. El soporte es experimental.

## Instalar y usar

Consulta [INSTALL.md](INSTALL.md) para:

1. Compilar OpenSC con las herramientas de tu distribución, sin necesitar Nix.
2. Guardar la biblioteca en `~/.local/lib/libjcop3p11/` si lo deseas.
3. Abrir `firmador` con `LIBASEP11` apuntando a esa biblioteca y la
   configuración de JCOP3 o JCOP4.
4. Descargar un binario de [Releases](https://github.com/tavocg/middleware-project/releases).
   Los binarios del workflow actual requieren su runtime Nix.

No hace falta instalar OpenSC en el sistema ni modificar el firmador.
El PIN se introduce en el diálogo del firmador.

## Desarrollo

Repositorio: [tavocg/middleware-project](https://github.com/tavocg/middleware-project).

- [JCOP3: observaciones y pruebas](docs/jcop3.md).
- [JCOP4: alcance y limitaciones](docs/jcop4-support.md).
- [Cambios frente a OpenSC master](docs/opensc-minimal-diff.md).
- [Parche para OpenSC](patches/opensc-jcop3-jcop4.diff) y
  [commit base](patches/opensc-base.json).
- [Contexto para continuar el proyecto](AGENTS.md).

Los cambios de OpenSC están en `vendor/opensc/`; los comandos de compilación
y pruebas, en el `Makefile`. Los resultados locales de investigación se
conservan en `local/`, excluido de Git. Las pruebas físicas anteriores a la
última limpieza del parche están documentadas junto con sus límites;
las regresiones de software posteriores no sustituyen pruebas con tarjeta.

El workflow de [GitHub Actions](https://github.com/tavocg/middleware-project/actions)
compila y prueba sin tarjeta. Una etiqueta `v*` publica una prerelease;
una ejecución manual solo genera artefactos descargables.

<!-- BEGIN OPENSC SURFACE -->
## Superficie modificada de OpenSC

Base `fc27ee876`; submódulo `b14152bb0`. Verde: función nueva; naranja:
función existente modificada. Los archivos indican **+añadidas / -eliminadas**
frente a la base. Archivos compartidos se repiten visualmente en ambas rutas,
pero las cifras no se suman dos veces. Las flechas entre funciones terminan
solo en funciones nuevas. Punteadas: callback o referencia explícita.
Cada subgrupo contiene un archivo y sus funciones; las flechas desde el archivo
marcan funciones modificadas o entradas sin otro llamador nuevo identificado.

<details>
<summary>Ver diagrama JCOP3 / JCOP4 completo</summary>

```mermaid
flowchart TB
  %% Solid: membership/direct call. Dotted: callback/reference.
  subgraph scope3["JCOP3"]
    direction TB
    JCOP3["JCOP3"]:::root
    subgraph c3_file1_group["src/libopensc"]
      c3_file1["Makefile.am<br/>EXISTENTE · compartido<br/>+7 / -2"]:::file
      JCOP3 --> c3_file1
    end
    subgraph c3_file2_group["src/libopensc"]
      c3_file2["Makefile.mak<br/>EXISTENTE · compartido<br/>+1 / -1"]:::file
      JCOP3 --> c3_file2
    end
    subgraph c3_file3_group["src/libopensc"]
      c3_file3["api-trace.h<br/>AGREGADO · compartido<br/>+93 / -0"]:::file
      JCOP3 --> c3_file3
      c3_fn3_0["sc_api_metadata_trace()<br/>NUEVA"]:::added
    end
    subgraph c3_file4_group["src/libopensc"]
      c3_file4["card-iasecc.c<br/>EXISTENTE · compartido<br/>+348 / -51"]:::file
      JCOP3 --> c3_file4
      c3_fn4_0["iasecc_trace_selection_target()<br/>NUEVA"]:::added
      c3_fn4_1["iasecc_select_file()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_1
      c3_fn4_2["iasecc_process_fci()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_2
      c3_fn4_3["iasecc_is_chipdoc_signing_key()<br/>NUEVA"]:::added
      c3_fn4_4["iasecc_get_algorithm()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_4
      c3_fn4_5["iasecc_set_security_env()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_5
      c3_fn4_6["iasecc_chipdoc_post_login_probe()<br/>NUEVA"]:::added
      c3_fn4_7["iasecc_chipdoc_fresh_verify()<br/>NUEVA"]:::added
      c3_fn4_8["iasecc_chipdoc_select_for_verify()<br/>NUEVA"]:::added
      c3_fn4_9["iasecc_pin_verify()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_9
      c3_fn4_10["iasecc_pin_get_policy()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_10
      c3_fn4_11["iasecc_pin_get_info()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_11
      c3_fn4_12["iasecc_pin_change()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_12
      c3_fn4_13["iasecc_pin_reset()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_13
      c3_fn4_14["iasecc_compute_signature_at()<br/>MODIFICADA"]:::modified
      c3_file4 --> c3_fn4_14
    end
    subgraph c3_file5_group["src/libopensc"]
      c3_file5["card.c<br/>EXISTENTE<br/>+5 / -0"]:::file
      JCOP3 --> c3_file5
      c3_fn5_0["sc_reset()<br/>MODIFICADA"]:::modified
      c3_file5 --> c3_fn5_0
      c3_fn5_1["sc_lock()<br/>MODIFICADA"]:::modified
      c3_file5 --> c3_fn5_1
    end
    subgraph c3_file15_group["src/libopensc"]
      c3_file15["iasecc-sdo.c<br/>EXISTENTE<br/>+34 / -13"]:::file
      JCOP3 --> c3_file15
      c3_fn15_0["iasecc_parse_acls()<br/>MODIFICADA"]:::modified
      c3_file15 --> c3_fn15_0
      c3_fn15_1["iasecc_sdo_free_fields()<br/>MODIFICADA"]:::modified
      c3_file15 --> c3_fn15_1
      c3_fn15_2["iasecc_parse_docp()<br/>MODIFICADA"]:::modified
      c3_file15 --> c3_fn15_2
      c3_fn15_3["iasecc_sdo_parse()<br/>MODIFICADA"]:::modified
      c3_file15 --> c3_fn15_3
      c3_fn15_4["iasecc_docp_copy()<br/>MODIFICADA"]:::modified
      c3_file15 --> c3_fn15_4
    end
    subgraph c3_file16_group["src/libopensc"]
      c3_file16["iasecc-sdo.h<br/>EXISTENTE<br/>+37 / -0"]:::file
      JCOP3 --> c3_file16
      c3_fn16_0["iasecc_parse_compact_acls()<br/>NUEVA"]:::added
    end
    subgraph c3_file17_group["src/libopensc"]
      c3_file17["iasecc.h<br/>EXISTENTE · compartido<br/>+35 / -0"]:::file
      JCOP3 --> c3_file17
      c3_fn17_0["iasecc_chipdoc_configured()<br/>NUEVA"]:::added
    end
    subgraph c3_file18_group["src/libopensc"]
      c3_file18["iso7816.c<br/>EXISTENTE<br/>+9 / -0"]:::file
      JCOP3 --> c3_file18
      c3_fn18_0["iso7816_pin_cmd()<br/>MODIFICADA"]:::modified
      c3_file18 --> c3_fn18_0
    end
    subgraph c3_file26_group["src/libopensc"]
      c3_file26["opensc.h<br/>EXISTENTE<br/>+2 / -0"]:::file
      JCOP3 --> c3_file26
    end
    subgraph c3_file27_group["src/libopensc"]
      c3_file27["pkcs15-iasecc.c<br/>EXISTENTE<br/>+132 / -1"]:::file
      JCOP3 --> c3_file27
      c3_fn27_0["_iasecc_is_chipdoc()<br/>NUEVA"]:::added
      c3_fn27_1["_iasecc_fixup_global_pin_paths()<br/>NUEVA"]:::added
      c3_fn27_2["_iasecc_fixup_signing_key_paths()<br/>NUEVA"]:::added
      c3_fn27_3["_iasecc_fixup_signature_algorithms()<br/>NUEVA"]:::added
      c3_fn27_4["_iasecc_parse_df()<br/>MODIFICADA"]:::modified
      c3_file27 --> c3_fn27_4
      c3_fn27_5["sc_pkcs15emu_iasecc_init()<br/>MODIFICADA"]:::modified
      c3_file27 --> c3_fn27_5
    end
    subgraph c3_file28_group["src/libopensc"]
      c3_file28["pkcs15-pin.c<br/>EXISTENTE<br/>+53 / -1"]:::file
      JCOP3 --> c3_file28
      c3_fn28_0["chipdoc_pin_status_in_place()<br/>NUEVA"]:::added
      c3_fn28_1["sc_pkcs15_get_pin_info()<br/>MODIFICADA"]:::modified
      c3_file28 --> c3_fn28_1
    end
    subgraph c3_file29_group["src/libopensc"]
      c3_file29["pkcs15-sec.c<br/>EXISTENTE · compartido<br/>+9 / -2"]:::file
      JCOP3 --> c3_file29
      c3_fn29_0["use_key()<br/>MODIFICADA"]:::modified
      c3_file29 --> c3_fn29_0
    end
    subgraph c3_file31_group["src/pkcs11"]
      c3_file31["framework-pkcs15.c<br/>EXISTENTE · compartido<br/>+97 / -4"]:::file
      JCOP3 --> c3_file31
      c3_fn31_0["C_GetTokenInfo()<br/>MODIFICADA"]:::modified
      c3_file31 --> c3_fn31_0
      c3_fn31_1["pkcs15_chipdoc_session_state()<br/>NUEVA"]:::added
      c3_fn31_2["slot_get_logged_in_state()<br/>MODIFICADA"]:::modified
      c3_file31 --> c3_fn31_2
      c3_fn31_3["pkcs15_prkey_sign()<br/>MODIFICADA"]:::modified
      c3_file31 --> c3_fn31_3
    end
    subgraph c3_file33_group["src/pkcs11"]
      c3_file33["pkcs11-global.c<br/>EXISTENTE · compartido<br/>+3 / -0"]:::file
      JCOP3 --> c3_file33
      c3_fn33_0["C_GetSlotInfo()<br/>MODIFICADA"]:::modified
      c3_file33 --> c3_fn33_0
    end
    subgraph c3_file34_group["src/pkcs11"]
      c3_file34["pkcs11-object.c<br/>EXISTENTE · compartido<br/>+14 / -0"]:::file
      JCOP3 --> c3_file34
      c3_fn34_0["C_GetAttributeValue()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_0
      c3_fn34_1["C_FindObjectsInit()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_1
      c3_fn34_2["C_SignInit()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_2
      c3_fn34_3["C_Sign()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_3
      c3_fn34_4["C_SignUpdate()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_4
      c3_fn34_5["C_SignFinal()<br/>MODIFICADA"]:::modified
      c3_file34 --> c3_fn34_5
    end
    subgraph c3_file35_group["src/pkcs11"]
      c3_file35["pkcs11-session.c<br/>EXISTENTE · compartido<br/>+27 / -3"]:::file
      JCOP3 --> c3_file35
      c3_fn35_0["C_OpenSession()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_0
      c3_fn35_1["sc_pkcs11_close_session()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_1
      c3_fn35_2["C_CloseSession()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_2
      c3_fn35_3["C_CloseAllSessions()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_3
      c3_fn35_4["C_GetSessionInfo()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_4
      c3_fn35_5["C_Login()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_5
      c3_fn35_6["C_Logout()<br/>MODIFICADA"]:::modified
      c3_file35 --> c3_fn35_6
    end
    subgraph c3_file36_group["src/pkcs11"]
      c3_file36["sc-pkcs11.h<br/>EXISTENTE · compartido<br/>+1 / -0"]:::file
      JCOP3 --> c3_file36
    end
    subgraph c3_file37_group["src/pkcs11"]
      c3_file37["slot.c<br/>EXISTENTE · compartido<br/>+9 / -3"]:::file
      JCOP3 --> c3_file37
      c3_fn37_0["card_detect()<br/>MODIFICADA"]:::modified
      c3_file37 --> c3_fn37_0
      c3_fn37_1["slot_token_removed()<br/>MODIFICADA"]:::modified
      c3_file37 --> c3_fn37_1
    end
    subgraph c3_file38_group["src/tools"]
      c3_file38["pkcs11-tool.c<br/>EXISTENTE<br/>+5 / -0"]:::file
      JCOP3 --> c3_file38
      c3_fn38_0["sign_data()<br/>MODIFICADA"]:::modified
      c3_file38 --> c3_fn38_0
    end
  end
  subgraph scope4["JCOP4"]
    direction TB
    JCOP4["JCOP4"]:::root
    subgraph c4_file0_group["etc"]
      c4_file0["opensc.conf.example.in<br/>EXISTENTE<br/>+10 / -0"]:::file
      JCOP4 --> c4_file0
    end
    subgraph c4_file1_group["src/libopensc"]
      c4_file1["Makefile.am<br/>EXISTENTE · compartido<br/>+7 / -2"]:::file
      JCOP4 --> c4_file1
    end
    subgraph c4_file2_group["src/libopensc"]
      c4_file2["Makefile.mak<br/>EXISTENTE · compartido<br/>+1 / -1"]:::file
      JCOP4 --> c4_file2
    end
    subgraph c4_file3_group["src/libopensc"]
      c4_file3["api-trace.h<br/>AGREGADO · compartido<br/>+93 / -0"]:::file
      JCOP4 --> c4_file3
      c4_fn3_0["sc_api_metadata_trace()<br/>NUEVA"]:::added
    end
    subgraph c4_file4_group["src/libopensc"]
      c4_file4["card-iasecc.c<br/>EXISTENTE · compartido<br/>+348 / -51"]:::file
      JCOP4 --> c4_file4
      c4_fn4_0["iasecc_match_card()<br/>MODIFICADA"]:::modified
      c4_file4 --> c4_fn4_0
      c4_fn4_1["iasecc_init()<br/>MODIFICADA"]:::modified
      c4_file4 --> c4_fn4_1
      c4_fn4_2["iasecc_chipdoc_invalidate_card()<br/>NUEVA"]:::added
      c4_fn4_3["iasecc_logout()<br/>MODIFICADA"]:::modified
      c4_file4 --> c4_fn4_3
      c4_fn4_4["iasecc_finish()<br/>MODIFICADA"]:::modified
      c4_file4 --> c4_fn4_4
      c4_fn4_5["iasecc_card_reader_lock_obtained()<br/>NUEVA"]:::added
      c4_fn4_6["sc_get_driver()<br/>MODIFICADA"]:::modified
      c4_file4 --> c4_fn4_6
    end
    subgraph c4_file6_group["src/libopensc"]
      c4_file6["cards.h<br/>EXISTENTE<br/>+1 / -0"]:::file
      JCOP4 --> c4_file6
    end
    subgraph c4_file7_group["src/libopensc"]
      c4_file7["iasecc-chipdoc-bind.c<br/>AGREGADO<br/>+42 / -0"]:::file
      JCOP4 --> c4_file7
      c4_fn7_0["iasecc_chipdoc_pkcs15_activate()<br/>NUEVA"]:::added
    end
    subgraph c4_file8_group["src/libopensc"]
      c4_file8["iasecc-chipdoc-init.h<br/>AGREGADO<br/>+121 / -0"]:::file
      JCOP4 --> c4_file8
      c4_fn8_0["iasecc_chipdoc_opted_in()<br/>NUEVA"]:::added
      c4_fn8_1["chipdoc_unbound_pin()<br/>NUEVA"]:::added
      c4_fn8_2["chipdoc_unbound_logout()<br/>NUEVA"]:::added
      c4_fn8_3["chipdoc_metadata_read()<br/>NUEVA"]:::added
      c4_fn8_4["chipdoc_metadata_record()<br/>NUEVA"]:::added
      c4_fn8_5["chipdoc_metadata_data()<br/>NUEVA"]:::added
      c4_fn8_6["iasecc_chipdoc_initialize()<br/>NUEVA"]:::added
    end
    subgraph c4_file9_group["src/libopensc"]
      c4_file9["iasecc-chipdoc-pkcs15.h<br/>AGREGADO<br/>+149 / -0"]:::file
      JCOP4 --> c4_file9
      c4_fn9_0["iasecc_chipdoc_pkcs15_key()<br/>NUEVA"]:::added
      c4_fn9_1["iasecc_chipdoc_pkcs15_operation()<br/>NUEVA"]:::added
      c4_fn9_2["iasecc_chipdoc_pkcs15_disable_pin_cache()<br/>NUEVA"]:::added
      c4_file9 --> c4_fn9_2
    end
    subgraph c4_file10_group["src/libopensc"]
      c4_file10["iasecc-chipdoc-routing.h<br/>AGREGADO<br/>+407 / -0"]:::file
      JCOP4 --> c4_file10
      c4_fn10_0["chipdoc_route()<br/>NUEVA"]:::added
      c4_fn10_1["chipdoc_route_close()<br/>NUEVA"]:::added
      c4_fn10_2["chipdoc_route_valid()<br/>NUEVA"]:::added
      c4_fn10_3["chipdoc_route_require()<br/>NUEVA"]:::added
      c4_fn10_4["chipdoc_route_idle()<br/>NUEVA"]:::added
      c4_fn10_5["chipdoc_route_pin()<br/>NUEVA"]:::added
      c4_fn10_6["chipdoc_route_select()<br/>NUEVA"]:::added
      c4_fn10_7["chipdoc_route_mse()<br/>NUEVA"]:::added
      c4_fn10_8["chipdoc_route_sign()<br/>NUEVA"]:::added
      c4_fn10_9["chipdoc_route_read()<br/>NUEVA"]:::added
      c4_fn10_10["chipdoc_route_record()<br/>NUEVA"]:::added
      c4_fn10_11["chipdoc_route_data()<br/>NUEVA"]:::added
      c4_fn10_12["chipdoc_route_logout()<br/>NUEVA"]:::added
      c4_fn10_13["iasecc_chipdoc_routing_remove()<br/>NUEVA"]:::added
      c4_fn10_14["chipdoc_route_clear()<br/>NUEVA"]:::added
      c4_fn10_15["chipdoc_route_finish()<br/>NUEVA"]:::added
      c4_fn10_16["iasecc_chipdoc_routing_install()<br/>NUEVA"]:::added
      c4_fn10_17["iasecc_chipdoc_routing_last_report()<br/>NUEVA"]:::added
      c4_file10 --> c4_fn10_17
    end
    subgraph c4_file11_group["src/libopensc"]
      c4_file11["iasecc-chipdoc-sm.c<br/>AGREGADO<br/>+1219 / -0"]:::file
      JCOP4 --> c4_file11
      c4_fn11_0["chipdoc_provider_matches()<br/>NUEVA"]:::added
      c4_fn11_1["chipdoc_crypto_clear()<br/>NUEVA"]:::added
      c4_fn11_2["chipdoc_platform_supported()<br/>NUEVA"]:::added
      c4_fn11_3["chipdoc_platform_ready()<br/>NUEVA"]:::added
      c4_fn11_4["chipdoc_ready()<br/>NUEVA"]:::added
      c4_fn11_5["iasecc_chipdoc_channel_supported()<br/>NUEVA"]:::added
      c4_fn11_6["iasecc_chipdoc_channel_pin_status()<br/>NUEVA"]:::added
      c4_fn11_7["chipdoc_setup_exchange()<br/>NUEVA"]:::added
      c4_fn11_8["iasecc_chipdoc_channel_open()<br/>NUEVA"]:::added
      c4_fn11_9["iasecc_chipdoc_channel_select_mf()<br/>NUEVA"]:::added
      c4_fn11_10["chipdoc_bootstrap_exchange()<br/>NUEVA"]:::added
      c4_fn11_11["iasecc_chipdoc_channel_bootstrap()<br/>NUEVA"]:::added
      c4_fn11_12["iasecc_chipdoc_transaction_begin()<br/>NUEVA"]:::added
      c4_fn11_13["iasecc_chipdoc_channel_query_pin()<br/>NUEVA"]:::added
      c4_fn11_14["iasecc_chipdoc_channel_verify_pin()<br/>NUEVA"]:::added
      c4_fn11_15["chipdoc_signing_ready()<br/>NUEVA"]:::added
      c4_fn11_16["chipdoc_signature_exchange()<br/>NUEVA"]:::added
      c4_fn11_17["iasecc_chipdoc_channel_select_application()<br/>NUEVA"]:::added
      c4_fn11_18["iasecc_chipdoc_channel_set_security_env()<br/>NUEVA"]:::added
      c4_fn11_19["iasecc_chipdoc_channel_compute_signature()<br/>NUEVA"]:::added
    end
    subgraph c4_file12_group["src/libopensc"]
      c4_file12["iasecc-chipdoc-sm.h<br/>AGREGADO<br/>+103 / -0"]:::file
      JCOP4 --> c4_file12
    end
    subgraph c4_file13_group["src/libopensc"]
      c4_file13["iasecc-chipdoc-transaction.h<br/>AGREGADO<br/>+82 / -0"]:::file
      JCOP4 --> c4_file13
      c4_fn13_0["iasecc_chipdoc_transaction_owns()<br/>NUEVA"]:::added
      c4_fn13_1["iasecc_chipdoc_transaction_close()<br/>NUEVA"]:::added
    end
    subgraph c4_file14_group["src/libopensc"]
      c4_file14["iasecc-chipdoc.h<br/>AGREGADO<br/>+261 / -0"]:::file
      JCOP4 --> c4_file14
      c4_fn14_0["iasecc_chipdoc_profile()<br/>NUEVA"]:::added
      c4_fn14_1["iasecc_chipdoc_candidate()<br/>NUEVA"]:::added
      c4_fn14_2["iasecc_chipdoc_transport()<br/>NUEVA"]:::added
      c4_fn14_3["iasecc_chipdoc_invalidate()<br/>NUEVA"]:::added
      c4_fn14_4["iasecc_chipdoc_begin_bootstrap()<br/>NUEVA"]:::added
      c4_fn14_5["iasecc_chipdoc_prepare()<br/>NUEVA"]:::added
      c4_fn14_6["iasecc_chipdoc_attach()<br/>NUEVA"]:::added
      c4_fn14_7["iasecc_chipdoc_signing_key()<br/>NUEVA"]:::added
      c4_fn14_8["iasecc_chipdoc_record()<br/>NUEVA"]:::added
    end
    subgraph c4_file17_group["src/libopensc"]
      c4_file17["iasecc.h<br/>EXISTENTE · compartido<br/>+35 / -0"]:::file
      JCOP4 --> c4_file17
      c4_fn17_0["iasecc_chipdoc_configured()<br/>NUEVA"]:::added
    end
    subgraph c4_file19_group["src/libopensc"]
      c4_file19["jcop4-channel-agreement.h<br/>AGREGADO<br/>+202 / -0"]:::file
      JCOP4 --> c4_file19
      c4_fn19_0["jcop4_ca_pending_clear()<br/>NUEVA"]:::added
      c4_fn19_1["jcop4_p521_public_valid()<br/>NUEVA"]:::added
      c4_fn19_2["jcop4_ca_prepare_key_ex()<br/>NUEVA"]:::added
      c4_fn19_3["jcop4_ca_prepare_ex()<br/>NUEVA"]:::added
      c4_fn19_4["jcop4_ca_prepare_key()<br/>NUEVA"]:::added
      c4_file19 --> c4_fn19_4
      c4_fn19_5["jcop4_ca_prepare()<br/>NUEVA"]:::added
      c4_file19 --> c4_fn19_5
      c4_fn19_6["jcop4_ca_build_mse()<br/>NUEVA"]:::added
      c4_fn19_7["jcop4_ca_build_ga()<br/>NUEVA"]:::added
    end
    subgraph c4_file20_group["src/libopensc"]
      c4_file20["jcop4-channel-confirmation.h<br/>AGREGADO<br/>+198 / -0"]:::file
      JCOP4 --> c4_file20
      c4_fn20_0["jcop4_ca_confirm_d004_session_ex()<br/>NUEVA"]:::added
      c4_fn20_1["jcop4_ca_confirm_d004_session()<br/>NUEVA"]:::added
      c4_fn20_2["jcop4_ca_confirm_d004()<br/>NUEVA"]:::added
      c4_file20 --> c4_fn20_2
    end
    subgraph c4_file21_group["src/libopensc"]
      c4_file21["jcop4-channel-controls.h<br/>AGREGADO<br/>+41 / -0"]:::file
      JCOP4 --> c4_file21
      c4_fn21_0["jcop4_sm_mf_control()<br/>NUEVA"]:::added
    end
    subgraph c4_file22_group["src/libopensc"]
      c4_file22["jcop4-channel-kdf.h<br/>AGREGADO<br/>+68 / -0"]:::file
      JCOP4 --> c4_file22
      c4_fn22_0["jcop4_ca_aes128_kdf_ex()<br/>NUEVA"]:::added
      c4_fn22_1["jcop4_ca_aes128_kdf()<br/>NUEVA"]:::added
      c4_file22 --> c4_fn22_1
    end
    subgraph c4_file23_group["src/libopensc"]
      c4_file23["jcop4-channel-messages.h<br/>AGREGADO<br/>+406 / -0"]:::file
      JCOP4 --> c4_file23
      c4_fn23_0["jcop4_sm_invalidate()<br/>NUEVA"]:::added
      c4_fn23_1["jcop4_sm_init()<br/>NUEVA"]:::added
      c4_fn23_2["jcop4_sm_ssc_iv_ex()<br/>NUEVA"]:::added
      c4_fn23_3["jcop4_sm_ssc_iv()<br/>NUEVA"]:::added
      c4_file23 --> c4_fn23_3
      c4_fn23_4["jcop4_sm_append_tlv()<br/>NUEVA"]:::added
      c4_fn23_5["jcop4_sm_take_tlv()<br/>NUEVA"]:::added
      c4_fn23_6["jcop4_sm_seal_policy_ex()<br/>NUEVA"]:::added
      c4_fn23_7["jcop4_sm_seal_policy()<br/>NUEVA"]:::added
      c4_fn23_8["jcop4_sm_seal()<br/>NUEVA"]:::added
      c4_file23 --> c4_fn23_8
      c4_fn23_9["jcop4_sm_decode_name()<br/>NUEVA"]:::added
      c4_file23 --> c4_fn23_9
      c4_fn23_10["jcop4_sm_open_report_policy_ex()<br/>NUEVA"]:::added
      c4_fn23_11["jcop4_sm_open_report_policy()<br/>NUEVA"]:::added
      c4_fn23_12["jcop4_sm_open_report()<br/>NUEVA"]:::added
      c4_fn23_13["jcop4_sm_open()<br/>NUEVA"]:::added
      c4_file23 --> c4_fn23_13
    end
    subgraph c4_file24_group["src/libopensc"]
      c4_file24["jcop4-channel-primitives.h<br/>AGREGADO<br/>+172 / -0"]:::file
      JCOP4 --> c4_file24
      c4_fn24_0["jcop4_sm_next_ssc()<br/>NUEVA"]:::added
      c4_fn24_1["jcop4_sm_pad()<br/>NUEVA"]:::added
      c4_fn24_2["jcop4_sm_unpad()<br/>NUEVA"]:::added
      c4_fn24_3["jcop4_sm_aes128_cbc_ex()<br/>NUEVA"]:::added
      c4_fn24_4["jcop4_sm_cmac_ex()<br/>NUEVA"]:::added
      c4_fn24_5["jcop4_sm_verify_mac8_ex()<br/>NUEVA"]:::added
      c4_fn24_6["jcop4_sm_aes128_cbc()<br/>NUEVA"]:::added
      c4_file24 --> c4_fn24_6
      c4_fn24_7["jcop4_sm_cmac()<br/>NUEVA"]:::added
      c4_file24 --> c4_fn24_7
      c4_fn24_8["jcop4_sm_verify_mac8()<br/>NUEVA"]:::added
      c4_file24 --> c4_fn24_8
    end
    subgraph c4_file25_group["src/libopensc"]
      c4_file25["jcop4-dh-control.h<br/>AGREGADO<br/>+53 / -0"]:::file
      JCOP4 --> c4_file25
      c4_fn25_0["d004_control_matches_observation()<br/>NUEVA"]:::added
    end
    subgraph c4_file29_group["src/libopensc"]
      c4_file29["pkcs15-sec.c<br/>EXISTENTE · compartido<br/>+9 / -2"]:::file
      JCOP4 --> c4_file29
      c4_fn29_0["use_key()<br/>MODIFICADA"]:::modified
      c4_file29 --> c4_fn29_0
    end
    subgraph c4_file30_group["src/libopensc"]
      c4_file30["pkcs15.c<br/>EXISTENTE<br/>+14 / -0"]:::file
      JCOP4 --> c4_file30
      c4_fn30_0["sc_pkcs15_bind()<br/>MODIFICADA"]:::modified
      c4_file30 --> c4_fn30_0
    end
    subgraph c4_file31_group["src/pkcs11"]
      c4_file31["framework-pkcs15.c<br/>EXISTENTE · compartido<br/>+97 / -4"]:::file
      JCOP4 --> c4_file31
      c4_fn31_0["pkcs15_bind()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_0
      c4_fn31_1["C_GetTokenInfo()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_1
      c4_fn31_2["pkcs15_create_pkcs11_objects()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_2
      c4_fn31_3["pkcs15_init_slot()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_3
      c4_fn31_4["slot_get_logged_in_state()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_4
      c4_fn31_5["pkcs15_login()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_5
      c4_fn31_6["pkcs15_logout()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_6
      c4_fn31_7["pkcs15_prkey_sign()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_7
      c4_fn31_8["register_mechanisms()<br/>MODIFICADA"]:::modified
      c4_file31 --> c4_fn31_8
    end
    subgraph c4_file32_group["src/pkcs11"]
      c4_file32["misc.c<br/>EXISTENTE<br/>+15 / -0"]:::file
      JCOP4 --> c4_file32
      c4_fn32_0["reset_login_state()<br/>MODIFICADA"]:::modified
      c4_file32 --> c4_fn32_0
    end
    subgraph c4_file33_group["src/pkcs11"]
      c4_file33["pkcs11-global.c<br/>EXISTENTE · compartido<br/>+3 / -0"]:::file
      JCOP4 --> c4_file33
      c4_fn33_0["C_GetSlotInfo()<br/>MODIFICADA"]:::modified
      c4_file33 --> c4_fn33_0
    end
    subgraph c4_file34_group["src/pkcs11"]
      c4_file34["pkcs11-object.c<br/>EXISTENTE · compartido<br/>+14 / -0"]:::file
      JCOP4 --> c4_file34
      c4_fn34_0["C_GetAttributeValue()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_0
      c4_fn34_1["C_FindObjectsInit()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_1
      c4_fn34_2["C_SignInit()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_2
      c4_fn34_3["C_Sign()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_3
      c4_fn34_4["C_SignUpdate()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_4
      c4_fn34_5["C_SignFinal()<br/>MODIFICADA"]:::modified
      c4_file34 --> c4_fn34_5
    end
    subgraph c4_file35_group["src/pkcs11"]
      c4_file35["pkcs11-session.c<br/>EXISTENTE · compartido<br/>+27 / -3"]:::file
      JCOP4 --> c4_file35
      c4_fn35_0["C_OpenSession()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_0
      c4_fn35_1["sc_pkcs11_close_session()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_1
      c4_fn35_2["C_CloseSession()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_2
      c4_fn35_3["C_CloseAllSessions()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_3
      c4_fn35_4["C_GetSessionInfo()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_4
      c4_fn35_5["C_Login()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_5
      c4_fn35_6["C_Logout()<br/>MODIFICADA"]:::modified
      c4_file35 --> c4_fn35_6
    end
    subgraph c4_file36_group["src/pkcs11"]
      c4_file36["sc-pkcs11.h<br/>EXISTENTE · compartido<br/>+1 / -0"]:::file
      JCOP4 --> c4_file36
    end
    subgraph c4_file37_group["src/pkcs11"]
      c4_file37["slot.c<br/>EXISTENTE · compartido<br/>+9 / -3"]:::file
      JCOP4 --> c4_file37
      c4_fn37_0["card_detect()<br/>MODIFICADA"]:::modified
      c4_file37 --> c4_fn37_0
      c4_fn37_1["slot_token_removed()<br/>MODIFICADA"]:::modified
      c4_file37 --> c4_fn37_1
    end
  end
  c3_fn15_0 --> c3_fn16_0
  c3_fn18_0 --> c3_fn3_0
  c3_fn27_1 --> c3_fn27_0
  c3_fn27_2 --> c3_fn27_0
  c3_fn27_3 --> c3_fn27_0
  c3_fn27_4 --> c3_fn27_1
  c3_fn27_4 --> c3_fn27_2
  c3_fn27_4 --> c3_fn27_3
  c3_fn27_5 --> c3_fn27_1
  c3_fn27_5 --> c3_fn27_2
  c3_fn27_5 --> c3_fn27_3
  c3_fn28_1 --> c3_fn28_0
  c3_fn28_1 --> c3_fn3_0
  c3_fn29_0 --> c3_fn3_0
  c3_fn31_0 --> c3_fn31_1
  c3_fn31_0 --> c3_fn3_0
  c3_fn31_2 --> c3_fn17_0
  c3_fn31_2 --> c3_fn31_1
  c3_fn31_2 --> c3_fn3_0
  c3_fn31_3 --> c3_fn3_0
  c3_fn33_0 --> c3_fn3_0
  c3_fn34_0 --> c3_fn3_0
  c3_fn34_1 --> c3_fn3_0
  c3_fn34_2 --> c3_fn17_0
  c3_fn34_2 --> c3_fn3_0
  c3_fn34_3 --> c3_fn3_0
  c3_fn34_4 --> c3_fn3_0
  c3_fn34_5 --> c3_fn3_0
  c3_fn35_0 --> c3_fn3_0
  c3_fn35_1 --> c3_fn17_0
  c3_fn35_2 --> c3_fn3_0
  c3_fn35_3 --> c3_fn3_0
  c3_fn35_4 --> c3_fn3_0
  c3_fn35_5 --> c3_fn17_0
  c3_fn35_5 --> c3_fn3_0
  c3_fn35_6 --> c3_fn3_0
  c3_fn37_0 --> c3_fn3_0
  c3_fn37_1 --> c3_fn3_0
  c3_fn4_1 --> c3_fn3_0
  c3_fn4_1 --> c3_fn4_0
  c3_fn4_11 --> c3_fn3_0
  c3_fn4_14 --> c3_fn3_0
  c3_fn4_2 --> c3_fn16_0
  c3_fn4_4 --> c3_fn4_3
  c3_fn4_5 --> c3_fn3_0
  c3_fn4_5 --> c3_fn4_3
  c3_fn4_6 --> c3_fn3_0
  c3_fn4_8 --> c3_fn3_0
  c3_fn4_8 --> c3_fn4_7
  c3_fn4_9 --> c3_fn3_0
  c3_fn4_9 --> c3_fn4_6
  c3_fn4_9 --> c3_fn4_7
  c3_fn4_9 --> c3_fn4_8
  c3_fn5_0 --> c3_fn3_0
  c3_fn5_1 --> c3_fn3_0
  c4_fn10_1 --> c4_fn13_1
  c4_fn10_10 --> c4_fn10_0
  c4_fn10_10 --> c4_fn10_4
  c4_fn10_11 --> c4_fn10_0
  c4_fn10_11 --> c4_fn10_4
  c4_fn10_12 --> c4_fn10_0
  c4_fn10_12 --> c4_fn10_1
  c4_fn10_13 --> c4_fn10_0
  c4_fn10_13 --> c4_fn10_1
  c4_fn10_13 --> c4_fn13_1
  c4_fn10_13 -. callback / referencia .-> c4_fn10_14
  c4_fn10_14 --> c4_fn10_0
  c4_fn10_14 --> c4_fn10_13
  c4_fn10_15 --> c4_fn10_0
  c4_fn10_15 --> c4_fn10_13
  c4_fn10_16 --> c4_fn11_5
  c4_fn10_16 --> c4_fn9_1
  c4_fn10_16 -. callback / referencia .-> c4_fn10_10
  c4_fn10_16 -. callback / referencia .-> c4_fn10_11
  c4_fn10_16 -. callback / referencia .-> c4_fn10_12
  c4_fn10_16 -. callback / referencia .-> c4_fn10_14
  c4_fn10_16 -. callback / referencia .-> c4_fn10_15
  c4_fn10_16 -. callback / referencia .-> c4_fn10_5
  c4_fn10_16 -. callback / referencia .-> c4_fn10_6
  c4_fn10_16 -. callback / referencia .-> c4_fn10_7
  c4_fn10_16 -. callback / referencia .-> c4_fn10_8
  c4_fn10_16 -. callback / referencia .-> c4_fn10_9
  c4_fn10_17 --> c4_fn10_0
  c4_fn10_2 --> c4_fn11_5
  c4_fn10_2 --> c4_fn9_1
  c4_fn10_2 -. callback / referencia .-> c4_fn10_14
  c4_fn10_3 --> c4_fn10_1
  c4_fn10_3 --> c4_fn10_2
  c4_fn10_3 --> c4_fn4_2
  c4_fn10_4 --> c4_fn10_3
  c4_fn10_5 --> c4_fn10_0
  c4_fn10_5 --> c4_fn10_1
  c4_fn10_5 --> c4_fn10_3
  c4_fn10_5 --> c4_fn10_4
  c4_fn10_5 --> c4_fn11_12
  c4_fn10_5 --> c4_fn11_13
  c4_fn10_5 --> c4_fn11_14
  c4_fn10_5 --> c4_fn11_6
  c4_fn10_6 --> c4_fn10_0
  c4_fn10_6 --> c4_fn10_1
  c4_fn10_6 --> c4_fn10_3
  c4_fn10_6 --> c4_fn10_4
  c4_fn10_6 --> c4_fn11_17
  c4_fn10_6 --> c4_fn14_0
  c4_fn10_7 --> c4_fn10_0
  c4_fn10_7 --> c4_fn10_1
  c4_fn10_7 --> c4_fn10_3
  c4_fn10_7 --> c4_fn11_18
  c4_fn10_7 --> c4_fn11_6
  c4_fn10_8 --> c4_fn10_0
  c4_fn10_8 --> c4_fn10_1
  c4_fn10_8 --> c4_fn10_3
  c4_fn10_8 --> c4_fn11_19
  c4_fn10_8 --> c4_fn11_6
  c4_fn10_9 --> c4_fn10_0
  c4_fn10_9 --> c4_fn10_4
  c4_fn11_10 --> c4_fn11_0
  c4_fn11_10 --> c4_fn11_3
  c4_fn11_11 --> c4_fn11_10
  c4_fn11_11 --> c4_fn11_3
  c4_fn11_11 --> c4_fn11_8
  c4_fn11_11 --> c4_fn14_0
  c4_fn11_11 --> c4_fn14_4
  c4_fn11_11 --> c4_fn14_5
  c4_fn11_11 --> c4_fn19_1
  c4_fn11_11 --> c4_fn25_0
  c4_fn11_11 --> c4_fn4_2
  c4_fn11_11 --> c4_fn9_0
  c4_fn11_12 --> c4_fn11_11
  c4_fn11_12 --> c4_fn11_2
  c4_fn11_12 --> c4_fn11_9
  c4_fn11_12 --> c4_fn13_0
  c4_fn11_12 --> c4_fn13_1
  c4_fn11_12 --> c4_fn4_2
  c4_fn11_12 --> c4_fn9_0
  c4_fn11_12 --> c4_fn9_1
  c4_fn11_13 --> c4_fn11_0
  c4_fn11_13 --> c4_fn11_4
  c4_fn11_13 --> c4_fn13_0
  c4_fn11_13 --> c4_fn14_8
  c4_fn11_13 --> c4_fn23_10
  c4_fn11_13 --> c4_fn23_6
  c4_fn11_13 --> c4_fn4_2
  c4_fn11_13 -. callback / referencia .-> c4_fn11_1
  c4_fn11_14 --> c4_fn11_0
  c4_fn11_14 --> c4_fn11_4
  c4_fn11_14 --> c4_fn13_0
  c4_fn11_14 --> c4_fn14_8
  c4_fn11_14 --> c4_fn23_10
  c4_fn11_14 --> c4_fn23_6
  c4_fn11_14 --> c4_fn4_2
  c4_fn11_14 -. callback / referencia .-> c4_fn11_1
  c4_fn11_15 --> c4_fn11_0
  c4_fn11_15 --> c4_fn11_4
  c4_fn11_15 --> c4_fn13_0
  c4_fn11_15 --> c4_fn4_2
  c4_fn11_15 -. callback / referencia .-> c4_fn11_1
  c4_fn11_16 --> c4_fn11_0
  c4_fn11_16 --> c4_fn11_15
  c4_fn11_16 --> c4_fn13_0
  c4_fn11_16 --> c4_fn23_10
  c4_fn11_16 --> c4_fn23_6
  c4_fn11_16 --> c4_fn4_2
  c4_fn11_17 --> c4_fn11_15
  c4_fn11_17 --> c4_fn11_16
  c4_fn11_17 --> c4_fn14_0
  c4_fn11_17 --> c4_fn14_8
  c4_fn11_17 --> c4_fn4_2
  c4_fn11_18 --> c4_fn11_15
  c4_fn11_18 --> c4_fn11_16
  c4_fn11_18 --> c4_fn11_17
  c4_fn11_18 --> c4_fn14_0
  c4_fn11_18 --> c4_fn14_7
  c4_fn11_18 --> c4_fn14_8
  c4_fn11_18 --> c4_fn4_2
  c4_fn11_19 --> c4_fn11_15
  c4_fn11_19 --> c4_fn11_16
  c4_fn11_19 --> c4_fn13_1
  c4_fn11_19 --> c4_fn14_8
  c4_fn11_19 --> c4_fn4_2
  c4_fn11_2 --> c4_fn14_2
  c4_fn11_3 --> c4_fn11_2
  c4_fn11_3 --> c4_fn13_0
  c4_fn11_4 --> c4_fn11_3
  c4_fn11_5 --> c4_fn11_2
  c4_fn11_6 --> c4_fn11_0
  c4_fn11_6 --> c4_fn11_5
  c4_fn11_6 --> c4_fn13_0
  c4_fn11_6 --> c4_fn4_2
  c4_fn11_6 -. callback / referencia .-> c4_fn11_1
  c4_fn11_7 --> c4_fn11_0
  c4_fn11_7 --> c4_fn11_4
  c4_fn11_7 --> c4_fn19_1
  c4_fn11_8 --> c4_fn11_0
  c4_fn11_8 --> c4_fn11_1
  c4_fn11_8 --> c4_fn11_4
  c4_fn11_8 --> c4_fn14_6
  c4_fn11_8 --> c4_fn20_0
  c4_fn11_8 --> c4_fn4_2
  c4_fn11_8 -. callback / referencia .-> c4_fn11_7
  c4_fn11_9 --> c4_fn11_0
  c4_fn11_9 --> c4_fn11_4
  c4_fn11_9 --> c4_fn14_8
  c4_fn11_9 --> c4_fn21_0
  c4_fn11_9 --> c4_fn23_10
  c4_fn11_9 --> c4_fn23_6
  c4_fn11_9 --> c4_fn4_2
  c4_fn11_9 -. callback / referencia .-> c4_fn11_1
  c4_fn13_1 --> c4_fn13_0
  c4_fn13_1 --> c4_fn4_2
  c4_fn14_1 --> c4_fn14_0
  c4_fn14_2 --> c4_fn14_0
  c4_fn14_4 --> c4_fn14_0
  c4_fn14_5 --> c4_fn14_0
  c4_fn14_5 --> c4_fn14_1
  c4_fn14_5 --> c4_fn14_2
  c4_fn14_6 --> c4_fn14_0
  c4_fn14_7 --> c4_fn14_0
  c4_fn14_8 --> c4_fn14_0
  c4_fn14_8 --> c4_fn14_3
  c4_fn19_2 --> c4_fn19_0
  c4_fn19_2 --> c4_fn19_1
  c4_fn19_2 --> c4_fn22_0
  c4_fn19_3 --> c4_fn19_0
  c4_fn19_3 --> c4_fn19_1
  c4_fn19_3 --> c4_fn19_2
  c4_fn19_4 --> c4_fn19_2
  c4_fn19_5 --> c4_fn19_3
  c4_fn19_7 --> c4_fn19_1
  c4_fn20_0 --> c4_fn19_0
  c4_fn20_0 --> c4_fn19_1
  c4_fn20_0 --> c4_fn19_3
  c4_fn20_0 --> c4_fn19_6
  c4_fn20_0 --> c4_fn19_7
  c4_fn20_0 --> c4_fn23_0
  c4_fn20_0 --> c4_fn23_1
  c4_fn20_0 --> c4_fn23_10
  c4_fn20_0 --> c4_fn23_6
  c4_fn20_1 --> c4_fn20_0
  c4_fn20_2 --> c4_fn20_1
  c4_fn21_0 --> c4_fn23_5
  c4_fn22_1 --> c4_fn22_0
  c4_fn23_1 --> c4_fn23_0
  c4_fn23_10 --> c4_fn23_0
  c4_fn23_10 --> c4_fn23_2
  c4_fn23_10 --> c4_fn23_5
  c4_fn23_10 --> c4_fn24_0
  c4_fn23_10 --> c4_fn24_1
  c4_fn23_10 --> c4_fn24_2
  c4_fn23_10 --> c4_fn24_3
  c4_fn23_10 --> c4_fn24_5
  c4_fn23_11 --> c4_fn23_10
  c4_fn23_12 --> c4_fn23_11
  c4_fn23_13 --> c4_fn23_12
  c4_fn23_2 --> c4_fn24_3
  c4_fn23_3 --> c4_fn23_2
  c4_fn23_6 --> c4_fn23_0
  c4_fn23_6 --> c4_fn23_2
  c4_fn23_6 --> c4_fn23_4
  c4_fn23_6 --> c4_fn24_0
  c4_fn23_6 --> c4_fn24_1
  c4_fn23_6 --> c4_fn24_3
  c4_fn23_6 --> c4_fn24_4
  c4_fn23_7 --> c4_fn23_6
  c4_fn23_8 --> c4_fn23_7
  c4_fn24_5 --> c4_fn24_4
  c4_fn24_6 --> c4_fn24_3
  c4_fn24_7 --> c4_fn24_4
  c4_fn24_8 --> c4_fn24_5
  c4_fn29_0 --> c4_fn3_0
  c4_fn30_0 --> c4_fn17_0
  c4_fn30_0 --> c4_fn7_0
  c4_fn31_0 --> c4_fn17_0
  c4_fn31_1 --> c3_fn31_1
  c4_fn31_1 --> c4_fn3_0
  c4_fn31_2 --> c4_fn17_0
  c4_fn31_3 --> c4_fn17_0
  c4_fn31_4 --> c3_fn31_1
  c4_fn31_4 --> c4_fn17_0
  c4_fn31_4 --> c4_fn3_0
  c4_fn31_5 --> c4_fn17_0
  c4_fn31_6 --> c4_fn17_0
  c4_fn31_7 --> c4_fn3_0
  c4_fn31_8 --> c4_fn17_0
  c4_fn32_0 --> c4_fn17_0
  c4_fn33_0 --> c4_fn3_0
  c4_fn34_0 --> c4_fn3_0
  c4_fn34_1 --> c4_fn3_0
  c4_fn34_2 --> c4_fn17_0
  c4_fn34_2 --> c4_fn3_0
  c4_fn34_3 --> c4_fn3_0
  c4_fn34_4 --> c4_fn3_0
  c4_fn34_5 --> c4_fn3_0
  c4_fn35_0 --> c4_fn3_0
  c4_fn35_1 --> c4_fn17_0
  c4_fn35_2 --> c4_fn3_0
  c4_fn35_3 --> c4_fn3_0
  c4_fn35_4 --> c4_fn3_0
  c4_fn35_5 --> c4_fn17_0
  c4_fn35_5 --> c4_fn3_0
  c4_fn35_6 --> c4_fn3_0
  c4_fn37_0 --> c4_fn3_0
  c4_fn37_1 --> c4_fn3_0
  c4_fn4_0 --> c4_fn8_0
  c4_fn4_1 --> c4_fn8_6
  c4_fn4_1 -. callback / referencia .-> c4_fn4_5
  c4_fn4_2 --> c4_fn14_3
  c4_fn4_3 --> c4_fn13_1
  c4_fn4_3 --> c4_fn4_2
  c4_fn4_4 --> c4_fn13_1
  c4_fn4_4 --> c4_fn4_2
  c4_fn4_5 --> c4_fn4_2
  c4_fn4_6 -. callback / referencia .-> c4_fn4_5
  c4_fn7_0 --> c4_fn10_16
  c4_fn7_0 --> c4_fn17_0
  c4_fn8_0 --> c4_fn14_0
  c4_fn8_6 --> c4_fn11_5
  c4_fn8_6 --> c4_fn14_0
  c4_fn8_6 --> c4_fn8_0
  c4_fn8_6 -. callback / referencia .-> c4_fn8_1
  c4_fn8_6 -. callback / referencia .-> c4_fn8_2
  c4_fn8_6 -. callback / referencia .-> c4_fn8_3
  c4_fn8_6 -. callback / referencia .-> c4_fn8_4
  c4_fn8_6 -. callback / referencia .-> c4_fn8_5
  c4_fn9_0 --> c4_fn14_0
  c4_fn9_0 --> c4_fn14_1
  c4_fn9_1 --> c4_fn9_0
  c4_fn9_2 --> c4_fn9_1
  classDef root fill:#183153,color:#fff,stroke:#183153
  classDef file fill:#edf2f7,color:#111,stroke:#64748b
  classDef added fill:#dcfce7,color:#111,stroke:#15803d
  classDef modified fill:#ffedd5,color:#111,stroke:#c2410c
```

</details>

[Inventario completo y límites del análisis](docs/opensc-surface.md).
[Mermaid sin formato Markdown](docs/opensc-surface.mmd).
[Aplicar el parche e instalar la release](INSTALL.md).
<!-- END OPENSC SURFACE -->
