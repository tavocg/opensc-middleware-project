# Superficie modificada de OpenSC

Base master: `fc27ee8768cb882fceb99535dce8b6a966534dcb`. Submódulo: `8462f8b9af0aa468b9e31da849560a9448d0760b`.

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
| `.gitignore` | Existente | 1 | 0 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `etc/opensc.conf.example.in` | Existente | 11 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/Makefile.am` | Existente | 7 | 2 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/Makefile.mak` | Existente | 1 | 1 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/card-iasecc.c` | Existente | 242 | 50 | JCOP3, JCOP4 | `iasecc_match_card` (M)<br/>`iasecc_init` (M)<br/>`iasecc_process_fci` (M)<br/>`iasecc_chipdoc_invalidate_card` (N)<br/>`iasecc_logout` (M)<br/>`iasecc_finish` (M)<br/>`iasecc_is_chipdoc_signing_key` (N)<br/>`iasecc_get_algorithm` (M)<br/>`iasecc_set_security_env` (M)<br/>`iasecc_chipdoc_fresh_verify` (N)<br/>`iasecc_chipdoc_select_for_verify` (N)<br/>`iasecc_pin_verify` (M)<br/>`iasecc_pin_get_policy` (M)<br/>`iasecc_pin_get_info` (M)<br/>`iasecc_pin_change` (M)<br/>`iasecc_pin_reset` (M)<br/>`iasecc_card_reader_lock_obtained` (N)<br/>`sc_get_driver` (M) |
| `src/libopensc/cards.h` | Existente | 1 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/iasecc-chipdoc-bind.c` | Agregado | 65 | 0 | JCOP4 | `iasecc_chipdoc_pkcs15_validate` (N)<br/>`iasecc_chipdoc_pkcs15_activate` (N) |
| `src/libopensc/iasecc-chipdoc-init.h` | Agregado | 136 | 0 | JCOP4 | `iasecc_chipdoc_profile_option` (N)<br/>`iasecc_chipdoc_match` (N)<br/>`chipdoc_unbound_pin` (N)<br/>`chipdoc_unbound_logout` (N)<br/>`chipdoc_metadata_read` (N)<br/>`chipdoc_metadata_record` (N)<br/>`chipdoc_metadata_data` (N)<br/>`iasecc_chipdoc_initialize` (N) |
| `src/libopensc/iasecc-chipdoc-pkcs15.h` | Agregado | 135 | 0 | JCOP4 | `iasecc_chipdoc_pkcs15_key` (N)<br/>`iasecc_chipdoc_pkcs15_operation` (N) |
| `src/libopensc/iasecc-chipdoc-routing.h` | Agregado | 389 | 0 | JCOP4 | `chipdoc_route` (N)<br/>`chipdoc_route_close` (N)<br/>`chipdoc_route_valid` (N)<br/>`chipdoc_route_require` (N)<br/>`chipdoc_route_idle` (N)<br/>`chipdoc_route_pin` (N)<br/>`chipdoc_route_select` (N)<br/>`chipdoc_route_mse` (N)<br/>`chipdoc_route_sign` (N)<br/>`chipdoc_route_read` (N)<br/>`chipdoc_route_record` (N)<br/>`chipdoc_route_data` (N)<br/>`chipdoc_route_logout` (N)<br/>`iasecc_chipdoc_routing_remove` (N)<br/>`chipdoc_route_clear` (N)<br/>`chipdoc_route_finish` (N)<br/>`iasecc_chipdoc_routing_install` (N) |
| `src/libopensc/iasecc-chipdoc-sm.c` | Agregado | 1332 | 0 | JCOP4 | `chipdoc_provider_matches` (N)<br/>`chipdoc_crypto_clear` (N)<br/>`chipdoc_platform_supported` (N)<br/>`chipdoc_platform_ready` (N)<br/>`chipdoc_ready` (N)<br/>`chipdoc_entry` (N)<br/>`chipdoc_channel_owned` (N)<br/>`chipdoc_channel_live` (N)<br/>`chipdoc_signing_ready` (N)<br/>`chipdoc_verify_exchange` (N)<br/>`iasecc_chipdoc_channel_supported` (N)<br/>`chipdoc_discovery_read` (N)<br/>`iasecc_chipdoc_detect` (N)<br/>`iasecc_chipdoc_channel_pin_status` (N)<br/>`chipdoc_setup_exchange` (N)<br/>`iasecc_chipdoc_channel_open` (N)<br/>`iasecc_chipdoc_channel_select_mf` (N)<br/>`chipdoc_bootstrap_exchange` (N)<br/>`iasecc_chipdoc_channel_bootstrap` (N)<br/>`iasecc_chipdoc_transaction_begin` (N)<br/>`iasecc_chipdoc_channel_query_pin` (N)<br/>`iasecc_chipdoc_channel_verify_pin` (N)<br/>`chipdoc_signature_exchange` (N)<br/>`iasecc_chipdoc_channel_select_application` (N)<br/>`iasecc_chipdoc_channel_set_security_env` (N)<br/>`iasecc_chipdoc_channel_compute_signature` (N) |
| `src/libopensc/iasecc-chipdoc-sm.h` | Agregado | 107 | 0 | JCOP4 | Datos / declaraciones / configuración / build |
| `src/libopensc/iasecc-chipdoc-transaction.h` | Agregado | 82 | 0 | JCOP4 | `iasecc_chipdoc_transaction_owns` (N)<br/>`iasecc_chipdoc_transaction_close` (N) |
| `src/libopensc/iasecc-chipdoc.h` | Agregado | 261 | 0 | JCOP4 | `iasecc_chipdoc_profile` (N)<br/>`iasecc_chipdoc_candidate` (N)<br/>`iasecc_chipdoc_transport` (N)<br/>`iasecc_chipdoc_invalidate` (N)<br/>`iasecc_chipdoc_begin_bootstrap` (N)<br/>`iasecc_chipdoc_prepare` (N)<br/>`iasecc_chipdoc_attach` (N)<br/>`iasecc_chipdoc_signing_key` (N)<br/>`iasecc_chipdoc_record` (N) |
| `src/libopensc/iasecc-sdo.c` | Existente | 34 | 13 | JCOP3 | `iasecc_parse_acls` (M)<br/>`iasecc_sdo_free_fields` (M)<br/>`iasecc_parse_docp` (M)<br/>`iasecc_sdo_parse` (M)<br/>`iasecc_docp_copy` (M) |
| `src/libopensc/iasecc-sdo.h` | Existente | 37 | 0 | JCOP3 | `iasecc_parse_compact_acls` (N) |
| `src/libopensc/iasecc.h` | Existente | 56 | 0 | JCOP3, JCOP4 | `iasecc_chipdoc_configured` (N)<br/>`iasecc_chipdoc_jcop3` (N) |
| `src/libopensc/jcop4-channel-agreement.h` | Agregado | 194 | 0 | JCOP4 | `jcop4_ca_pending_clear` (N)<br/>`jcop4_p521_public_valid` (N)<br/>`jcop4_ca_prepare_key_ex` (N)<br/>`jcop4_ca_prepare_ex` (N)<br/>`jcop4_ca_build_mse` (N)<br/>`jcop4_ca_build_ga` (N) |
| `src/libopensc/jcop4-channel-confirmation.h` | Agregado | 179 | 0 | JCOP4 | `jcop4_ca_confirm_d004_session_ex` (N) |
| `src/libopensc/jcop4-channel-controls.h` | Agregado | 41 | 0 | JCOP4 | `jcop4_sm_mf_control` (N) |
| `src/libopensc/jcop4-channel-kdf.h` | Agregado | 60 | 0 | JCOP4 | `jcop4_ca_aes128_kdf_ex` (N) |
| `src/libopensc/jcop4-channel-messages.h` | Agregado | 279 | 0 | JCOP4 | `jcop4_sm_invalidate` (N)<br/>`jcop4_sm_init` (N)<br/>`jcop4_sm_ssc_iv_ex` (N)<br/>`jcop4_sm_append_tlv` (N)<br/>`jcop4_sm_take_tlv` (N)<br/>`jcop4_sm_seal_policy_ex` (N)<br/>`jcop4_sm_open_report_policy_ex` (N) |
| `src/libopensc/jcop4-channel-primitives.h` | Agregado | 152 | 0 | JCOP4 | `jcop4_sm_next_ssc` (N)<br/>`jcop4_sm_pad` (N)<br/>`jcop4_sm_unpad` (N)<br/>`jcop4_sm_aes128_cbc_ex` (N)<br/>`jcop4_sm_cmac_ex` (N)<br/>`jcop4_sm_verify_mac8_ex` (N) |
| `src/libopensc/jcop4-dh-control.h` | Agregado | 53 | 0 | JCOP4 | `d004_control_matches_observation` (N) |
| `src/libopensc/pkcs15-iasecc.c` | Existente | 125 | 1 | JCOP3 | `_iasecc_is_chipdoc` (N)<br/>`_iasecc_fixup_global_pin_paths` (N)<br/>`_iasecc_fixup_signing_key_paths` (N)<br/>`_iasecc_fixup_signature_algorithms` (N)<br/>`_iasecc_parse_df` (M)<br/>`sc_pkcs15emu_iasecc_init` (M) |
| `src/libopensc/pkcs15-pin.c` | Existente | 45 | 1 | JCOP3 | `chipdoc_pin_status_in_place` (N)<br/>`sc_pkcs15_get_pin_info` (M) |
| `src/libopensc/pkcs15.c` | Existente | 32 | 2 | JCOP4 | `sc_pkcs15_bind` (M) |
| `src/pkcs11/framework-pkcs15.c` | Existente | 52 | 7 | JCOP3, JCOP4 | `pkcs15_bind` (M)<br/>`pkcs15_create_pkcs11_objects` (M)<br/>`pkcs15_init_slot` (M)<br/>`slot_get_logged_in_state` (M)<br/>`pkcs15_login` (M)<br/>`pkcs15_logout` (M)<br/>`register_mechanisms` (M) |
| `src/pkcs11/misc.c` | Existente | 15 | 0 | JCOP4 | `reset_login_state` (M) |
| `src/pkcs11/pkcs11-object.c` | Existente | 8 | 0 | JCOP3, JCOP4 | `C_SignInit` (M) |
| `src/pkcs11/pkcs11-session.c` | Existente | 21 | 3 | JCOP3, JCOP4 | `sc_pkcs11_close_session` (M)<br/>`C_Login` (M) |
| `src/pkcs11/slot.c` | Existente | 6 | 3 | JCOP3, JCOP4 | `card_detect` (M) |
| `src/tests/unittests/Makefile.am` | Existente | 3 | 2 | JCOP3, JCOP4 | Datos / declaraciones / configuración / build |
| `src/tests/unittests/iasecc-detection.c` | Agregado | 773 | 0 | JCOP3, JCOP4 | `transmit_metadata` (N)<br/>`reader_success` (N)<br/>`reader_lock` (N)<br/>`reader_unlock` (N)<br/>`reader_reset` (N)<br/>`reader_transmit` (N)<br/>`fallback_match` (N)<br/>`card_success` (N)<br/>`select_public_file` (N)<br/>`encode_objects` (N)<br/>`directory_aid` (N)<br/>`read_public_file` (N)<br/>`configure` (N)<br/>`setup` (N)<br/>`teardown` (N)<br/>`connect_as` (N)<br/>`detect_jcop4` (N)<br/>`bind_jcop4` (N)<br/>`reject_missing_key` (N)<br/>`reject_other_key` (N)<br/>`reject_other_key_size` (N)<br/>`reject_other_pin` (N)<br/>`reject_other_application` (N)<br/>`reject_duplicate_application` (N)<br/>`reject_duplicate_key` (N)<br/>`reject_duplicate_pin` (N)<br/>`reject_shared_atr` (N)<br/>`reject_control` (N)<br/>`reject_control_size` (N)<br/>`reject_curve` (N)<br/>`reject_short_read` (N)<br/>`reject_point` (N)<br/>`preserve_crypto_errors` (N)<br/>`reject_reader_lock` (N)<br/>`reject_reader_unlock` (N)<br/>`reject_transport` (N)<br/>`reject_retry_status` (N)<br/>`reject_get_response` (N)<br/>`explicit_profile` (N)<br/>`explicit_auto` (N)<br/>`unrelated_atr` (N)<br/>`disabled_profile` (N)<br/>`unsupported_transport` (N)<br/>`unsupported_protocol` (N)<br/>`prepare_bind` (N)<br/>`bind_jcop3` (N)<br/>`bind_other_application` (N)<br/>`bind_other_iasecc` (N)<br/>`bind_other_driver` (N)<br/>`bind_explicit_options` (N)<br/>`main` (N) |

Generar de nuevo: `python3 scripts/export-opensc-patch.py`. Verificar sin escribir: añadir `--check`.
