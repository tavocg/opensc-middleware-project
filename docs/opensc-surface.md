# Superficie modificada de OpenSC

Base master: `fc27ee8768cb882fceb99535dce8b6a966534dcb`. Submódulo: `b14152bb016dde201fe3d1807d71655a6ccd91e6`.

Verde = función **nueva respecto a master**; naranja = función existente
modificada. Las flechas de llamadas solo terminan en funciones nuevas: no se
reproduce el grafo de llamadas ya existente en master. Las flechas desde
JCOP3/JCOP4 y desde archivos expresan pertenencia, no llamadas. Punteadas:
referencias/callbacks identificables por nombre, no prueba de ejecución.

La separación sigue las rutas Generic PKI/AMOS (JCOP3) y el perfil optativo
CR RSA2048 (JCOP4); no implica soporte para todas las tarjetas de esas familias.
Archivos compartidos se muestran bajo ambas raíces, pero sus cifras se cuentan
una sola vez. Cada + / - corresponde al **archivo completo**, no a cada perfil.
Headers de declaraciones, datos, configuración y build pueden no tener funciones.
Se incluyen helpers internos y wrappers aún sin llamador en este submódulo;
una función sin flecha entrante no acredita un recorrido activo.

El análisis es léxico: reconoce llamadas directas y referencias explícitas;
no resuelve punteros de función genéricos, macros ni ejecución condicional.
Las implementaciones alternas de una función por preprocesador se agrupan.

El [diagrama completo está en README.md](../README.md#superficie-modificada-de-opensc).

## Inventario por archivo

| Archivo | Estado | + | - | Ruta | Funciones nuevas (N) y modificadas (M) |
| --- | --- | ---: | ---: | --- | --- |
| `etc/opensc.conf.example.in` | Existente | 10 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/Makefile.am` | Existente | 7 | 2 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/Makefile.mak` | Existente | 1 | 1 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/api-trace.h` | Agregado | 93 | 0 | JCOP3, JCOP4 | `sc_api_metadata_trace` (N) |
| `src/libopensc/card-iasecc.c` | Existente | 348 | 51 | JCOP3, JCOP4 | `iasecc_match_card` (M)<br/>`iasecc_init` (M)<br/>`iasecc_trace_selection_target` (N)<br/>`iasecc_select_file` (M)<br/>`iasecc_process_fci` (M)<br/>`iasecc_chipdoc_invalidate_card` (N)<br/>`iasecc_logout` (M)<br/>`iasecc_finish` (M)<br/>`iasecc_is_chipdoc_signing_key` (N)<br/>`iasecc_get_algorithm` (M)<br/>`iasecc_set_security_env` (M)<br/>`iasecc_chipdoc_post_login_probe` (N)<br/>`iasecc_chipdoc_fresh_verify` (N)<br/>`iasecc_chipdoc_select_for_verify` (N)<br/>`iasecc_pin_verify` (M)<br/>`iasecc_pin_get_policy` (M)<br/>`iasecc_pin_get_info` (M)<br/>`iasecc_pin_change` (M)<br/>`iasecc_pin_reset` (M)<br/>`iasecc_compute_signature_at` (M)<br/>`iasecc_card_reader_lock_obtained` (N)<br/>`sc_get_driver` (M) |
| `src/libopensc/card.c` | Existente | 5 | 0 | JCOP3 | `sc_reset` (M)<br/>`sc_lock` (M) |
| `src/libopensc/cards.h` | Existente | 1 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/iasecc-chipdoc-bind.c` | Agregado | 42 | 0 | JCOP4 | `iasecc_chipdoc_pkcs15_activate` (N) |
| `src/libopensc/iasecc-chipdoc-init.h` | Agregado | 121 | 0 | JCOP4 | `iasecc_chipdoc_opted_in` (N)<br/>`chipdoc_unbound_pin` (N)<br/>`chipdoc_unbound_logout` (N)<br/>`chipdoc_metadata_read` (N)<br/>`chipdoc_metadata_record` (N)<br/>`chipdoc_metadata_data` (N)<br/>`iasecc_chipdoc_initialize` (N) |
| `src/libopensc/iasecc-chipdoc-pkcs15.h` | Agregado | 149 | 0 | JCOP4 | `iasecc_chipdoc_pkcs15_key` (N)<br/>`iasecc_chipdoc_pkcs15_operation` (N)<br/>`iasecc_chipdoc_pkcs15_disable_pin_cache` (N) |
| `src/libopensc/iasecc-chipdoc-routing.h` | Agregado | 407 | 0 | JCOP4 | `chipdoc_route` (N)<br/>`chipdoc_route_close` (N)<br/>`chipdoc_route_valid` (N)<br/>`chipdoc_route_require` (N)<br/>`chipdoc_route_idle` (N)<br/>`chipdoc_route_pin` (N)<br/>`chipdoc_route_select` (N)<br/>`chipdoc_route_mse` (N)<br/>`chipdoc_route_sign` (N)<br/>`chipdoc_route_read` (N)<br/>`chipdoc_route_record` (N)<br/>`chipdoc_route_data` (N)<br/>`chipdoc_route_logout` (N)<br/>`iasecc_chipdoc_routing_remove` (N)<br/>`chipdoc_route_clear` (N)<br/>`chipdoc_route_finish` (N)<br/>`iasecc_chipdoc_routing_install` (N)<br/>`iasecc_chipdoc_routing_last_report` (N) |
| `src/libopensc/iasecc-chipdoc-sm.c` | Agregado | 1219 | 0 | JCOP4 | `chipdoc_provider_matches` (N)<br/>`chipdoc_crypto_clear` (N)<br/>`chipdoc_platform_supported` (N)<br/>`chipdoc_platform_ready` (N)<br/>`chipdoc_ready` (N)<br/>`iasecc_chipdoc_channel_supported` (N)<br/>`iasecc_chipdoc_channel_pin_status` (N)<br/>`chipdoc_setup_exchange` (N)<br/>`iasecc_chipdoc_channel_open` (N)<br/>`iasecc_chipdoc_channel_select_mf` (N)<br/>`chipdoc_bootstrap_exchange` (N)<br/>`iasecc_chipdoc_channel_bootstrap` (N)<br/>`iasecc_chipdoc_transaction_begin` (N)<br/>`iasecc_chipdoc_channel_query_pin` (N)<br/>`iasecc_chipdoc_channel_verify_pin` (N)<br/>`chipdoc_signing_ready` (N)<br/>`chipdoc_signature_exchange` (N)<br/>`iasecc_chipdoc_channel_select_application` (N)<br/>`iasecc_chipdoc_channel_set_security_env` (N)<br/>`iasecc_chipdoc_channel_compute_signature` (N) |
| `src/libopensc/iasecc-chipdoc-sm.h` | Agregado | 103 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/iasecc-chipdoc-transaction.h` | Agregado | 82 | 0 | JCOP4 | `iasecc_chipdoc_transaction_owns` (N)<br/>`iasecc_chipdoc_transaction_close` (N) |
| `src/libopensc/iasecc-chipdoc.h` | Agregado | 261 | 0 | JCOP4 | `iasecc_chipdoc_profile` (N)<br/>`iasecc_chipdoc_candidate` (N)<br/>`iasecc_chipdoc_transport` (N)<br/>`iasecc_chipdoc_invalidate` (N)<br/>`iasecc_chipdoc_begin_bootstrap` (N)<br/>`iasecc_chipdoc_prepare` (N)<br/>`iasecc_chipdoc_attach` (N)<br/>`iasecc_chipdoc_signing_key` (N)<br/>`iasecc_chipdoc_record` (N) |
| `src/libopensc/iasecc-sdo.c` | Existente | 34 | 13 | JCOP3 | `iasecc_parse_acls` (M)<br/>`iasecc_sdo_free_fields` (M)<br/>`iasecc_parse_docp` (M)<br/>`iasecc_sdo_parse` (M)<br/>`iasecc_docp_copy` (M) |
| `src/libopensc/iasecc-sdo.h` | Existente | 37 | 0 | JCOP3 | `iasecc_parse_compact_acls` (N) |
| `src/libopensc/iasecc.h` | Existente | 35 | 0 | JCOP3, JCOP4 | `iasecc_chipdoc_configured` (N) |
| `src/libopensc/iso7816.c` | Existente | 9 | 0 | JCOP3 | `iso7816_pin_cmd` (M) |
| `src/libopensc/jcop4-channel-agreement.h` | Agregado | 202 | 0 | JCOP4 | `jcop4_ca_pending_clear` (N)<br/>`jcop4_p521_public_valid` (N)<br/>`jcop4_ca_prepare_key_ex` (N)<br/>`jcop4_ca_prepare_ex` (N)<br/>`jcop4_ca_prepare_key` (N)<br/>`jcop4_ca_prepare` (N)<br/>`jcop4_ca_build_mse` (N)<br/>`jcop4_ca_build_ga` (N) |
| `src/libopensc/jcop4-channel-confirmation.h` | Agregado | 198 | 0 | JCOP4 | `jcop4_ca_confirm_d004_session_ex` (N)<br/>`jcop4_ca_confirm_d004_session` (N)<br/>`jcop4_ca_confirm_d004` (N) |
| `src/libopensc/jcop4-channel-controls.h` | Agregado | 41 | 0 | JCOP4 | `jcop4_sm_mf_control` (N) |
| `src/libopensc/jcop4-channel-kdf.h` | Agregado | 68 | 0 | JCOP4 | `jcop4_ca_aes128_kdf_ex` (N)<br/>`jcop4_ca_aes128_kdf` (N) |
| `src/libopensc/jcop4-channel-messages.h` | Agregado | 406 | 0 | JCOP4 | `jcop4_sm_invalidate` (N)<br/>`jcop4_sm_init` (N)<br/>`jcop4_sm_ssc_iv_ex` (N)<br/>`jcop4_sm_ssc_iv` (N)<br/>`jcop4_sm_append_tlv` (N)<br/>`jcop4_sm_take_tlv` (N)<br/>`jcop4_sm_seal_policy_ex` (N)<br/>`jcop4_sm_seal_policy` (N)<br/>`jcop4_sm_seal` (N)<br/>`jcop4_sm_decode_name` (N)<br/>`jcop4_sm_open_report_policy_ex` (N)<br/>`jcop4_sm_open_report_policy` (N)<br/>`jcop4_sm_open_report` (N)<br/>`jcop4_sm_open` (N) |
| `src/libopensc/jcop4-channel-primitives.h` | Agregado | 172 | 0 | JCOP4 | `jcop4_sm_next_ssc` (N)<br/>`jcop4_sm_pad` (N)<br/>`jcop4_sm_unpad` (N)<br/>`jcop4_sm_aes128_cbc_ex` (N)<br/>`jcop4_sm_cmac_ex` (N)<br/>`jcop4_sm_verify_mac8_ex` (N)<br/>`jcop4_sm_aes128_cbc` (N)<br/>`jcop4_sm_cmac` (N)<br/>`jcop4_sm_verify_mac8` (N) |
| `src/libopensc/jcop4-dh-control.h` | Agregado | 53 | 0 | JCOP4 | `d004_control_matches_observation` (N) |
| `src/libopensc/opensc.h` | Existente | 2 | 0 | JCOP3 | Datos / declaraciones / configuración / build |
| `src/libopensc/pkcs15-iasecc.c` | Existente | 132 | 1 | JCOP3 | `_iasecc_is_chipdoc` (N)<br/>`_iasecc_fixup_global_pin_paths` (N)<br/>`_iasecc_fixup_signing_key_paths` (N)<br/>`_iasecc_fixup_signature_algorithms` (N)<br/>`_iasecc_parse_df` (M)<br/>`sc_pkcs15emu_iasecc_init` (M) |
| `src/libopensc/pkcs15-pin.c` | Existente | 53 | 1 | JCOP3 | `chipdoc_pin_status_in_place` (N)<br/>`sc_pkcs15_get_pin_info` (M) |
| `src/libopensc/pkcs15-sec.c` | Existente | 9 | 2 | JCOP3, JCOP4 | `use_key` (M) |
| `src/libopensc/pkcs15.c` | Existente | 14 | 0 | JCOP4 | `sc_pkcs15_bind` (M) |
| `src/pkcs11/framework-pkcs15.c` | Existente | 97 | 4 | JCOP3, JCOP4 | `pkcs15_bind` (M)<br/>`C_GetTokenInfo` (M)<br/>`pkcs15_create_pkcs11_objects` (M)<br/>`pkcs15_init_slot` (M)<br/>`pkcs15_chipdoc_session_state` (N)<br/>`slot_get_logged_in_state` (M)<br/>`pkcs15_login` (M)<br/>`pkcs15_logout` (M)<br/>`pkcs15_prkey_sign` (M)<br/>`register_mechanisms` (M) |
| `src/pkcs11/misc.c` | Existente | 15 | 0 | JCOP4 | `reset_login_state` (M) |
| `src/pkcs11/pkcs11-global.c` | Existente | 3 | 0 | JCOP3, JCOP4 | `C_GetSlotInfo` (M) |
| `src/pkcs11/pkcs11-object.c` | Existente | 14 | 0 | JCOP3, JCOP4 | `C_GetAttributeValue` (M)<br/>`C_FindObjectsInit` (M)<br/>`C_SignInit` (M)<br/>`C_Sign` (M)<br/>`C_SignUpdate` (M)<br/>`C_SignFinal` (M) |
| `src/pkcs11/pkcs11-session.c` | Existente | 27 | 3 | JCOP3, JCOP4 | `C_OpenSession` (M)<br/>`sc_pkcs11_close_session` (M)<br/>`C_CloseSession` (M)<br/>`C_CloseAllSessions` (M)<br/>`C_GetSessionInfo` (M)<br/>`C_Login` (M)<br/>`C_Logout` (M) |
| `src/pkcs11/sc-pkcs11.h` | Existente | 1 | 0 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/pkcs11/slot.c` | Existente | 9 | 3 | JCOP3, JCOP4 | `card_detect` (M)<br/>`slot_token_removed` (M) |
| `src/tools/pkcs11-tool.c` | Existente | 5 | 0 | JCOP3 | `sign_data` (M) |

Generar de nuevo: `python3 scripts/export-opensc-patch.py`. Verificar sin escribir: añadir `--check`.
