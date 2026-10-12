# libjcop3p11: contexto para continuar

Estado actualizado el **12 de octubre de 2026**, después de revisar el
fallo GUI de reinserción rápida de las **01:46 de Costa Rica** (07:46 UTC),
captura `local/jcop3-firmador-api-y17eLwTD/`, OpenSC `1736ce3a9` limpio.
Los 260 eventos muestran primera firma OK, diez lecturas PRESENT|CHANGED,
ninguna limpieza del token, segundo login CKR_DEVICE_REMOVED antes de VERIFY
y sesión final inválida; Java vuelve a fallar con Token.secretCache null.
**Se encontró y corrigió localmente el atajo de card_detect() que ignoraba
CARD_CHANGED cuando la espera de eventos reportaba timeout/otro lector.**
La corrección exige procesar el cambio, limpiar sesiones y reconectar, sin
heredar autenticación. El presupuesto de reintentos ahora es local a cada
invocación; antes se agotaba entre cambios sucesivos. No modifica PIN/RSA.
Pasan 256 regresiones sin tarjeta y la compilación completa sin PC/SC.
La corrección aún necesita compilarse y probarse con la tarjeta del usuario.
El pase con esperas de las 01:39 sigue verificado: dos PDF/sellos; no prueba
reinserción rápida. El PDF de control de las 01:46 también se verificó, sin
lector, PIN ni red; no hay PDF después de la reinserción fallida.
Git al iniciar esta revisión: principal `9bd4570`, submódulo `1736ce3a9`, limpios.
El usuario prefiere conservar la GUI. CLI con stdin sigue preparado y
sin prueba real; el agente no ejecuta pass ni firmas y no dispone del lector.
La candidata de firma sigue optativa. No afirmar recuperación en hardware,
soporte completo o confianza/revocación solo por las firmas verificadas.
Leer este archivo al retomar y contrastar Git y las capturas más recientes.

## Objetivo y alcance

Desarrollar soporte libre, sobre OpenSC, para una tarjeta **JCOP3 de firma
digital de Costa Rica**, mediante su interfaz de contacto. El usuario tiene
la tarjeta y un lector USB. El primer hito es obtener una firma RSA SHA-256
con `LlaveDeFirma` y verificarla con OpenSSL. **Ese primer hito se completó**
con RSA-PKCS y DigestInfo SHA-256 el 11 de octubre a las 20:18 de Costa Rica.
Firmador ya produjo PDF verificados tanto con el experimento de omisión
como con la candidata que mantiene consultas físicas sin reselección.
Ahora se validarán los límites de esta corrección antes de preparar cambios
para OpenSC.

La cadena de comunicación es:

```text
firmador → Cryptoki/PKCS#11 → OpenSC → PC/SC → CCID → lector USB → tarjeta
                                                APDU hacia la tarjeta
```

El nombre elegido por el usuario es **libjcop3p11**. La implementación actual
amplía el driver existente **`iasecc`**; no existe un `card-jcop3.c` ni un módulo
independiente renombrado. La biblioteca compilada sigue siendo:

```text
build/opensc/src/pkcs11/.libs/opensc-pkcs11.so
```

JCOP3 identifica la plataforma, pero la compatibilidad depende de la aplicación
interna y de sus comandos. No asumir soporte de todas las JCOP3. JCOP4,
contactless, CAN/PACE y criptografía poscuántica quedan fuera del primer hito.

## Cómo colaborar con el usuario

- Responder en español. Investigar, editar y verificar de manera autónoma
  mientras sea posible; dar comandos concretos cuando se necesite el lector,
  la tarjeta o una acción desde la terminal del usuario.
- Usar **`nix develop` y objetivos Make**. El prompt conserva el PS1 del usuario
  y antepone `(libjcop3p11)`.
- Mantener commits incrementales, pero **dar al usuario los comandos para
  hacerlos**. No usar `build-commits` ni repositorios alternativos para eludir
  la restricción de escritura de `.git`.
- No pedir nuevamente permiso para investigación o cambios locales ya
  autorizados. El usuario pidió documentar el estado antes de continuar:
  retomar desde los informes existentes, sin repetir automáticamente pruebas.
- En la sesión de desarrollo hasta este punto, el agente no tenía acceso al
  lector y `.git` era de solo lectura. Las pruebas reales las ejecutó el usuario.
- No delegar a otros agentes salvo que se solicite explícitamente.

## Entorno y archivos

Directorio de trabajo: `/home/tavo/Projects/middleware-project`.

| Ruta | Función |
| --- | --- |
| `flake.nix`, `flake.lock` | Herramientas y dependencias fijadas de Nix |
| `Makefile` | Entrada a compilación, consultas y firma de prueba |
| `vendor/opensc/` | Submódulo, rama local `cr-jcop3` |
| `scripts/build-opensc.sh` | Compilación fuera del árbol con PC/SC |
| `scripts/probe-jcop3.sh` | Consultas e informes sin introducir PIN |
| `scripts/sign-jcop3.sh` | Firma interactiva de texto y verificación OpenSSL |
| `scripts/sha256-digestinfo.sh` | Prepara DigestInfo SHA-256 DER para `RSA-PKCS`, sin PIN |
| `scripts/firmador-jcop3.sh` | Abre firmador con módulo local y configuración PKCS#15, sin depuración APDU |
| `scripts/test-iasecc.sh`, `tests/` | Regresiones sin tarjeta |
| `docs/jcop3.md` | Evidencia técnica y evolución de la investigación |
| `docs/idopte.md` | Localización del módulo privativo instalado |
| `local/` | Capturas originales y resultados; excluido de Git |
| `build/opensc/` | Compilación con PC/SC usada por el usuario |
| `build/opensc-offline/` | Compilación de comprobación sin PC/SC |

Consultar las secciones más recientes de `docs/jcop3.md`: algunos pasos
pendientes en secciones antiguas ya se realizaron. No ejecutar de nuevo los
comandos de inicialización Git del README en este repositorio existente.

El entorno incluye OpenSC empaquetado, pcsc-tools, compilador, autotools,
pkg-config, OpenSSL, pcsclite y zlib. En NixOS se necesita el servicio PC/SC
(`services.pcscd.enable = true;`). Compilar no instala nada en el sistema:

```bash
nix develop
make test/iasecc JOBS="$(nproc)"
```

`JOBS` vale 2 por defecto. `READER=0` selecciona lector PC/SC y `SLOT_INDEX=0`
selecciona slot PKCS#11; no son necesariamente el mismo índice.
`inspect/jcop3-pkcs11`, los diagnósticos de política, `sign/jcop3`,
`sign/jcop3-sha256`, `sign/jcop3-pin-status`, `sign/jcop3-select-root`,
`firmador/jcop3`, `firmador/jcop3-diagnose`, `firmador/jcop3-session-state` y
`firmador/jcop3-pin-status-in-place` y `firmador/jcop3-cli` utilizan
la compilación existente: **no recompilan**.

Las restricciones del entorno del agente pueden impedir usar el daemon de
Nix o acceder a red. Una compilación sin PC/SC sirve para comprobar código,
pero no valida comunicación con la tarjeta.

## Git al momento de esta documentación

Los últimos commits guardados son:

- Repositorio principal: **`9bd4570`**, `Document reinsertion success`.
- Submódulo OpenSC: **`1736ce3a9`**, `Trace token removal`.
- Base upstream original: `c7a377af971df03f879c1697e91ebac1f7e28c56`.

Ambos árboles estaban limpios al comenzar a revisar la captura de las 01:46.
La candidata y las trazas ya están compiladas y comprometidas. Los cambios
locales nuevos corrigen detección de cambios/reconexión en slot.c y agregan
19 regresiones de slot/sesiones con lector y framework simulados; también
actualizan documentación. Falta compilar con PC/SC y probar la corrección
GUI en hardware. Dar comandos de commit: submódulo primero y principal
con tests, documentos y nuevo puntero. No hay commits nuevos del agente.

La URL del submódulo aún es `https://github.com/OpenSC/OpenSC.git`. Los commits
locales del driver necesitarán publicarse en un fork y actualizar esa URL
para que otro clon pueda obtenerlos. No se ha enviado un issue ni un PR.
Al continuar, revisar ambos `git status` y conservar cualquier trabajo nuevo;
no restablecer ni actualizar automáticamente el submódulo. Dar al usuario
los comandos de commit; el agente no escribe en `.git`.

## Identidad y objetos comprobados

- Lector: `HID Global OMNIKEY 3x21 Smart Card Reader`, índice observado 0.
- ATR: `3b:df:18:ff:81:91:fe:1f:c3:00:31:b8:64:0c:01:ec:c1:73:94:01:80:82:90:00:b3`.
  Coincide exactamente con `SC_CARD_TYPE_IASECC_AMOS` de OpenSC. La tarjeta
  eOI es un precedente útil, pero no se debe asumir que usa el mismo perfil.
- AID raíz: **`F0496173456363526F6F74`** (`IasEccRoot`).
- AID de `ChipDoc`: **`E828BD080FD25047656E65726963`**, reconocido como
  `ECC Generic PKI` por OpenSC.
- EF.DIR tiene 256 bytes: un registro ChipDoc y relleno. ODF `5031`,
  TokenInfo `5032`, AODF `7001`, PuKDF `7004`, CDF `7005`.
- Certificados legibles: `001C`, `LlaveDeFirma`, RSA 2048; `001D`,
  `LlaveDeAutenticacion`, RSA 2048.
- PrKDF declara referencias privadas **3 para firma** y **2 para autenticación**.
  Sus permisos PKCS#15 no demuestran todavía cómo realizar la operación RSA.
- ID PKCS#11 de firma: **`4c6c61766544654669726d61`** (`LlaveDeFirma`).
- `Card PIN`: CHV global, referencia **1**; auth ID `436172642050494e`.
  AODF declara mínimo 4 y máximo 20; los atributos públicos CHV indican
  **mínimo 4 y máximo 14**, que usa el driver. No ajustar un PIN existente.
- Existe `Card PUK` como AuthKey. No se ha probado ni se debe utilizar para
  desbloqueo en esta investigación.

PKCS#11 enumera dos certificados y dos llaves públicas RSA, derivadas de los
certificados. Los objetos privados no visibles sin login son esperables.
PuKDF aún produce `-1402`, `Required ASN.1 object not found`; su posible
relleno no se ha resuelto. No afirmar que esta parte funciona completamente.
El módulo anuncia Cryptoki 3.0 y mecanismos RSA, incluido `SHA256-RSA-PKCS`.
Anunciar un mecanismo no demuestra que funcione en esta tarjeta.
La prueba de las 19:41 falla con `SHA256-RSA-PKCS` después de pasar el login.
El helper ahora usa `RSA-PKCS` con DigestInfo SHA-256 y esa ruta se verificó
en hardware. El mecanismo combinado también produjo una firma verificada
después de la corrección de negociación (`34a040c32`). No se implementó
QSign con hashing en hardware.

## Cambios realizados y límites

1. **ACL compactas de FCP** (`6226bd256`): el parser solo procesaba el primer
   grupo y perdía READ, impidiendo EF.DIR. Ahora recorre todos los grupos,
   valida truncamientos y rechaza alternativas contradictorias que no puede
   representar. La tarjeta confirmó lectura de certificados.
2. **Contexto del PIN global** (`ea864361d`): la ruta PKCS#15 `0001` no es
   seleccionable (`6A82`). Una corrección limitada a Amos + ChipDoc + CHV 1
   global sustituye esa ruta por MF `3F00` en `pkcs15-iasecc.c`.
3. **Conservación de SDO** (`512084237`): `iasecc_sdo_parse_data()` devuelve
   bytes consumidos positivos al tener éxito. El llamador los trataba como
   error y borraba clase/referencia. La comprobación ahora es `rv < 0`.
4. **Política ChipDoc observada** (`840a7b204`): `iasecc-sdo.c/.h` conserva
   `C9 01 01` como `chipdoc_c9`, sin asignarle un significado. Solo acepta
   ese valor en Amos, CHV 1. Para contacto reconoce únicamente la ACL
   capturada `F3 E7 00 E7 FF 00 D0 D3 D3`, conserva los bytes originales y
   expone VERIFY y GET DATA. Otras operaciones no están soportadas; variantes
   de esa ACL se rechazan. No implementa la política contactless.
   `card-iasecc.c` marca `read_verify_only` y bloquea cambio/desbloqueo antes
   de cualquier autenticación.
5. **Todos los grupos de ACL de SDO** (`0c3e1c4f5`):
   `iasecc_parse_acls()` también perdía los grupos posteriores al primero.
   Ahora conserva todas las condiciones de los DOCP RSA capturados, acepta
   repeticiones idénticas y rechaza alternativas distintas y truncamientos.
   Las condiciones de firma, autenticación interna y descifrado apuntan al
   entorno de seguridad 3. No se ejecutaron operaciones administrativas.
6. **Ruta de la llave de firma** (`fb767b3dd`):
   `_iasecc_fixup_signing_key_paths()` conserva el AID ChipDoc y la referencia
   nativa RSA 3, eliminando la ruta de archivo `0003`, cuya selección devuelve
   `6A82`. Se limita a Amos + Generic PKI + RSA nativa de 2048 bits y ese AID
   en la ruta. La llave 2 y otros perfiles quedan intactos. La corrección se
   aplica al cargar PrKDF y a los objetos ya cargados en la inicialización.
7. **Referencias medidas y DOCP público** (`70d7afc20`):
   `card-iasecc.c` resuelve HT SHA-256 `40`, DST RSA/SHA-256 `42` y AT RSA_PKCS
   `02` cuando no hay tabla declarada. Se limita a Amos, DF seleccionado con
   el AID Generic PKI, RSA 2048 y referencia de llave 3. Tablas declaradas y
   otros perfiles no reciben fallback. La preparación de esa llave lee solo
   DOCP, comprueba tamaño 256 bytes y libera los metadatos; no solicita `7F48`.
   La operación AT RSA-PKCS produjo firmas SHA-256 verificadas tanto con
   DigestInfo explícito como mediante el mecanismo combinado.

La condición `00` de VERIFY permite ejecutar el comando sin autenticación
previa; **no acredita el PIN ni evita su comprobación por la tarjeta**.
El significado de `C9` y las condiciones administrativas siguen pendientes.

La corrección de ruta depende de la emulación PKCS#15. Los scripts generan
configuración local con **`try_emulation_first = yes`** y
**`use_file_caching = no`**. El binding nativo por defecto puede omitir esa
corrección: al integrar con firmador habrá que conservar esta configuración
o resolver la selección del perfil. No basta con cambiar la ruta de la .so.
El nuevo objetivo `make firmador/jcop3` proporciona esa configuración local.

8. **Hash SHA-256 en software para ChipDoc** (`34a040c32`):
   `_iasecc_fixup_signature_algorithms()` en `pkcs15-iasecc.c` retira el flag
   SHA-256 de la capacidad RSA 2048 cuando coincide el perfil y la llave
   observados, no hay algoritmos declarados y se dispone de PKCS#1/HASH_NONE
   sin RSA RAW. Así PKCS#15 prepara DigestInfo y usa AT RSA-PKCS sobre el
   digest que PKCS#11 ya calculó. Preserva SHA-1, otros tamaños/perfiles y
   algoritmos declarados. La capacidad RSA 2048 es compartida por las llaves;
   este ajuste no valida la llave de autenticación. No modifica PIN ni ACL.

## Verificación y últimas capturas

Hay **256 comprobaciones de regresión** en trece fixtures C: `iasecc-fci.c`,
`iasecc-pin-path.c`, `iasecc-key-path.c`, `iasecc-sdo.c`, `iasecc-pin-policy.c`
y `iasecc-sign-setup.c`, más `iasecc-rsa-pkcs.c`, `pkcs11-api-trace.c`,
`pkcs11-session-state.c`, `iasecc-context-trace.c`,
`iasecc-post-login-status.c`, `pkcs15-pin-status-in-place.c`,
`pkcs11-card-change.c`, y los scripts
`sha256-digestinfo.sh`, `firmador-launcher.sh` y `pkcs11-sign-fallback.sh`, este último con un módulo C
PKCS#11 completamente simulado. Incluyen respuestas
capturadas, conservación/copia de metadatos, rechazo de perfiles distintos y
transporte simulado que verifica que cambio/desbloqueo no autentican.
Pasaron en la comprobación local; también terminó la compilación completa
sin PC/SC, registrada en `build/opensc-offline/card-change-build.log`.
Las últimas regresiones están en `build/opensc-offline/card-change-tests.log`.
Esto no sustituye pruebas de firma en hardware.
El lanzador de firmador, en modos normal, diagnóstico y experimento de sesión,
pasó sintaxis y una ejecución con
`firmador` simulado: selección del módulo, sustitución de configuración/debug
heredados, perfil idéntico al de la firma, permisos 0600 y propagación del
código de salida. Los modos normal y diagnóstico eliminan
`OPENSC_JCOP3_SESSION_STATE` heredado; el normal también elimina `OPENSC_API_TRACE_FILE`;
el diagnóstico crea directorio 0700 y archivos 0600. Esa comprobación no
abrió Java ni accedió al lector.

| Captura local | Resultado relevante |
| --- | --- |
| `jcop3-inspection-WAZLkGL7.txt` | Certificados legibles tras corregir FCP |
| `jcop3-signature-S8Pd9rPG/report.txt` | Intento anterior que llegó a `C_Login`: falla con `CKR_GENERAL_ERROR`; no hay firma |
| `jcop3-pin-context-6Hd0jGpP.txt` | CHV 1 accesible en raíz; `0001` no seleccionable |
| `jcop3-pkcs11-gYBEWLz8.txt` | SDO ya conserva CHV 1, pero parser antiguo rechaza `C9` |
| `jcop3-pin-policy-9U5otg3U.txt` | Selecciones anidadas devuelven DOCP entero; controles idénticos, 7/7 intentos |
| **`jcop3-pkcs11-cRVr2acc.txt`** | Última consulta, 21:34:21 UTC, OpenSC `840a7b204`: política del PIN leída, límites 14/4 e intentos 7/7 |
| **`jcop3-key-policy-egE7wr1j.txt`** | Última consulta, 21:34:35 UTC: SELECT ChipDoc y GET DOCP de RSA_PRIVATE 2 y 3 devuelven `9000` |
| **`jcop3-security-env-MMjEmIcC.txt`** | 12 de octubre, 00:31:57 UTC (11 de octubre, 18:31 en Costa Rica): PC/SC no encuentra lectores; ninguna consulta SE/ruta se ejecutó |
| **`jcop3-security-env-yw7ohMX7.txt`**, **`jcop3-security-env-OQCXHYVH.txt`** | 12 de octubre, 00:49:16 y 00:49:19 UTC: seis respuestas idénticas; SE 3 legible en ambos AID, AT USER PASSWORD referencia 1; SELECT `0003` devuelve `6A82` |
| **`jcop3-pkcs11-OwOOGiF2.txt`** | 12 de octubre, 01:16:15 UTC: corrección de ruta de firma aplicada por PKCS#15 y PKCS#11; límites PIN 14/4, intentos 7/7 |
| **`jcop3-algorithms-FinI8oaD.txt`** | 12 de octubre, 01:16:28 UTC: nueve respuestas `9000`; HT `40`, DST `42` y AT `02` aceptados; DOCP CHV 1 antes/después idénticos, 7/7 intentos |
| **`jcop3-signature-FktWTv57/report.txt`**, **`jcop3-signature-jQYkimJu/report.txt`**, **`jcop3-signature-54OpXE2f/report.txt`** | 12 de octubre, 01:28:26, 01:28:46 y 01:28:53 UTC, OpenSC `70d7afc20`: `No slots.` en lectura del certificado; no se ejecutó el comando autenticado |
| **`jcop3-pkcs11-XtNrgvHO.txt`** | 12 de octubre, 01:39:13 UTC, OpenSC `70d7afc20`: lector y ATR visibles, slot 0 presente, dos certificados y dos llaves públicas RSA; correcciones de rutas aplicadas, intentos PIN 7/7 |
| **`jcop3-signature-CXq9hBMQ/report.txt`** | 12 de octubre, 01:41:29 UTC: certificado leído y login aceptado; `C_SignFinal` devuelve `CKR_FUNCTION_NOT_SUPPORTED`; no hay firma ni verificación OpenSSL |
| **`jcop3-signature-Pwfndadv/report.txt`** | 12 de octubre, 01:59:58 UTC: helper `RSA-PKCS`, certificado y DigestInfo preparados; `C_Login` devuelve `CKR_PIN_LEN_RANGE`; no se llegó a la firma |
| **`jcop3-pkcs11-EqveuSwF.txt`** | 12 de octubre, 02:12:28 UTC: mismo módulo y ATR, slot 0 y objetos RSA visibles; AODF 4–20, CHV 4–14, intentos 7/7; no hubo login |
| **`jcop3-signature-crUXrnAi/report.txt`** | 12 de octubre, 02:18:39 UTC: login y RSA-PKCS exitosos, firma de 256 bytes, OpenSSL `Verified OK`, estado 0; primera firma verificada |
| **`jcop3-signature-kiHbc1Qh/report.txt`** | 12 de octubre, 02:37:33 UTC, OpenSC `34a040c32`: login y SHA256-RSA-PKCS exitosos, texto original como entrada, firma de 256 bytes, OpenSSL `Verified OK`, estado 0 |
| **`jcop3-firmador-api-1LBmDPbP/api.log`** | 12 de octubre, 03:06:20 UTC, OpenSC `f7752be36`: un login exitoso, PIN_STATUS logged in, sesiones RW USER, SHA256-RSA-PKCS y llave 3; `C_SignFinal` devuelve `CKR_USER_NOT_LOGGED_IN`; no hay logout/cierre intermedio |
| **`jcop3-firmador-api-hBIzcp79/api.log`** | 12 de octubre, 03:24:32 UTC, OpenSC `a19ee5927`: un login exitoso; llave 3; selección, DOCP y MSE pasan; RSA AT devuelve `6982`, traducido a `SC_ERROR_SECURITY_STATUS_NOT_SATISFIED (-1211)` y `CKR_USER_NOT_LOGGED_IN`; no reintento RSA |
| **`jcop3-firmador-api-HQ6utMiv/api.log`** | 12 de octubre, 03:39:50 UTC, OpenSC `ca8f65684`: experimento activo; `C_GetSessionInfo` omite consultas físicas; login, selección y MSE pasan; RSA AT sigue devolviendo `6982`; un login y una firma fallida |
| **`jcop3-firmador-api-D83Fsxrk/api.log`** | 12 de octubre, 03:54:31 UTC, OpenSC `a3b001ea2`: un login exitoso; una consulta token y cuatro consultas sesión después del login; 15 SELECT raíz y un SELECT ChipDoc; sin reset detectado; RSA AT devuelve `6982` |
| **`jcop3-firmador-api-WV8KnIjI/api.log`** | 12 de octubre, 05:34:00 UTC, OpenSC `da1edb457`: experimento sesión+token activo; login OK, llave 3, SHA256-RSA-PKCS; ninguna selección raíz después del login; RSA AT `9000`, retorno 256 y `C_SignFinal CKR_OK`; firma PDF y sello verificados posteriormente con OpenSSL |
| **`jcop3-signature-rwfMn3zh/report.txt`**, **`jcop3-signature-ELmY1jCa/report.txt`** | 12 de octubre, 06:02:54 y 06:03:14 UTC, OpenSC `0e7549960`: lectura del certificado detenida en `C_OpenSession CKR_TOKEN_NOT_PRESENT`, slot 0; no login ni consulta experimental |
| **`jcop3-signature-QfdEBgds/report.txt`**, **`api.log`** | 12 de octubre, 06:04:31 UTC: consulta vacía inmediatamente después del VERIFY con retorno 0 y estado LOGGED_IN; login OK; llave 3; RSA AT `9000`, 256 bytes; OpenSSL `Verified OK`, estado 0 |
| **`jcop3-signature-zPoqBbBZ/api.log`**, **`jcop3-signature-rb2JHk1X/api.log`** | 12 de octubre, 06:13:20 y 06:13:35 UTC, OpenSC `416c11c7b`: un SELECT raíz aislado `9000` después de VERIFY; login OK; cada ejecución tiene dos rechazos RSA `6982` debido al fallback multipart del CLI; no firma ni segundo login |
| **`jcop3-firmador-api-MWUUuIrV/api.log`** | 12 de octubre, 06:37:01 UTC, OpenSC `23b3d25fa`: candidata sin selección, login OK, cuatro consultas físicas autenticadas, cero SELECT raíz después del login, RSA AT `9000`, 256 bytes y C_SignFinal OK; nuevo PDF y sello verificados |
| **`jcop3-firmador-api-UpCXoCn0/api.log`** | 12 de octubre, 06:47:51 UTC, OpenSC `23b3d25fa`: un login y una firma RSA exitosa, cuatro consultas físicas autenticadas |
| **`jcop3-firmador-api-LB7oJvqS/api.log`** | 12 de octubre, 06:48:43 UTC: primera firma exitosa, logout software, CHV sigue autenticado, un SELECT raíz antes de segundo login exitoso; segunda RSA `6982`; primer PDF consecutivo y sello verificados |
| **`jcop3-firmador-api-fgRfkdCF/api.log`** | 12 de octubre, 07:07:46 UTC, OpenSC `381c469b2`: dos logins con VERIFY real, logout intermedio, consultas físicas autenticadas sin reselección, ambas RSA `9000` de 256 bytes; los dos PDF y sellos se verificaron |
| **`jcop3-firmador-api-9ujB6ts6/api.log`** | 12 de octubre, 07:15:42 UTC: primera firma OK y PDF verificado; tras retirada/reinserción cinco bloqueos -1105, segundo login CKR_DEVICE_REMOVED antes de VERIFY y sesión final inválida; Java NullPointerException Token.secretCache null |
| **`jcop3-firmador-api-oJtaNn1s/api.log`** | 12 de octubre, 07:39:06 UTC: 430 eventos; ausencia y limpieza de token observadas, nueva presencia, dos VERIFY reales y dos RSA de 256 bytes; ambos PDF y sellos verificados. Base 381c469b2 con trazas modificadas, luego comprometidas como 1736ce3a9 |
| **`jcop3-firmador-api-y17eLwTD/api.log`** | 12 de octubre, 07:46:21 UTC, 1736ce3a9 limpio: 260 eventos, primera firma verificada, diez CARD_PRESENCE=3 sin limpieza del token, segundo login 0x32 antes de VERIFY y sesión 0xB3; regresión reproducida y corrección local preparada |

Los informes PKCS#11 y DOCP RSA de las 21:34 UTC ya se revisaron: **no pedir ejecutarlos de nuevo como
primer paso**. Los DOCP RSA se reprodujeron en las regresiones del parser
corregido; ambos declaran tamaño 256 bytes y ACL idénticas. La condición de
firma `13` significa autenticación de usuario mediante SE 3 según OpenSC;
las nuevas capturas confirman que su AT refiere al CHV 1. Las consultas solicitaron solo
DOCP `A0`, nunca la plantilla `7F48` ni componentes privados.

TokenInfo no incluye `supportedAlgorithms`, y las llaves muestran
`Algo_refs: 0`. Se preparó el fallback limitado a las tres referencias MSE
aceptadas. La corrección de ruta ya aparece en la enumeración de la tarjeta.
Hubo tres ejecuciones del helper a las 19:28, pero todas se
detuvieron en la enumeración de slots previa a leer el certificado. Ninguna
ejecutó `--login --sign` ni verificó un PIN. Los informes no distinguen entre
lector no visible, servicio PC/SC inaccesible o configuración de slots.
El diagnóstico posterior `jcop3-pkcs11-XtNrgvHO.txt` sí detecta lector,
tarjeta y slot 0 con el mismo módulo y proveedor PC/SC. No se reprodujo
`No slots.` y su causa sigue sin confirmarse. Persiste el aviso conocido
de PuKDF, pero no impide esa enumeración. No hubo autenticación; el PIN
conserva 7/7 intentos en ese diagnóstico previo al último login.
La ejecución de las 19:41 sí pasó el login y falló en la firma. No hay captura
APDU autenticada que confirme el comando exacto o el punto físico del fallo.
La regresión reproduce `SC_ERROR_NOT_SUPPORTED` en QSign SHA-256 con OpenSSL 3
después de preparar HT/DST. El helper evita ese camino con `RSA-PKCS` y un
DigestInfo de 51 bytes; la tarjeta debe hacer el padding PKCS#1 v1.5 y RSA
mediante la operación AT ya presente en el driver. No se añadió una corrección
de QSign al submódulo. La cadena combinada requiere revisar también cómo
PKCS#11 calcula el hash al faltar referencias declaradas de algoritmos.
Aceptar MSE no demuestra una operación RSA correcta.

El usuario confirmó que pasan las pruebas con el helper actualizado. El nuevo
intento de las 19:59 se detuvo en `C_Login` con `CKR_PIN_LEN_RANGE (0xa2)`.
No demuestra un fallo de la ruta RSA-PKCS ni un PIN incorrecto. En el camino
de código revisado, este error de longitud se produce antes del VERIFY con
el PIN. `_validate_pin()` valida primero el rango PKCS#15 declarado en AODF
(4–20), distinto del límite público CHV (4–14) que aplica después el driver.
No alterar ninguno de esos límites para eludir el error.
El usuario no puede confirmar si introdujo el mismo PIN que en el login
exitoso anterior; cree que sí. La lectura de `util_getpass()` y el paso de
su resultado a `C_Login` no muestran un cambio entre ambos intentos.
La consulta posterior `jcop3-pkcs11-EqveuSwF.txt` confirma que los límites
permanecen iguales y los intentos siguen en 7/7. Lector, ATR, slot 0 y objetos
RSA son visibles; persiste PuKDF `-1402`, sin bloquear la enumeración.
No hubo login en esa consulta ni hay captura autenticada. No se confirmó
la causa de la entrada rechazada y no se cambiaron límites ni código.
El intento posterior de las 20:18 pasó el login y produjo la primera firma
RSA SHA-256 verificada. Se compararon la llave pública extraída del certificado
y el DigestInfo recalculado con los archivos guardados; una comprobación
OpenSSL independiente también devolvió `Verified OK`. No repetir esa firma
como primer paso. La causa de la entrada rechazada anterior sigue sin
confirmarse; no se modificó el tratamiento del PIN para obtener este éxito.

El primer intento del firmador usando el lanzador local falló con
`sun.security.pkcs11.wrapper.PKCS11Exception: CKR_FUNCTION_NOT_SUPPORTED`,
seguido de `signatureValue cannot be null!` y un aviso de sello/revocación.
No hay una firma PDF verificada. Ese aviso no demuestra un fallo de Internet:
falta primero el valor de firma. No hay stack trace ni APDU autenticadas para
identificar la función exacta. La revisión estática confirma SunPKCS11, cuyo
código OpenJDK 21 prefiere CKM_SHA256_RSA_PKCS sobre CKM_RSA_PKCS. La regresión
reproduce la ruta QSign fallida y la corrige con DigestInfo + RSA-PKCS mediante
el ajuste de capacidades. El intento posterior de las 20:37 con
`make sign/jcop3-sha256` produjo una firma verificada del texto original.
La llave pública extraída nuevamente del certificado y el DigestInfo
recalculado coinciden con los archivos guardados; una verificación OpenSSL
independiente también devolvió `Verified OK`. El mecanismo combinado queda
validado en esta prueba técnica. La validación del PDF sigue pendiente.
El usuario volvió al firmador y recibió `CKR_USER_NOT_LOGGED_IN` seguido
de los errores de firma nula. Pidió el PIN; el certificado seleccionado
no se confirmó inicialmente. El registro de las 21:06 ya confirma un
`C_Login CKR_OK`, llave 3, mecanismo `0x40` y fallo en `C_SignFinal`. Las
cuatro consultas de estado posteriores al login informan autenticación
(`PIN_STATUS a=2`, sesión RW USER `a=3`). No hay logout/cierre intermedio.
Los tres mensajes del usuario no corresponden a tres firmas en este
registro: solo hay un login y un `C_SignFinal`. Un logout previo al login
devuelve `0x101`; no confundirlo con el error posterior de firma.
El registro ampliado de las 21:24 confirma que selección, DOCP y MSE
pasan. RSA AT devuelve `6982`, y la revalidación de PIN queda rechazada
por caché desactivada. No hay otra operación RSA en ese registro.
Falta explicar qué condición de seguridad perdió o no satisfizo la tarjeta.
Las consultas posteriores de `C_GetSessionInfo` seleccionan MF y leen
política; eso es una diferencia concreta con la prueba CLI verificada.
El experimento de las 21:39 omitió esas consultas y el mismo rechazo RSA
persistió: la hipótesis no se confirma. No descarta todas las consultas de
PIN: el registro de las 21:54 confirma que `C_GetTokenInfo` llama a
`sc_pkcs15_get_pin_info` inmediatamente después del login y vuelve a
seleccionar la raíz IAS. Después hay cuatro consultas de sesión similares.
Las cinco informan PIN verificado. No se observaron selecciones de archivo
por enumeración de objetos tras login ni reinicios detectados al bloquear.
La firma selecciona ChipDoc, prepara MSE y recibe `6982`. El experimento
se amplió para omitir ambas clases de consulta en el perfil autenticado.
`TOKEN_PIN_CACHED` marca la omisión; las banderas de intentos del token usan
metadatos públicos anteriores y pueden estar desactualizadas. No afirma una
nueva lectura física del PIN, ni guarda su valor. El registro de las 23:34
confirma `TOKEN_PIN_CACHED` y tres `SESSION_LOGIN_STATE`, ninguna selección
raíz después del login, y RSA AT `9000` con retorno de 256 bytes y
`C_SignFinal CKR_OK`. No hay revalidación ni segunda operación RSA. Esto
sustenta revisar consultas/contexto como origen del fallo, pero no distingue
el efecto de SELECT, VERIFY vacío y GET DATA individualmente. Falta diseñar y validar
una corrección definitiva; el experimento sigue optativo. Los PDF se
verificaron después: la firma CAdES coincide con el certificado de la
tarjeta, y el CMS del sello RFC 3161 y su huella sobre el PDF final pasan.
Informe: `local/jcop3-pdf-verification-ydbv_z5o/report.json`; no se validaron
confianza ni revocación de los certificados.
No hay evidencia de PIN incorrecto, llave equivocada o problema de Internet.

Las consultas vacías de VERIFY devuelven `63 C7`: PIN no verificado, 7 intentos
restantes. No llevan el valor del PIN. El primer intento de firma no tuvo
depuración APDU; no se puede asegurar a partir del registro si el PIN llegó
a transmitirse. No hay evidencia de PIN incorrecto. La firma exitosa posterior
consta en `jcop3-signature-crUXrnAi/report.txt`.
Un `exit_status=0` en una herramienta no garantiza que no haya errores
internos de parsing: revisar las respuestas y el contexto.

## Próximo trabajo, en orden

1. **Las dos firmas consecutivas ya pasaron y se verificaron.** No pedir
   recompilar ni repetirlas sin un motivo nuevo. Captura
   `local/jcop3-firmador-api-fgRfkdCF/`, resumen `analysis.json`:
   dos CARD_PIN_VERIFY exitosos, dos C_Login CKR_OK, logout software
   intermedio y dos RSA `9000` con 256 bytes. Cada login tiene cuatro consultas
   físicas antes de firmar, sin SELECT raíz en ese intervalo. Hay además dos
   consultas entre primera firma y nuevo login, incluso tras logout software.
   El estado CHV físico sigue autenticado; el segundo login ejecuta VERIFY real
   después de seleccionar raíz y leer política. No hay atajo por hash,
   omisión de consultas ni reset detectado.
2. Los PDF `local/jcop3-fresh-verify-1.pdf` y `local/jcop3-fresh-verify-2.pdf`
   se verificaron sin lector, PIN ni red. Informes privados:
   `local/jcop3-pdf-verification-w2t9vw2q/report.json` y
   `local/jcop3-pdf-verification-_5f871pd/report.json`, con copias signed.pdf.
   Ambos tienen 80485 bytes; CAdES cubre revisión de 38403 bytes y sello
   RFC 3161 se vincula al archivo final salvo su propio Contents.
   Certificado idéntico a LlaveDeFirma, CMS e imprint SHA-512 válidos;
   original, texto y página sin anotaciones conservados. No se validaron
   confianza ni revocación. Para versiones previas usar las copias y hashes
   de cada informe, no nombres o tamaños. No publicar esos archivos.
3. **Reinserción con espera pasó; la rápida falló y ya tiene corrección local.**
   Captura exitosa `oJtaNn1s`, 430 eventos, ausencia y limpieza observadas,
   nuevo VERIFY y segunda firma. Sus PDF before/after de 80485/80465 bytes
   están verificados: informes `local/jcop3-pdf-verification-z_xushy2/` y
   `local/jcop3-pdf-verification-sblid4m2/`. No repetir ese control sin motivo.
   Captura rápida `y17eLwTD`, 260 eventos: primera C_SignFinal en 208; después
   diez CARD_PRESENCE=3 y ninguna SLOT_TOKEN_REMOVED. Cuatro locks -1105,
   tres C_GetTokenInfo OK pese al fallo físico, segundo login 0x32 en 247,
   nueva sesión OK y C_GetSessionInfo 0xB3 en 260. Solo hubo un VERIFY y una
   firma. No hay PDF after; el before de 80485 bytes está verificado, informe
   `local/jcop3-pdf-verification-bjaldk_f/report.json`. Resumen privado en
   `local/jcop3-firmador-api-y17eLwTD/analysis.json`.
   En slot.c, no_change retornaba OK antes de CARD_CHANGED. Ahora ese atajo
   excluye cambios detectados; al limpiar se elimina no_change para que la
   siguiente lectura pase por conexión y binding. retry es local a la llamada,
   evitando gastar un presupuesto estático entre reinserciones independientes.
   Es una corrección general de la capa PKCS#11, no una condición exclusiva
   Amos; no modifica VERIFY, APDU RSA ni el opt-in de firma. Reconectar vuelve
   a inicializar el perfil y exige autenticación nueva. Java no se modificó;
   su NPE sigue explicada por el código local, sin stack trace completo.
   **Siguiente intervención GUI con corrección:** compilar mediante
   `make test/iasecc JOBS="$(nproc)"`. Guardar commit de slot.c en submódulo y
   luego principal con tests/documentación y puntero. Cerrar otras instancias,
   abrir `make firmador/jcop3-pin-status-in-place` y firmar el original doc.pdf
   hacia `local/jcop3-hotplug-fixed-before.pdf`. Mantener lector conectado y
   GUI abierta; retirar solo tarjeta y reinsertar después de aproximadamente
   un segundo. Esperar cinco segundos tras insertar antes de abrir el original
   y firmar hacia `local/jcop3-hotplug-fixed-after.pdf`. Observar retirada y
   nueva tarjeta. Detenerse al primer error, sin reintentar; cerrar al terminar.
   Revisar limpieza, sesiones, VERIFY y PDF antes de afirmar recuperación.
4. La candidata sigue **optativa** mediante
   `OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes`. PKCS#15 usa estado físico sin SELECT,
   limitado a Amos + Generic PKI + CHV global 1 + MF FILE_ID, excluyendo SO/PUK.
   Driver STATUS_ONLY requiere política pública conocida y solo conserva su
   máximo público de intentos, nunca estado ni contador restante. Si aún no
   se conoce, NOT_SUPPORTED permite inicialización completa. Fallos invalidan
   observaciones físicas; no afirmar que resuelven sesiones software o cambios
   externos. La ampliación desactiva consulta y almacenamiento del atajo SHA-1
   para VERIFY explícito Amos + Generic PKI completo + CHV global 1. Conserva
   estado previo, política/ACL/límites y un VERIFY real con la entrada del
   usuario, sin reintentos. No modifica logout físico. Fuera del opt-in se
   conserva el camino anterior. Pasan 256 regresiones. No activar caché PIN.
5. CARD_PIN_VERIFY y CARD_PIN_VERIFY_CACHED indican resultado del VERIFY
   mediante el driver ISO o retorno anticipado por hash. Llevan retorno y
   referencia pública, sin valor, longitud ni hash del PIN. El informe
   `pin_verify_experiment=fresh_verify` no basta sin eventos del módulo.
   La captura nueva confirma un VERIFY por login y ninguna marca CACHED.
   Firmador pidió PIN dos veces porque hace logout y login entre documentos,
   no porque la candidata genere un diálogo adicional o reintente un error.
   No inferir una política de una firma por PIN de la tarjeta. El experimento
   de omisión sigue disponible pero no convertirlo en predeterminado. SELECT
   raíz aislado ya reprodujo `6982`; C9 sigue sin significado confirmado.
   No activar atomic/lock_login, modificar PIN/llaves/ACL, forzar reset/logout
   físico ni capturar APDU autenticadas como arreglo especulativo.
6. **Modo CLI adicional preparado, no sustituto del caso GUI.** El usuario
   informó -dargs y autorizó PIN desde pass por stdin. Revisión estática de
   GUISelector/GUIArgs confirma entrada desde pipe, sin argumentos del PIN,
   y limpieza de su char[]. `make firmador/jcop3-cli` activa misma candidata,
   configuración privada y metadatos; PDF_INPUT=doc.pdf y
   PDF_OUTPUT=local/jcop3-cli.pdf por defecto. El wrapper no ejecuta pass ni
   lee/copia/guarda PIN: exec conserva stdin hacia firmador. Rechaza entrada
   ausente, salida existente y directorio inexistente. Usar explícitamente:

   ```bash
   set -o pipefail
   pass personal/firma-digital/pin | make firmador/jcop3-cli
   ```

   El agente aún no ejecutó una firma CLI y sigue sin lectores. Nueve pruebas
   nuevas del lanzador usan firmador/git falsos y stdin sintético, no pass ni
   Java reales; no son repositorios alternativos ni commits. Verifican rutas
   con espacios, stdin, salida 17, privacidad, rechazos y cuatro modos GUI.
   El helper técnico sign-jcop3.sh conserva su requisito TTY.
7. Después de delimitar reinserción rápida, validar sesiones, pérdida de autenticación,
   concurrencia y contexto cambiado por otro cliente. Preparar intervenciones
   concretas, sin reintentos automáticos de PIN. Decidir activación por defecto
   con evidencia. Antes de upstream, preservar rama y reproducir contra master
   actual según vendor/opensc/AGENTS.md. Si cambia código, dar comandos de commit
   al usuario: submódulo primero, principal después con su puntero. Ampliar
   mecanismos y plataformas antes de afirmar soporte completo. No generar llaves.

**Firmas consecutivas y reinserción con espera pasaron. La reinserción rápida
falló; se reprodujo con lector simulado y se corrigió localmente card_detect.
La siguiente prueba GUI validará esa corrección en hardware. CLI, concurrencia
y activación por defecto siguen pendientes.**

## PIN, privacidad e Idopte

- El PIN lo introduce el usuario sin eco en terminal/GUI, o lo entrega a
  firmador CLI por stdin desde pass, según su autorización explícita.
  Nunca ponerlo en argumentos, variables de entorno, archivos nuevos, chat,
  informes o capturas. El almacén existente de pass queda fuera del proyecto;
  el wrapper no lee ni guarda el flujo. No automatizar reintentos de PIN.
- `sign-jcop3.sh` exige stdin y stdout TTY porque `util_getpass` usa stdout
  para desactivar el eco. No redirigir ni pasar por `tee` la salida del comando
  autenticado. El helper fija `OPENSC_DEBUG=0` y `debug = 0`.
- El helper desactiva `use_pin_caching`: impide la revalidación automática
  de PKCS#15 después de un fallo de firma. No volver a activarla durante
  la investigación de compatibilidad.
- No habilitar capturas APDU durante login/firma: podrían contener el PIN.
  No se ha capturado una sesión autenticada de Idopte.
- La prueba firma el texto técnico visible en `message.txt`, no un PDF.
  Calcula SHA-256 en el equipo y prepara `digest-info.der` antes del login;
  la tarjeta recibe ese DigestInfo para RSA PKCS#1 v1.5, no el texto completo.
  Verifica `signature.bin` con la llave pública del certificado mediante
  OpenSSL. No intenta exportar la llave privada de la tarjeta.
- Firmar con la tarjeta no requiere Internet. Descargar dependencias Nix sí
  puede requerirlo; firmador puede usar red para otros servicios como sellado
  de tiempo o comprobación de revocación.
- `doc.pdf` y `doc-firmado.pdf` son archivos de prueba del usuario,
  excluidos de Git. No incorporarlos a commits.
- `local/` contiene certificados y otros datos identificables. Mantenerlo
  fuera de Git y evitar volcar capturas completas al chat. Los scripts usan
  `umask 077`; no publicar los informes sin revisar y anonimizar.
- No inicializar, borrar, generar llaves, cambiar PIN ni desbloquear la tarjeta.

El módulo privativo que funciona con el firmador del sistema es
`/run/current-system/sw/lib/SCMiddleware/libidop11.so`, repaquetado para Nix
(Idopte 6.23.50.5-1). `LIBASEP11` conserva el nombre histórico, pero puede
seleccionar ese módulo. El archivo del instalador en `vendor/idopte/` tiene
dependencias/rutas que no funcionan igual en Nix; ver `docs/idopte.md`.
La revisión estática de `libt_ias.so` no confirmó el significado de `C9`.
No copiar código privativo al driver libre ni incorporar binarios a Git.

## Instrucciones de OpenSC

Leer [vendor/opensc/AGENTS.md](vendor/opensc/AGENTS.md) al trabajar allí.
Antes de preparar un reporte upstream requiere reproducir contra `master`
actual y proponer una solución concreta. Esa actualización debe preservar
la rama local y su evidencia. Los mensajes de commit deben ser concisos.
Si se sospecha una vulnerabilidad, seguir `SECURITY.md` y su procedimiento
privado; no publicar un reporte de seguridad. No se ha iniciado tal reporte.

Actualizar este documento cuando cambie el hito, el bloqueo o la evidencia
principal, conservando las capturas originales en `local/`.
