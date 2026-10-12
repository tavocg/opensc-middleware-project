# Investigación: JCOP3 de Costa Rica

## Inventario

| Dato | Valor / evidencia |
| --- | --- |
| Generación indicada por el propietario | JCOP3; pendiente de corroborar técnicamente |
| Interfaz inicial | Contacto |
| Modelo de lector | HID Global OMNIKEY 3x21 Smart Card Reader; índice PC/SC 0 |
| Sistema y arquitectura | Linux 6.18.55 x86_64 |
| Versión de OpenSC empaquetado | 0.27.1; corroborado con `opensc-tool --info` y la ruta del ejecutable |
| Commit base del submódulo OpenSC | `c7a377af971df03f879c1697e91ebac1f7e28c56`; sin cambios en las capturas originales |
| Versión y ruta del módulo Idopte | Biblioteca 6.23; paquete instalado `6.23.50.5-1`; `/run/current-system/sw/lib/SCMiddleware/libidop11.so`; véase `idopte.md` |
| ATR por contacto | `3b:df:18:ff:81:91:fe:1f:c3:00:31:b8:64:0c:01:ec:c1:73:94:01:80:82:90:00:b3` |
| Entrada coincidente en OpenSC | `IAS/ECC v1.0.1 Amos`, tipo `SC_CARD_TYPE_IASECC_AMOS`, driver `iasecc` |
| AID raíz IAS/ECC | `F0496173456363526F6F74` (`F0` seguido de `IasEccRoot`), leído de EF.ATR y seleccionado con éxito |
| AID de aplicación anunciado en EF.DIR | `E828BD080FD25047656E65726963`, etiqueta `ChipDoc`; OpenSC lo identifica como `ECC Generic PKI` |
| Versión de aplicación | Pendiente |
| Tipo y tamaño de llave del certificado | RSA de 2048 bits en ambos certificados, comprobado con OpenSSL sobre los DER leídos |
| Mecanismo de firma usado por el firmador | SHA256-RSA-PKCS (0x40), llave 3 RSA 2048; GUI y CLI producen PDF verificados |

Un ATR o AID coincidente es una pista, no una demostración de compatibilidad.
Las capacidades del chip tampoco demuestran qué permiten las llaves emitidas.

## Primera observación: 2026-10-11

El informe `local/jcop3-inventory-mSphakPR.txt`, generado a las 20:06:50 UTC,
muestra el lector en el índice 0 con tarjeta presente y la lectura exitosa del
ATR. El ATR completo coincide con la entrada Amos en
`vendor/opensc/src/libopensc/card-iasecc.c`, línea 93 del commit registrado.
No coincide con los ATR de `card-eoi.c` de ese commit.

La coincidencia estática no demuestra que el mapa PKCS#15 o la firma sean
compatibles. La inspección posterior sí confirmó la inicialización IAS/ECC;
el fallo de enumeración y la lectura directa se describen más abajo.

En esta captura `opensc-tool --version` devuelve solo la información de revisión
Git; el script ahora usa `--info` para registrar la versión y funciones habilitadas.

## Hitos

- [x] PC/SC enumera el lector y detecta la tarjeta.
- [x] ATR registrado mediante el script de observación.
- [x] ATR comparado con las tablas de IAS/ECC y eOI.
- [x] Driver IAS/ECC inicializa la tarjeta y selecciona su AID raíz.
- [x] EF.DIR leído por APDU y aplicación `ChipDoc` identificada.
- [x] Fallo del parser de ACL reproducido y corregido en pruebas locales.
- [x] Corrección validada con la tarjeta y una compilación con PC/SC.
- [ ] AID y APDU comparados con IAS/ECC y eOI.
- [ ] Enumeración pública de Idopte capturada y documentada.
- [x] Certificado y referencia de llave encontrados.
- [x] Ambos certificados leídos sin PIN; algoritmo y tamaño corroborados.
- [x] PKCS#11 publica certificados, llaves públicas y mecanismos de firma.
- [ ] OpenSC enumera los objetos correctamente.
- [x] Una firma con el mecanismo observado se verifica externamente.
- [x] Firma reproducida sin cargar Idopte, en GUI y CLI.
- [x] Retirada/reinserción rápida en la misma GUI, con PDF verificados.
- [x] Cuatro ciclos reales de reinserción con seis firmas verificadas.
- [ ] Errores adicionales y clientes concurrentes comprobados.

## Registro de comandos observados

No incluir PIN, datos personales ni certificados identificables en este documento.
Guardar los originales en `local/` y publicar únicamente ejemplos revisados.

| Operación | Estado previo | APDU documentada | Respuesta / SW | Evidencia |
| --- | --- | --- | --- | --- |
| Seleccionar raíz IAS/ECC | Tarjeta conectada por contacto | `00 A4 04 04 0B F0496173456363526F6F74 00` | `9000` | `jcop3-dir-97OiAqpB.txt` |
| Seleccionar EF.DIR | Raíz IAS/ECC seleccionada | `00 A4 09 04 02 2F00 00` | FCP, `9000` | Mismo informe |
| Leer EF.DIR | EF.DIR seleccionado | `00 B0 00 00 80`, `00 B0 00 80 80` | 256 bytes, ambos `9000` | Mismo informe |
| Seleccionar aplicación ChipDoc | Pendiente | Pendiente | Pendiente | Pendiente |
| Leer certificado | Pendiente | Pendiente | Pendiente | Pendiente |
| Autenticar | Pendiente | Documentar formato; ocultar PIN | Pendiente | Pendiente |
| Seleccionar llave y algoritmo | Pendiente | Pendiente | Pendiente | Pendiente |
| Firmar | Pendiente | Pendiente | Pendiente | Pendiente |

Para cada captura registrar lector, interfaz, middleware, mecanismo y parámetros,
estado de caché y si se retiró la tarjeta antes de la operación. Distinguir hechos
observados de hipótesis sobre el protocolo.

## Inspección: 2026-10-11

El informe `local/jcop3-inspection-EkcrTRgz.txt` muestra que `opensc-tool --name`
termina con éxito y devuelve `IAS-ECC`. La selección del AID raíz
`F0496173456363526F6F74` y de EF.DIR (`2F00`) también devuelve `9000`.

El primer fallo al enumerar aplicaciones ocurre en el middleware, antes de
transmitir READ BINARY para EF.DIR:

```text
_iasecc_sm_read_binary: iasecc_sm_read() 'READ' ACL not present
sc_enum_apps: sc_read_binary() failed: -1407 (Requested object not found)
```

El FCP de EF.DIR declara un archivo transparente de 256 bytes y contiene:

```text
A1 0C 8C 04 40 00 01 00 9C 04 40 00 01 00
```

En el código original, `iasecc_process_fci()` interpreta únicamente el primer
byte de modos de acceso (`40`) y su condición (`00`); no procesa el grupo
siguiente (`01 00`). El formato compacto admite varios grupos de modo y
condiciones. La prueba local con el FCP capturado confirmó que la regla de
lectura del segundo grupo no se incorporaba al modelo de ACL de OpenSC.

Referencia para el formato compacto: [ETSI TS 102 221, sección 9.2.5](https://www.etsi.org/deliver/etsi_ts/102200_102299/102221/18.01.00_60/ts_102221v180100p.pdf).
La lectura directa confirmó que la tarjeta permite leer este archivo sin PIN.

Al no poder leer EF.DIR, OpenSC intenta las rutas predeterminadas `5015` y
`5031`, recibe `6A82`, y termina con `PKCS#15 binding failed: Unsupported card`.
Esto no prueba que falten certificados ni que el mapa sea propietario.
La ausencia de `/etc/opensc.conf` tampoco impide la inicialización observada.

## Lectura directa de EF.DIR: 2026-10-11

El informe `local/jcop3-dir-97OiAqpB.txt` contiene las cuatro respuestas `9000`
de `make read-dir/jcop3`. Los 256 bytes incluyen un registro de aplicación de
64 bytes y relleno cero. El AID es `E828BD080FD25047656E65726963` y la etiqueta
es `ChipDoc`, rellenada con espacios. Es distinto del AID raíz IAS/ECC.

La tabla de aplicaciones de `src/libopensc/dir.c` ya contiene ese AID como
`ECC Generic PKI`. Una prueba con el parser real `parse_dir_record()` acepta
el registro observado y construye una ruta por nombre de aplicación
(`SC_PATH_TYPE_DF_NAME`). Esto permite intentar el enlace PKCS#15 dentro de
la aplicación correcta; no demuestra todavía que los certificados o las
llaves sean accesibles ni cuál sea su algoritmo.

## Corrección y pruebas locales

La modificación de `src/libopensc/card-iasecc.c` recorre todos los grupos
compactos de modo/condición. Mantiene las condiciones de autenticación y
denegación. Acepta reglas repetidas idénticas y devuelve
`SC_ERROR_NOT_SUPPORTED` para condiciones alternativas distintas de una misma
operación, cuya semántica OR todavía no se implementa. Los grupos incompletos
devuelven `SC_ERROR_INVALID_DATA`.

`tests/iasecc-fci.c` ejecuta el parser real, con el FCP y registro de aplicación
capturados. Las once comprobaciones cubren grupos únicos y múltiples,
autenticación, denegación, reglas repetidas, alternativas no soportadas,
truncamiento y el registro `ChipDoc`. Antes de la corrección fallaron siete
comprobaciones de ACL; después pasan las once comprobaciones, incluyendo el
registro de aplicación.

La compilación completa del submódulo y las pruebas se ejecutaron localmente
en `build/opensc-offline/` con PC/SC desactivado porque esta sesión no tiene
los encabezados de desarrollo de PC/SC ni acceso al lector. Los registros
de compilación y regresión quedan en ese directorio, excluido de Git.
Esta validación no sustituye la prueba en tarjeta.

La siguiente comprobación en la terminal del propietario es:

```bash
nix develop
make test/iasecc
make inspect/jcop3-built
```

La compilación normal sí habilita PC/SC y fija la ruta del proveedor obtenida
con `pkg-config`. La inspección utiliza esa compilación, desactiva la caché
de archivos y no verifica PIN. Si aparece otro fallo, investigar el primer
APDU o estructura incompatible del nuevo informe.

Antes de preparar un reporte para OpenSC, actualizar desde `master` y repetir
la compilación y verificación, según `vendor/opensc/AGENTS.md`. Todavía no se
ha enviado un reporte ni una propuesta upstream.

## Validación con tarjeta: 2026-10-11

`local/jcop3-inspection-WAZLkGL7.txt`, generado a las 20:44:32 UTC, corresponde
al commit `6226bd25614025a52e71f0146687beffed73e4ac`, con el submódulo limpio.
La compilación anuncia PC/SC, OpenSSL y zlib. Todas las consultas terminaron
con código cero. EF.DIR se leyó con IAS/ECC y se seleccionó `ChipDoc` para
enlazar su estructura PKCS#15.

OpenSC enumeró y leyó dos certificados:

| Etiqueta | Archivo dentro de ChipDoc | Llave pública | Uso declarado en X.509 |
| --- | --- | --- | --- |
| `LlaveDeFirma` | `001C` | RSA, 2048 bits | Digital Signature, Non Repudiation |
| `LlaveDeAutenticacion` | `001D` | RSA, 2048 bits | Digital Signature, Key Encipherment |

El algoritmo y tamaño se corroboraron reconstruyendo los DER de las respuestas
READ BINARY del informe y analizándolos con OpenSSL. Los datos identificables
del certificado permanecen en `local/`. El ID PKCS#15 de cada certificado es
su etiqueta codificada en bytes; la referencia de la llave privada aún no se
ha consultado. El uso declarado no demuestra que una firma ya funcione.

El informe también contiene un error al decodificar el directorio de llaves
públicas `7004`: `SC_ERROR_ASN1_OBJECT_NOT_FOUND` (`-1402`). No se imprimieron
objetos de llave pública independientes. Este error no se reflejó en el código
de salida final. Aún hay que determinar si afecta a la construcción de objetos
PKCS#11; el certificado sí contiene una llave pública RSA válida.

La siguiente consulta, sin PIN ni firma y aprovechando la compilación existente:

```bash
make inspect/jcop3-pkcs11
```

Registra metadatos de llaves privadas y PIN mediante PKCS#15, y luego información
de biblioteca, slots, mecanismos y objetos mediante el módulo compilado
`opensc-pkcs11.so`. La caché de archivos se desactiva con una configuración local
por informe. `READER` selecciona el lector de PKCS#15 y `SLOT_INDEX` selecciona
el slot de PKCS#11; son índices distintos y ambos valen cero por defecto.
El informe y su configuración se guardan en `local/`.

## Enumeración PKCS#11: 2026-10-11

`local/jcop3-pkcs11-QWvNuNAk.txt`, generado a las 20:48:25 UTC, confirma que
el módulo compilado inicializa correctamente, anuncia Cryptoki 3.0 y publica
el slot 0 con un token que requiere login. Todas las consultas devuelven cero,
con caché de archivos desactivada.

PKCS#15 describe ambas llaves privadas RSA de 2048 bits:

| Etiqueta | Referencia | Archivo / referencia dentro de ChipDoc | Uso |
| --- | --- | --- | --- |
| `LlaveDeAutenticacion` | `2` | `0002` | decrypt, sign, signRecover, unwrap |
| `LlaveDeFirma` | `3` | `0003` | sign, signRecover |

Ambas se asocian al objeto `Card PIN`, con referencia 1 e ID
`436172642050494e`. Son llaves nativas, sensibles y no extraíbles según los
metadatos. No se intentó exportar ninguna llave privada ni verificar el PIN.

PKCS#11 enumera los dos certificados y dos objetos de llave pública RSA con
los IDs correspondientes. Las llaves privadas no aparecen en la consulta sin
login. El error de decodificación de `7004` persiste, pero no impide inicializar
el módulo ni construir las llaves públicas a partir de los certificados.

Se anuncian `RSA-PKCS`, `SHA1-RSA-PKCS` y `SHA256-RSA-PKCS`, entre otros. Los
mecanismos combinados con hash no llevan la marca `hw`, mientras `RSA-PKCS`
sí la lleva: el anuncio no demuestra que el hash se ejecute en la tarjeta.
Tampoco se han probado las operaciones de descifrado o generación anunciadas.

La siguiente prueba utiliza únicamente `LlaveDeFirma`, identificada por
`4c6c61766544654669726d61`, con `SHA256-RSA-PKCS`:

```bash
make sign/jcop3
```

Requiere una terminal interactiva. Crea un texto técnico con fecha UTC, lee el
certificado asociado, pide el PIN mediante `pkcs11-tool` y solicita una firma.
No reintenta un error de PIN. Verifica la firma fuera del módulo con
`openssl dgst -sha256 -verify` y guarda texto, certificado, llave pública,
firma, configuración e informe en un nuevo directorio `local/jcop3-signature-*`.
La depuración OpenSC/APDU se desactiva antes de autenticar; el PIN no se pasa
como argumento ni se registra. La salida estándar permanece conectada a la
terminal porque el lector de PIN de la herramienta la necesita para desactivar
el eco. El primer intento en tarjeta falló durante el login, como se describe
a continuación.

## Primer intento de firma: 2026-10-11

`local/jcop3-signature-S8Pd9rPG/report.txt`, generado a las 20:58:01 UTC,
confirma que se leyó el certificado y se extrajo su llave pública. La operación
autenticada terminó con `C_Login failed: CKR_GENERAL_ERROR (0x5)` y código 1.
No se generó `signature.bin` ni se llegó a la verificación externa. La
depuración APDU permaneció desactivada durante este intento.

El código genérico no demuestra un PIN incorrecto. La captura anterior, sin
autenticación, ya mostraba un fallo al consultar su estado: el AODF declara
`path=0001`, y `sc_pkcs15_get_pin_info()` intenta seleccionarlo como EF con
`00 A4 02 04 02 00 01 00`; recibe `6A82` y devuelve `SC_ERROR_FILE_NOT_FOUND`.
Este resultado no se reflejó en el estado final de la enumeración.

`sc_pkcs15_verify_pin_with_session_pin()` también selecciona esa ruta antes de
llamar a `sc_pin_cmd()`. El mapeo PKCS#11 de `SC_ERROR_FILE_NOT_FOUND` produce
`CKR_GENERAL_ERROR`. Por ello, la selección del contexto es la primera causa
que debe comprobarse. El informe de firma sin depuración no permite confirmar
su secuencia APDU ni establecer si se transmitió un VERIFY con el PIN.

Para investigar sin otra autenticación:

```bash
make inspect/jcop3-pin-context
```

Usa el driver `default` y compara la selección de `0001` como EF, DF o archivo
de cualquier tipo, empezando cada vez desde el AID raíz IAS/ECC y desde
`ChipDoc`. Consulta también los metadatos DOCP del objeto CHV de referencia 1
en ambos contextos con el formato GET DATA del driver IAS/ECC. No transmite
VERIFY ni solicita el valor del PIN; solo SELECT y GET DATA de metadatos.
No modifica los contadores de intentos mediante autenticación.

Guarda el informe en `local/jcop3-pin-context-*`. Los SW negativos forman parte
de la comparación y no se consideran por sí solos un fallo del diagnóstico.
La corrección del contexto y un nuevo intento autenticado quedan pendientes
de revisar esas respuestas.

## Contexto global del PIN: 2026-10-11

`local/jcop3-pin-context-6Hd0jGpP.txt`, generado a las 21:04:50 UTC, contiene
las 16 respuestas esperadas. La selección de `0001` como EF o DF devuelve
`6A82` tanto desde la raíz como desde `ChipDoc`; la selección sin especificar
tipo devuelve `6A86` en ambos contextos.

GET DATA del DOCP del CHV 1 devuelve `9000` desde la raíz IAS/ECC y `6A88`
desde `ChipDoc`. La respuesta de la raíz declara máximo 7 intentos (`9A 01 07`),
7 restantes (`9B 01 07`) y tamaño almacenado 14 (`80 02 00 0E`). Este último
campo no permite deducir la longitud que debe escribir el usuario. No se
consultó ni transmitió el valor del PIN.

La evidencia sitúa el CHV global en MF. El precedente de `pkcs15-eoi.c` y
`card-eoi.c` también distingue rutas declaradas de PIN de archivos seleccionables.
Nuestra corrección se aplica a los metadatos PKCS#15: para la combinación
IAS/ECC Amos, AID Generic PKI, PIN global de referencia 1 y ruta `0001`, cambia
el contexto a MF (`3F00`). Conserva referencias, condiciones de autenticación
y rutas de los otros objetos; no modifica archivos de la tarjeta.

El ajuste reside en `pkcs15-iasecc.c` y se aplica al cargar el AODF mediante
su callback de emulación. Las configuraciones locales de enumeración y firma
activan `try_emulation_first = yes`, porque el enlace nativo por defecto
termina antes de ejecutar ese callback. Es una configuración experimental del
proyecto; todavía no se activa automáticamente para ese perfil en OpenSC.

`tests/iasecc-pin-path.c` comprueba el ajuste con los metadatos observados y
que otros perfiles, AID, rutas, referencias, PIN locales y objetos AuthKey
conservan su contexto. Pasan estas nueve comprobaciones y las once anteriores.
La compilación completa en `build/opensc-offline/`, sin PC/SC, también terminó
correctamente. Falta validar este ajuste en tarjeta.

Además, el DOCP contiene `C9 01 01`, campo no reconocido por el parser actual.
Una reproducción local de sus 45 bytes con `iasecc_sdo_parse()` devuelve
`SC_ERROR_UNKNOWN_DATA_RECEIVED` (`-1213`). El significado de `C9` queda por
investigar; la corrección de ruta no modifica ese parser ni omite ese campo.
Las ACL de ese objeto también contienen grupos adicionales que habrá que
interpretar antes de considerar completa la política del PIN.

La siguiente comprobación usa la corrección de ruta para observar hasta dónde
llega el driver al consultar el estado:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
```

No pide el PIN ni realiza login o firma. A diferencia del diagnóstico de APDU
directas, la consulta de estado de OpenSC puede enviar VERIFY sin datos:
es la consulta ISO de estado, sin proporcionar un PIN para autenticarse.
Revisar el informe para confirmar que se usa MF, se leen los metadatos y se
localiza el siguiente fallo. Un código de salida cero no demuestra que el
estado del PIN se haya consultado correctamente. La autenticación sigue
pendiente de interpretar la política completa.

## Validación del contexto y parser SDO: 2026-10-11

`local/jcop3-pkcs11-sqj7pvyv.txt`, generado con el commit OpenSC
`ea864361d` y el repositorio principal `299bfa3`, confirma que se aplica el
ajuste de ruta. `Card PIN` aparece con contexto `3f00`. La selección de la
raíz funciona y la consulta de estado `00 20 00 01`, sin datos de PIN,
devuelve `63 C7`: PIN no verificado, con 7 intentos restantes. No se realizó
login ni firma.

GET DATA de los atributos públicos CHV de referencia 1 también funciona.
La respuesta, sin SW final, es:

```text
70 0D BF 81 01 09 7F 41 06 80 01 0E 81 01 04
```

Declara longitud máxima 14 y mínima 4. Son atributos de la tarjeta y difieren
de la longitud máxima 20 del AODF. No contienen el valor del PIN.

El nuevo fallo aparece inmediatamente después: el siguiente GET DATA solicita
DOCP de clase 0 y referencia 0 (`BF 80 00`), en lugar de CHV 1 (`BF 81 01`),
y recibe `6A88`. La causa está en `iasecc_sdo_parse()`:
`iasecc_sdo_parse_data()` devuelve un número positivo de bytes consumidos al
tener éxito, pero el llamador compara ese resultado con cero y libera todos
los campos. `iasecc_sdo_free_fields()` también pone a cero clase y referencia.
El parser termina anunciando éxito después de haber descartado los datos.

La corrección cambia esa comprobación a `rv < 0`, conservando el objeto para
resultados válidos y manteniendo la liberación y propagación de errores para
resultados negativos. `tests/iasecc-sdo.c` reproduce la pérdida con la respuesta
CHV capturada y prueba que se acumulan sus límites con una segunda respuesta
DOCP estándar. El DOCP de esa prueba es sintético; no sustituye al capturado,
que todavía contiene el campo desconocido `C9` y ACL adicionales.

Antes del cambio fallaron las cuatro comprobaciones de conservación de datos;
la comprobación de rechazo de un campo CHV desconocido ya pasaba. Después
pasan las cinco y las veinte comprobaciones anteriores. La compilación completa
en `build/opensc-offline/`, sin PC/SC, terminó correctamente; el registro queda
en `sdo-metadata-build.log` dentro de ese directorio ignorado.

La consulta en tarjeta debe comprobar ahora que el segundo GET DATA conserva
clase CHV y referencia 1 y recibe el DOCP previamente observado:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
```

Todavía no se solicita una nueva firma. El campo `C9` sigue sin definición
confirmada: las búsquedas en documentación pública y la revisión estática
inicial del módulo `libt_ias.so` no permitieron identificar su significado.
No se modificó el comportamiento del parser ante ese campo ni se capturó
ninguna sesión autenticada de Idopte.

## Política DOCP y selección de campos: 2026-10-11

`local/jcop3-pkcs11-gYBEWLz8.txt`, generado con OpenSC `512084237` y el
repositorio principal `0ebb25d`, valida la conservación del objeto SDO. Tras
leer los límites públicos, el driver mantiene clase CHV y referencia 1. El
GET DATA siguiente contiene `BF 81 01` y recibe `9000` con el DOCP de 45 bytes
ya observado. El primer rechazo del parser es ahora `C9 01 01`, con código
`-1213`. Las consultas de estado siguen recibiendo `63 C7`.

Falta confirmar el significado de esa extensión. Antes de modificar su
interpretación, se añadió una comparación de selección de campos:

```bash
make inspect/jcop3-pin-policy
```

No recompila. Usa el driver `default`, selecciona la raíz y envía exclusivamente
GET DATA de metadatos del CHV 1. Consulta el DOCP completo al principio y al
final como control, y compara dos formas de seleccionar cada campo conocido:
directamente bajo el encabezado del SDO y anidado bajo `A0`.

| Campo | Tag | Propósito |
| --- | --- | --- |
| Máximo de intentos | `9A` | Límite de autenticaciones fallidas |
| Intentos restantes | `9B` | Estado del contador |
| Tamaño almacenado | `80` | Metadato de longitud del objeto |
| Permisos | `A1` | Condiciones de acceso por interfaz |

También prueba una solicitud conjunta de esos cuatro campos bajo `A0`. Los
encabezados siguen la estructura del GET DATA existente; la compatibilidad
de estas selecciones concretas queda por verificar en tarjeta. La comparación
permite saber si es posible leer solo los campos estándar que consume el
driver. No supone que `C9` sea irrelevante ni cambia el parser o la política.

Se esperan 22 respuestas APDU: 11 selecciones y 11 consultas. Los SW negativos
son evidencia sobre las formas soportadas, sin convertirlos automáticamente
en fallo de transporte. Revisar que los controles completos se lean y comparar
sus contadores antes y después. El informe queda en `local/jcop3-pin-policy-*`.
No envía VERIFY, no solicita valor de PIN y no ejecuta administración.

La sintaxis del script, el objetivo Make y la construcción de encabezados se
comprobaron localmente. Una herramienta simulada validó las longitudes APDU,
los encabezados CHV 1 y los selectores de metadatos, y produjo las 22 respuestas
esperadas. Esta comprobación no demuestra que la tarjeta admita esas consultas.

## Resultado de selectores y soporte limitado del PIN: 2026-10-11

`local/jcop3-pin-policy-9U5otg3U.txt`, generado con el repositorio principal
`da14f65`, contiene las 22 respuestas previstas. Las selecciones directas de
`9A`, `9B`, `80` y `A1` devuelven `6A80`. Todas las selecciones anidadas bajo
`A0`, incluida la lista conjunta, devuelven `9000`, pero entregan el DOCP
completo de 45 bytes. Ninguna de esas formas filtra `C9` ni las ACL adicionales.
Los controles al principio y al final contienen los mismos bytes y reportan
7 intentos máximos y 7 restantes.

Se añadió una adaptación experimental al parser para el formato observado:

- Retiene `C9` en `chipdoc_c9`, sin atribuir significado a su valor. Solo se
  acepta `C9 01 01` en IAS/ECC Amos, clase CHV, referencia 1. Otros perfiles,
  referencias, valores o campos desconocidos siguen devolviendo error.
- Conserva las ACL completas de contacto y sin contacto. Para contacto, solo
  acepta la secuencia exacta observada `F3 E7 00 E7 FF 00 D0 D3 D3` y expone
  las condiciones `00` de VERIFY y GET DATA. Las otras operaciones permanecen
  sin soporte; una ACL distinta se rechaza, sin reemplazarla por acceso libre.
- La política resultante se marca `read_verify_only`. Los caminos de cambio
  y desbloqueo devuelven `SC_ERROR_NOT_SUPPORTED` antes de autenticar. El
  soporte de contacto sin PIN todavía debe validarse en tarjeta; tampoco
  implementa una política contactless.

La condición `00` permite ejecutar el comando VERIFY sin una autenticación
previa ni un canal protegido exigido por esa ACL; no acredita el PIN ni
autoriza por sí misma una firma. La tarjeta sigue comprobando el PIN que se
le envíe. El significado completo de `C9` y las condiciones administrativas
siguen pendientes, y este ajuste no equivale a soporte completo de ChipDoc.

`tests/iasecc-sdo.c` reproduce el DOCP capturado y verifica contadores,
conservación/copia de la extensión, ACL originales y rechazo de variantes.
`tests/iasecc-pin-policy.c` usa el driver y parser reales con transporte
simulado: comprueba lectura de contadores y que cambio/desbloqueo se detienen
antes de cualquier autenticación. Pasan 38 comprobaciones. La compilación
completa en `build/opensc-offline/`, sin PC/SC, terminó correctamente y se
registró en `chipdoc-policy-build.log`.

Para validar el estado del PIN y consultar el siguiente contexto de firma:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
make inspect/jcop3-key-policy
```

El último objetivo no recompila ni solicita PIN. Selecciona `ChipDoc` y pide
solo DOCP (`A0`) de la clase RSA_PRIVATE (`10`) para las referencias 2 y 3
declaradas en el PrKDF. No solicita la plantilla `7F48` ni componentes de las
llaves privadas. Guarda cuatro respuestas (dos selecciones, dos GET DATA)
en `local/jcop3-key-policy-*`. Los SW de error también permiten determinar
si esas referencias están disponibles en este contexto. Las tramas y el
recuento se comprobaron con una herramienta simulada, sin tarjeta.

Todavía no se ejecuta otra firma. Revisar primero el nuevo estado del PIN y
los permisos reales de las llaves; las referencias PKCS#15 no demuestran por
sí solas cómo se selecciona el objeto criptográfico para una operación RSA.

## Política RSA y prerrequisitos de firma: 2026-10-11

Se revisaron los informes `local/jcop3-pkcs11-cRVr2acc.txt` y
`local/jcop3-key-policy-egE7wr1j.txt`, de las 21:34 UTC, con OpenSC `840a7b204`.
El primero confirma que el parser acepta la política observada del PIN y
`iasecc_pin_get_policy()` devuelve límites máximo/mínimo 14/4 e intentos 7/7.
Las consultas vacías de estado siguen devolviendo `63 C7`. Persiste el error
ASN.1 de PuKDF; no impide esta enumeración ni es evidencia de PIN incorrecto.

Las dos referencias RSA_PRIVATE, 2 y 3, devuelven DOCP con `9000`. El tamaño
declarado es `01 00`, 256 bytes, compatible con RSA 2048. Ambas respuestas
contienen las mismas ACL de contacto y sin contacto:

```text
C0 13 90 13 A0 13 88 13 82 D3 81 00
```

Aplicando las máscaras de operaciones IAS/ECC definidas por OpenSC:

| Grupo | Operación | Condición conservada |
| --- | --- | --- |
| `C0 13` | PSO COMPUTE SIGNATURE | Autenticación de usuario mediante SE 3 |
| `90 13` | PSO DECIPHER | Autenticación de usuario mediante SE 3 |
| `A0 13` | INTERNAL AUTHENTICATE | Autenticación de usuario mediante SE 3 |
| `88 13` | GENERATE KEY | Autenticación de usuario mediante SE 3; no se prueba |
| `82 D3` | PUT DATA | SCB combinado `D3`, conservado sin ejecutar administración |
| `81 00` | GET DATA | Sin autenticación previa |

La referencia SE 3 no es necesariamente el PIN 3. Falta leer su plantilla
de autenticación para conocer qué CHV referencia. Los permisos del DOCP
tampoco demuestran por sí solos que una operación criptográfica funcione.

`iasecc_parse_acls()` solo procesaba el primer grupo, perdiendo los permisos
posteriores. Se amplió para recorrer toda la ACL, acumular las máscaras de
operaciones y conservar cada SCB. Acepta repeticiones idénticas y rechaza
condiciones alternativas distintas, que el arreglo `scbs[]` no representa.
También detecta truncamientos de grupos posteriores. Se mantiene la adaptación
limitada de CHV 1 con `C9`, incluido el rechazo de cambio y desbloqueo.

Las nuevas regresiones reproducen ambos DOCP RSA capturados, verifican las
conversiones a métodos/referencias de OpenSC y prueban duplicados, alternativas
y truncamientos. Pasan **49 comprobaciones** en total. La compilación completa
sin PC/SC terminó y quedó registrada en
`build/opensc-offline/rsa-acl-build.log`. Falta compilar y validar estos últimos
cambios con el entorno PC/SC del usuario.

Hay otro prerrequisito pendiente: TokenInfo no incluye `supportedAlgorithms`
y las llaves enumeradas muestran `Algo_refs: 0`. `format_senv()` copia la
tabla de TokenInfo al entorno usado por `iasecc_set_security_env()`, cuyo
`iasecc_get_algorithm()` necesita referencias para configurar RSA. Con esa
tabla vacía, el camino actual no encuentra referencias para SHA-256/RSA ni
para RSA PKCS#1 mediante INTERNAL AUTHENTICATE. No asignar valores a ciegas
ni tratar la lista de mecanismos PKCS#11 como prueba de estas referencias.

Antes de pedir otra autenticación, consultar:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-security-env
```

El segundo objetivo no recompila ni solicita PIN. Lee exclusivamente la
plantilla pública de SE 3 en ChipDoc y en la raíz, usando el GET DATA de
`iasecc_se_get_info()`:

```text
00 CB 3F FF 0A 4D 08 70 06 BF FB 03 02 7B 80 00
```

También selecciona ChipDoc y prueba la ruta `0003` declarada para la llave
de firma mediante `00 A4 09 04 02 00 03 00`, como el camino de selección
por ruta de IAS/ECC. No lee el objeto seleccionado. Esta comprobación permite
detectar una ruta no seleccionable antes del próximo intento autenticado.

Se esperan **seis respuestas**: tres selecciones de AID, dos consultas de SE
y una selección de ruta. El informe se guarda en
`local/jcop3-security-env-*`. No envía VERIFY, MSE, firma, consultas `7F48`
ni comandos administrativos. Una prueba con transporte simulado verificó
longitudes, referencias, contextos y número de respuestas. Su compatibilidad
real y el contenido de SE 3 siguen pendientes de la tarjeta.

## Consulta de SE sin lector: 2026-10-11, 18:31 de Costa Rica

Los cambios ya están guardados: repositorio principal `2ef3ccd` y OpenSC
`0c3e1c4f5`. El informe `local/jcop3-security-env-MMjEmIcC.txt`, de
`2026-10-12T00:31:57Z`, identifica la compilación con PC/SC, pero la lista
de lectores devuelve `No smart card readers found`. La consulta del ATR
falla con `Failed to connect to reader: No readers found` y estado 1.

El script se detiene antes de las consultas: no hay respuestas de SE 3 ni
de la ruta `0003`. Esto no aporta evidencia sobre su compatibilidad. Falta
restaurar la disponibilidad del lector y repetir
`make inspect/jcop3-security-env`, sin recompilar ni solicitar PIN. Si la
reconexión no lo resuelve, revisar servicio PC/SC y detección USB.

## SE 3 confirmado y ruta del objeto de firma: 2026-10-11, 18:49 de Costa Rica

Después de una ejecución adicional sin lector (`jcop3-security-env-3Ox7z8hz.txt`),
los informes `local/jcop3-security-env-yw7ohMX7.txt` y
`local/jcop3-security-env-OQCXHYVH.txt`, de las 00:49:16 y 00:49:19 UTC del
12 de octubre, contienen las seis respuestas previstas. Ambos usan OpenSC
`0c3e1c4f5` y tienen los mismos resultados.

GET DATA de SE 3 devuelve `9000` tanto en ChipDoc como en la raíz. La
respuesta pública, sin SW, es:

```text
70 27 BF FB 03 23 7B 21
B4 09 80 01 00 95 01 30 83 01 00
B8 09 80 01 00 95 01 30 83 01 00
A4 09 80 01 00 95 01 08 83 01 01
```

El parser existente conserva tres CRT. La plantilla AT (`A4`) tiene usage
`08`, USER PASSWORD, y referencia `01`: el entorno de seguridad exigido por
las ACL RSA apunta al **CHV 1**, cuya existencia global ya se comprobó.
El algoritmo `00` de esa plantilla describe la autenticación de usuario;
no proporciona las referencias ausentes de RSA/SHA-256.

La selección de ChipDoc funciona, pero SELECT por ruta `0003` devuelve
`6A82` en ambos informes. El DOCP RSA_PRIVATE 3 ya se leyó en esa aplicación:
la llave existe como objeto nativo, pero el camino PKCS#15 intentaba abrir
un archivo `0003` antes de configurar MSE.

Se añadió `_iasecc_fixup_signing_key_paths()` a la emulación IAS/ECC. Solo
actúa sobre Amos + AID Generic PKI, llave RSA nativa de 2048 bits, referencia
3 y ruta `0003` con ese mismo AID. Conserva el AID y la referencia de llave,
eliminando la porción de archivo. `select_key_file()` puede así seleccionar
solo la aplicación; MSE seguirá identificando el objeto RSA por su referencia.
Se aplica al cargar PrKDF y a los objetos ya cargados durante la inicialización.
La ruta de la llave de autenticación 2 se mantiene porque no se probó su
selección. Los permisos y la asociación con el PIN no cambian.

`tests/iasecc-key-path.c` comprueba la corrección, su idempotencia, exclusión
de otros perfiles y el selector PKCS#15 real con selección simulada.
`tests/iasecc-sdo.c` reproduce SE 3 y su referencia CHV 1, y rechaza la
respuesta truncada. Pasan **63 comprobaciones**. La compilación completa
sin PC/SC terminó; registro `build/opensc-offline/signing-path-build.log`.
Falta validar la nueva ruta en la enumeración de la tarjeta.

## Comprobar referencias de algoritmo, sin PIN

La tabla `supportedAlgorithms` de TokenInfo sigue ausente. Se añadió
`make inspect/jcop3-algorithms` para probar las plantillas MSE ya presentes
en `iasecc_set_security_env()` con las constantes de `iasecc-sdo.h`:

| Plantilla | APDU | Referencia candidata definida por OpenSC |
| --- | --- | --- |
| HT, SHA-256 | `00 22 41 AA 03 80 01 40` | `IASECC_ALGORITHM_SHA2 = 40` |
| DST, RSA/SHA-256 | `00 22 41 B6 06 84 01 83 80 01 42` | RSA_PKCS `02` combinado con SHA2 `40` |
| AT, RSA PKCS#1 | `00 22 41 A4 06 84 01 83 80 01 02` | `IASECC_ALGORITHM_RSA_PKCS = 02` |

`84 01 83` identifica la referencia de llave local 3 en las plantillas de
OpenSC. Estas referencias de algoritmo **todavía no están confirmadas para
la tarjeta**; no se ha añadido un fallback que las asuma. Un `9000` de MSE
solo demostraría aceptación de la configuración, no una firma correcta.

El diagnóstico selecciona ChipDoc para cada caso, configura HT/DST en una
misma conexión y compara AT en otra. Antes y después selecciona la raíz y
lee DOCP del CHV 1 para comparar contadores. Son **nueve respuestas**:
cuatro SELECT, dos GET DATA y tres MSE. Guarda el informe en
`local/jcop3-algorithms-*`.

MSE cambia el estado temporal de la operación; no es una consulta puramente
de lectura. El diagnóstico no transmite VERIFY, PIN, PSO, INTERNAL AUTHENTICATE,
administración de objetos ni consultas de componentes privados. No genera
llaves ni modifica archivos. Su secuencia, longitudes y recuento se comprobaron
con transporte simulado; falta ejecutarlo con la tarjeta.

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
make inspect/jcop3-algorithms
```

Revisar primero la ruta de firma corregida y los SW de MSE. Sigue pendiente
resolver las referencias ausentes en el driver. Antes del próximo intento
autenticado, revisar además `iasecc_set_security_env()`: actualmente solicita
la plantilla privada `7F48` mediante `iasecc_sdo_get_data()`, aunque para
configurar la operación usa tamaño y ACL del DOCP. Evaluar una lectura limitada
a DOCP para el perfil observado. No se ha ejecutado esa consulta privada en
estos diagnósticos ni producido una firma.

## MSE aceptado y preparación del próximo intento: 2026-10-11, 19:16 de Costa Rica

`local/jcop3-pkcs11-OwOOGiF2.txt`, de las 01:16:15 UTC del 12 de octubre,
usa OpenSC `fb767b3dd`. Confirma que la emulación aplica la nueva ruta de
firma: selecciona ChipDoc para el SDO 3, sin abrir el archivo `0003`.
La política del PIN sigue reportando límites 14/4 e intentos 7/7. Persisten
los avisos conocidos de PuKDF; no se produjo un nuevo login.

`local/jcop3-algorithms-FinI8oaD.txt`, de las 01:16:28 UTC, contiene las
nueve respuestas previstas, todas `9000`. La tarjeta acepta HT `40`, DST
`42` y AT `02` con referencia local de llave `83`. Los DOCP del CHV 1 al
principio y al final son idénticos y conservan máximo/restantes 7/7. Esto
confirma la aceptación de las configuraciones MSE probadas, no una firma.

Se agregó un fallback de referencias en `card-iasecc.c`, limitado al contexto
observado: Amos, DF seleccionado por nombre con AID Generic PKI, RSA 2048,
referencia nativa 3 y tabla de algoritmos vacía. Resuelve exclusivamente
SHA-256 (`40`), RSA/SHA-256 (`42`) y RSA PKCS#1 (`02`). Una tabla declarada,
incluso incompleta o con referencia cero, tiene prioridad. No se infieren
SHA-1, descifrado, otras llaves, tamaños, algoritmos o aplicaciones.

En ese mismo perfil, `iasecc_set_security_env()` solicita únicamente DOCP
para obtener tamaño y ACL, y comprueba tamaño 256 bytes. No solicita la
plantilla RSA privada `7F48`. Conserva el camino previo para otros perfiles.
Los metadatos temporales se liberan después de convertir las condiciones
de acceso y antes de configurar MSE, también si falla su preparación.

`tests/iasecc-sign-setup.c` ejecuta el driver y parser reales con transporte
simulado que solo acepta el DOCP público capturado y las tres plantillas
MSE observadas. Rechaza consultas privadas, PIN y operaciones criptográficas.
Verifica la preparación HT/DST y AT, propagación de un rechazo MSE sin
reintento, exclusión de perfiles distintos y prioridad de tablas declaradas.
Antes del cambio fallaban las cuatro comprobaciones iniciales; después pasan
**80 comprobaciones** en toda la suite. La compilación completa sin PC/SC
terminó; registro `build/opensc-offline/sign-setup-build.log`.

El helper de firma añade `use_pin_caching = no` a su configuración PKCS#15.
`sc_pkcs15_pincache_revalidate()` devuelve error sin volver a verificar el
PIN cuando esa opción está desactivada. Así, un fallo posterior de firma
no causa la revalidación automática de un PIN guardado por PKCS#15. Se
mantienen `OPENSC_DEBUG=0`, `debug = 0`, entrada interactiva sin eco y un
solo comando autenticado, sin bucle de reintentos.

La siguiente prueba necesita la terminal y la tarjeta del usuario:

```bash
make test/iasecc JOBS="$(nproc)"
make sign/jcop3
```

Ejecutar la firma una vez, directamente en la terminal, sin redirigir stdout
ni pasar el comando por `tee`. Firmará el texto técnico que muestra el helper;
no un PDF. Todavía no se ha validado login con este conjunto de correcciones,
ni una operación RSA efectiva. Revisar el informe de
`local/jcop3-signature-*` para distinguir login, firma y verificación OpenSSL.
Si falla, detenerse e investigar el resultado antes de otra autenticación.

## Fallo antes del PIN: 2026-10-11, 19:28 de Costa Rica

Las ejecuciones guardadas en `jcop3-signature-FktWTv57`,
`jcop3-signature-jQYkimJu` y `jcop3-signature-54OpXE2f`, a las
01:28:26, 01:28:46 y 01:28:53 UTC del 12 de octubre, usan OpenSC
`70d7afc20`. Los tres informes muestran el mismo fallo: `No slots.` y
estado de salida 1 al ejecutar `--read-object --type cert`.

El helper no llegó a ejecutar `--login --sign`. En `pkcs11-tool.c`,
`No slots.` se emite después de inicializar el módulo y enumerar slots,
antes de abrir sesión, leer el certificado o autenticar. Por ello, estas
tres ejecuciones no verificaron un PIN ni probaron la nueva preparación RSA.
No hay evidencia de un PIN incorrecto en ellas.

El mensaje establece que la enumeración PKCS#11 está vacía, pero no explica
su causa. Podría faltar visibilidad del lector o acceso al servicio PC/SC;
también hay que revisar la configuración de slots. `build/opensc/config.h`
confirma `ENABLE_PCSC=1` y la biblioteca configurada de pcsclite existe.
Esto descarta que esta compilación sea la variante offline, pero no prueba
que pueda acceder al servicio o al lector durante la ejecución del usuario.

El siguiente diagnóstico usa la compilación existente, sin PIN ni firma:

```bash
make inspect/jcop3-pkcs11
```

Ejecutarlo dentro de `nix develop`, con el lector conectado y la tarjeta
insertada. Revisar primero lector y ATR, después slots y objetos. No repetir
firma ni los diagnósticos MSE mientras esté pendiente ese resultado.

## Enumeración recuperada: 2026-10-11, 19:39 de Costa Rica

`local/jcop3-pkcs11-XtNrgvHO.txt`, de las 01:39:13 UTC del 12 de octubre,
usa OpenSC `70d7afc20`. PC/SC detecta el lector OMNIKEY con tarjeta y el
ATR esperado. El mismo módulo PKCS#11 devuelve el slot 0 y enumera los dos
certificados y dos llaves públicas RSA de 2048 bits, incluida `LlaveDeFirma`.
Las herramientas terminan con estado 0; persiste el aviso conocido de
PuKDF `-1402`, por lo que no se considera resuelto ese parsing.

La emulación aplica las correcciones de ruta del PIN global y de la llave
de firma. Las consultas públicas del PIN indican límites máximo/mínimo
14/4 e intentos máximo/restantes 7/7. No se ejecutó un login ni una firma.

El error `No slots.` no se reprodujo. Esto confirma disponibilidad del lector
y enumeración con esa configuración en este diagnóstico, sin determinar
por qué fallaron los tres comandos anteriores. La inspección usa depuración
9 y no fija `use_pin_caching`; el helper de firma mantiene depuración 0 y
caché de PIN desactivada. No se deben cambiar esas protecciones para firmar.

El siguiente paso es **una ejecución** de `make sign/jcop3`, directamente
en la terminal del usuario dentro de `nix develop`, sin recompilar ni
redirigir stdout. El helper primero intenta leer el certificado y solo
después ejecuta el comando autenticado. Revisar el nuevo informe ante
cualquier fallo antes de repetirlo. Sigue pendiente producir y verificar
una firma real con el conjunto actual.

## Login aceptado y fallo en QSign: 2026-10-11, 19:41 de Costa Rica

`local/jcop3-signature-CXq9hBMQ/report.txt`, de las 01:41:29 UTC del 12 de
octubre, usa OpenSC `70d7afc20`. El helper leyó el certificado y extrajo la
llave pública. El comando autenticado pasó el login y eligió
`SHA256-RSA-PKCS`, pero terminó en `C_SignFinal` con
`CKR_FUNCTION_NOT_SUPPORTED (0x54)`. No produjo una firma ni ejecutó la
verificación OpenSSL. El PIN no figura en el informe. Sin APDU autenticadas
no se puede identificar el comando físico exacto que produjo el fallo.

La revisión del código encuentra un bloqueo reproducible:
`iasecc_qsign_data_sha256()` devuelve incondicionalmente
`SC_ERROR_NOT_SUPPORTED` para OpenSSL 3. El transporte simulado reproduce
ese error con la ruta PKCS#15 real después de aceptar DOCP y HT/DST, antes
de transmitir PSO HASH o PSO SIGN. Además, al no declarar `algo_refs`,
`pkcs15_prkey_can_do()` provoca hashing en software en la capa PKCS#11;
las flags de SHA-256 aún llevan la operación al camino QSign del driver.
Resolver el mecanismo combinado exige revisar ambas capas, no solo retirar
el guard de versión de OpenSSL. Su soporte sigue pendiente.

Para el primer hito se cambió el helper a `RSA-PKCS` con DigestInfo SHA-256
calculado antes del login. `scripts/sha256-digestinfo.sh` genera 51 bytes:
19 bytes de cabecera DER (OID SHA-256 y NULL) y el hash de 32 bytes del texto
original. La tarjeta agrega el padding PKCS#1 v1.5 y realiza RSA. El driver
existente usa MSE AT `02`, con referencia local de llave `83`, e INTERNAL
AUTHENTICATE para esta modalidad; su ACL RSA también refiere al SE 3.
La aceptación previa de MSE no acredita que la operación criptográfica vaya
a funcionar. No se modificó el submódulo ni el tratamiento del PIN.

`tests/iasecc-rsa-pkcs.c` usa el formato PKCS#15, padding y driver reales con
transporte simulado. Comprueba selección del AID, GET DOCP público, AT `02`
y transmisión del DigestInfo SHA-256 de `abc` sin volver a hashearlo.
Verifica que fallos de MSE o de RSA detienen esa ruta PKCS#15 sin reintento
ni revalidación de PIN. Su respuesta de 256 bytes es sintética, no una firma.
También reproduce el fallo QSign actual con OpenSSL 3.
`tests/sha256-digestinfo.sh` compara la salida DER completa con el vector
conocido de `abc` y rechaza una entrada inexistente sin producir bytes.

Pasan **86 comprobaciones** en la suite local, con registro
`build/opensc-offline/rsa-pkcs-tests.log`; terminó la compilación completa
sin PC/SC (`rsa-pkcs-build.log`). Estas pruebas no usan tarjeta ni generan
llaves. Para validar en el entorno del usuario:

```bash
make test/iasecc JOBS="$(nproc)"
make sign/jcop3
```

Solo ejecutar la firma si las pruebas pasan, una vez y directamente en la
terminal. El helper conserva depuración 0 y caché de PIN desactivada.
Ahora registra `mechanism=RSA-PKCS`, `digest_algorithm=SHA256` y firma
`digest-info.der`, pero OpenSSL verifica siempre contra el `message.txt`
original. El hito solo se cumple si esa verificación tiene éxito.

## Rechazo de longitud en login: 2026-10-11, 19:59 de Costa Rica

El usuario confirmó que las pruebas pasan. El informe
`local/jcop3-signature-Pwfndadv/report.txt`, de las 01:59:58 UTC del 12 de
octubre, usa el mismo OpenSC `70d7afc20` y el helper actualizado comprometido
en `62e6d5a`. La lectura del certificado, extracción de llave pública y
preparación de DigestInfo terminaron correctamente. El comando `RSA-PKCS`
se detuvo en `C_Login` con `CKR_PIN_LEN_RANGE (0xa2)`. No ejecutó la firma
RSA ni la verificación OpenSSL; este informe no permite evaluar aún la
operación INTERNAL AUTHENTICATE.

La revisión de `pkcs15_login()`, `sc_pkcs15_verify_pin()` y `_validate_pin()`
identifica un rechazo de longitud antes del VERIFY con el PIN. La validación
PKCS#15 inicial usa los atributos AODF (mínimo 4, máximo 20); la política
pública CHV (mínimo 4, máximo 14) se combina posteriormente en el driver.
No confundir esos dos rangos ni ajustar sus límites para forzar el login.
`SC_ERROR_INVALID_PIN_LENGTH` se convierte en `CKR_PIN_LEN_RANGE`, distinto
del error de PIN incorrecto. El control posterior de máximo en
`sc_build_pin()` devuelve `SC_ERROR_INVALID_ARGUMENTS`, por lo que no explica
por sí solo el código observado.

La captura no contiene la entrada ni su longitud, y no hubo una consulta
posterior del contador. El login anterior sí había sido aceptado con el
mismo driver. Antes de solicitar otro intento, aclarar si el usuario
introdujo el mismo PIN o cometió un error de entrada, sin pedir su valor.
Si confirma la misma entrada, investigar los metadatos públicos y la lectura
desde la terminal. Conservar depuración 0 durante cualquier autenticación.

El usuario respondió que no puede asegurar si introdujo el mismo PIN, aunque
cree que sí. No se considera confirmado un error de entrada. La revisión de
`util_getpass()` y su llamada desde `pkcs11-tool` no identifica un cambio
entre el login aceptado y el rechazo de longitud. Se solicita una consulta
`make inspect/jcop3-pkcs11` sin PIN para contrastar los atributos AODF,
la política pública CHV y los intentos restantes después del fallo.
No recompilar ni repetir la firma antes de revisar ese nuevo informe.

## Metadatos posteriores al rechazo: 2026-10-11, 20:12 de Costa Rica

`local/jcop3-pkcs11-EqveuSwF.txt`, de las 02:12:28 UTC del 12 de octubre,
usa OpenSC `70d7afc20`, con el submódulo limpio y el mismo proveedor PC/SC.
Detecta el lector, el ATR esperado y el slot 0; enumera los dos certificados
y las dos llaves públicas RSA. Las correcciones de ruta del CHV global
y de la llave de firma siguen aplicadas. Los comandos terminan con estado 0,
aunque persiste el error conocido de PuKDF `-1402`.

Los atributos AODF conservan mínimo 4, máximo 20 y longitud almacenada 11.
El slot anuncia esos límites 4/20. La consulta de política CHV indica
máximo/mínimo 14/4 y máximo/restantes 7/7. El VERIFY vacío de estado devuelve
`63 C7`, sin enviar el valor del PIN. Estos resultados coinciden con la
consulta anterior al login aceptado de las 19:41: no se observa un cambio
de límites ni una reducción del contador tras el rechazo de longitud.
La entrada que originó ese rechazo sigue sin conocerse.

No se necesita cambiar el driver ni recompilar por esta evidencia. El
siguiente paso es un único `make sign/jcop3`, directamente en la terminal,
escribiendo el PIN habitual solo después del prompt. Mantener depuración 0
y no redirigir stdout. Revisar el nuevo informe si falla, antes de otro
login. La ruta RSA-PKCS con DigestInfo aún no se ha probado después de un
login exitoso; no existe todavía una firma verificada.

## Primera firma verificada: 2026-10-11, 20:18 de Costa Rica

`local/jcop3-signature-crUXrnAi/report.txt`, de las 02:18:39 UTC del 12 de
octubre, usa OpenSC `70d7afc20`, sin cambios locales. El helper leyó el
certificado de `LlaveDeFirma`, extrajo su llave pública y preparó DigestInfo
SHA-256 de 51 bytes a partir del texto técnico de 82 bytes. El login pasó
y `RSA-PKCS` produjo `signature.bin` de 256 bytes. OpenSSL verificó la firma
contra el texto original con `Verified OK`; la ejecución terminó con estado 0.

La comprobación independiente extrajo nuevamente la llave pública del
certificado DER, la comparó con `public-key.pem`, recalculó y comparó el
DigestInfo y verificó la firma con OpenSSL. También devolvió `Verified OK`.
Estas comprobaciones no comunicaron con la tarjeta ni necesitaron el PIN.
Los archivos permanecen en `local/`, con permisos 0600, fuera de Git.

Se completó el primer hito: firma RSA SHA-256 real mediante PKCS#11 y
verificación con la llave pública del certificado. No se cambió el PIN
ni su tratamiento para este intento; la causa del rechazo de longitud
anterior sigue sin confirmarse. No hubo depuración APDU autenticada, por
lo que la secuencia física exacta no se capturó. La ruta del código y sus
regresiones usan MSE AT `02` e INTERNAL AUTHENTICATE para RSA-PKCS.

El siguiente paso es `make firmador/jcop3`. El nuevo helper fija `LIBASEP11`
al módulo compilado y `OPENSC_CONF` a un archivo privado recién creado en
`local/`, con el mismo contenido que el perfil de la firma verificada:
depuración 0, emulación PKCS#15 primero y cachés de archivos/PIN desactivadas.
Conserva la salida en la terminal y ejecuta el firmador instalado sin
recompilar ni instalar. Su lanzador respeta `LIBASEP11` y conserva el entorno
al ejecutar Java. Comprobar el certificado y una firma de un PDF de prueba;
ante fallo, investigar antes de otro intento.

El lanzador pasó la comprobación de sintaxis y se ejecutó con un `firmador`
simulado que comprobó la ruta del módulo, `OPENSC_DEBUG=0`, el perfil idéntico
al de la firma y la sustitución de variables heredadas. Los archivos de
configuración quedaron con permisos 0600; se comprobó propagación de estado
0 y 17 y rechazo de argumentos. No se abrió Java ni se accedió al lector.

La verificación técnica no demuestra compatibilidad con el firmador:
podría seleccionar otro mecanismo. `SHA256-RSA-PKCS` y QSign siguen pendientes,
al igual que PuKDF `-1402`, concurrencia, otras llaves/mecanismos y validación
de versiones/plataformas. No se reclama soporte completo de IAS/ECC o JCOP3.

## Integración con firmador: mecanismo combinado y firma nula

El usuario ejecutó `make firmador/jcop3` y recibió
`sun.security.pkcs11.wrapper.PKCS11Exception: CKR_FUNCTION_NOT_SUPPORTED`,
seguido de un aviso sobre sello de tiempo/revocación y
`signatureValue cannot be null!`. No hay una firma PDF verificada. El aviso
no demuestra un problema de Internet: la ausencia del valor de firma basta
para impedir las operaciones posteriores. No se capturaron APDU autenticadas
ni un stack trace que identifique la función PKCS#11 exacta que falló.

La revisión estática del firmador instalado confirma que configura
SunPKCS11. El código fuente de OpenJDK 21 incluido en su `lib/src.zip`
registra `SHA256withRSA` con preferencia por `CKM_SHA256_RSA_PKCS`, seguido
de `CKM_RSA_PKCS` y `CKM_RSA_X_509`. Esto señala el mecanismo combinado ya
fallido como candidato, sin confirmar qué operación ejecutó esa sesión.

La ruta reproducible de OpenSC tiene un problema de negociación: al no
declarar `algo_refs`, PKCS#11 calcula SHA-256 en software y entrega el digest
de 32 bytes a PKCS#15, pero los flags predeterminados de IAS/ECC todavía
encaminan ese digest a QSign. Ese camino espera el mensaje para calcular
su estado de hash y devuelve `SC_ERROR_NOT_SUPPORTED` con OpenSSL 3.

Se agregó `_iasecc_fixup_signature_algorithms()` en `pkcs15-iasecc.c`,
aplicado después de corregir la ruta de firma. Solo actúa en Amos + Generic
PKI, con llave nativa RSA 2048, referencia 3, permiso SIGN, ruta corregida
al AID y ausencia de algoritmos declarados tanto en la llave como en
TokenInfo. Si RSA 2048 ofrece PKCS#1 y HASH_NONE y no anuncia RSA RAW,
retira únicamente su flag de hash SHA-256. Esta capacidad es compartida
por las llaves RSA 2048 de la tarjeta; no se valida por ello la llave de
autenticación. No cambia SHA-1, otros tamaños de RSA ni ECC.

Con esa capacidad corregida, `sc_get_encoding_flags()` hace que PKCS#15
agregue DigestInfo al digest ya calculado y seleccione RSA-PKCS con AT `02`.
El mecanismo combinado sigue disponible mediante hashing en software;
no se rebaja SHA-256 ni se modifica el firmador. No se implementó QSign
con hashing en hardware.

Pasan **104 comprobaciones**, registradas en
`build/opensc-offline/chipdoc-software-hash-tests.log`. Las nuevas regresiones
comprueban el DigestInfo completo de `abc` en la ruta real PKCS#15/driver,
rechazo de un digest de longitud incorrecta antes del transporte, conservación
de RSA-PKCS explícito y rechazo de perfiles/algoritmos fuera del ajuste.
La compilación completa sin PC/SC terminó correctamente en
`chipdoc-software-hash-build.log`. Esto no valida el mecanismo combinado
en hardware ni una firma PDF.

Se añadió `make sign/jcop3-sha256` para probar `SHA256-RSA-PKCS` con el texto
original como entrada, sin hashearlo en el script antes de enviarlo al módulo.
Se conserva la preparación de DigestInfo como archivo de referencia local.
El helper mantiene terminal obligatoria, depuración 0, caché PIN desactivada,
un comando autenticado y verificación OpenSSL sobre el texto original.
`make sign/jcop3` sigue usando RSA-PKCS explícito.

Después de guardar los commits y pasar `make test/iasecc JOBS="$(nproc)"`,
realizar **una ejecución** de `make sign/jcop3-sha256`. Revisar el informe
antes de repetir login o volver al firmador. Solo si OpenSSL confirma
`Verified OK`, probar nuevamente `make firmador/jcop3` tras cerrar cualquier
instancia anterior, con un PDF de prueba y sin depuración APDU.

## Mecanismo combinado verificado: 2026-10-11, 20:37 de Costa Rica

`local/jcop3-signature-kiHbc1Qh/report.txt`, de las 02:37:33 UTC del 12 de
octubre, usa OpenSC `34a040c32`, sin cambios locales, y el helper comprometido
en el repositorio principal `3dd45c9`. Lee el certificado y acepta el login.
La entrada de `SHA256-RSA-PKCS` es el `message.txt` original de 82 bytes.
Produce una firma de 256 bytes; OpenSSL confirma `Verified OK` sobre ese
texto, con estado final 0. Se mantuvo depuración APDU desactivada.

La comprobación independiente extrajo nuevamente la llave pública del
certificado DER y la comparó con `public-key.pem`, recalculó y comparó el
DigestInfo y volvió a verificar la firma contra el texto original. Todas
las comprobaciones pasaron, sin acceder al lector ni introducir el PIN.
Esto valida el mecanismo combinado en la tarjeta observada mediante hashing
en software y la operación RSA-PKCS; no acredita hashing QSign en hardware.

La corrección ya está comprometida en el submódulo y registrada por el
repositorio principal. No se necesita recompilar ni repetir esta firma.
El siguiente paso es cerrar cualquier instancia anterior de firmador y
ejecutar `make firmador/jcop3` desde `nix develop`, con un PDF de prueba.
Revisar cualquier fallo antes de otro intento y verificar el PDF si se
produce. Todavía no hay una firma PDF verificada con el firmador y OpenSC.

## Firmador: autenticación pendiente y diagnóstico de sesiones

Después de verificar el mecanismo combinado, el usuario ejecutó
`make firmador/jcop3` y recibió `CKR_USER_NOT_LOGGED_IN`, seguido de los
errores de `signatureValue cannot be null!` y el aviso de sello/revocación.
Confirma que el programa pidió el PIN, pero no sabe qué certificado estaba
seleccionado. Pedir el PIN no acredita por sí solo un login exitoso.
No hay captura de este intento ni una firma PDF válida. No se atribuye
el fallo a PIN incorrecto, certificado o conectividad.

La revisión estática del JAR instalado muestra varios componentes que usan
PKCS#11: `PKCS11Manager` enumera certificados con sesiones propias y crea
proveedores SunPKCS11; `CRSigner` crea además un `Pkcs11SignatureToken` de
DSS para firmar. Esto justifica observar apertura/cierre, login/logout y
estado de sesiones; no demuestra que esos componentes causen el fallo.

En OpenSC, `C_GetSessionInfo` consulta `slot_get_logged_in_state`, que ya
lee el estado del PIN mediante PKCS#15. La lectura selecciona el contexto
global del PIN; queda pendiente comprobar si esa interacción influye en
la autenticación de esta tarjeta. También se puede devolver
`CKR_USER_NOT_LOGGED_IN` al traducir `SC_ERROR_SECURITY_STATUS_NOT_SATISFIED`
de una operación. Sin la función que devuelve el error no se puede
distinguir estos caminos. No se cambian selección, autenticación ni cachés
para probar una hipótesis.

Se agregó un diagnóstico temporal y optativo al módulo PKCS#11:
`sc_pkcs11_api_trace()` en `debug.c`. Solo acepta un evento de un conjunto
fijo y cuatro enteros, con salida independiente de los niveles de debug.
Registra retorno de funciones de sesión, login/logout, búsqueda/atributos
y firma. `PIN_STATUS` registra el resultado de la consulta de estado ya
existente; no ejecuta consultas adicionales. `SIGN_KEY` registra metadatos
públicos de la llave seleccionada por la ruta de firma. No modifica el
resultado de las llamadas y conserva `errno` ante errores de escritura.
Algunos retornos tempranos de argumentos/bloqueo no pasan por el registro.

Cada línea usa `evento session=... rv=... a=... b=...`, en hexadecimal:

| Evento | Campo `a` | Campo `b` |
| --- | --- | --- |
| `C_OpenSession` | slot | flags de sesión |
| `C_CloseAllSessions` | slot | 0 |
| `C_GetSessionInfo` | estado de sesión si retorna OK | 0 |
| `C_Login` | tipo de usuario | 0 |
| `C_SignInit` | mecanismo | handle de llave |
| `C_GetAttributeValue` | handle de objeto | 0 |
| `C_Sign`, `C_SignFinal` | 1 si hay buffer de salida, 0 si consulta tamaño | 0 |
| `PIN_STATUS` | flags públicos de estado del PIN | usuario guardado por OpenSC |
| `SIGN_KEY` | referencia pública de llave (3 firma, 2 autenticación) | bits de módulo RSA |

`rv` es el retorno PKCS#11, excepto `PIN_STATUS`, que conserva el resultado
interno firmado de OpenSC convertido a entero sin signo. `SIGN_KEY rv=0`
marca que se seleccionó la llave; no acredita una firma exitosa. El registro
no contiene PIN, longitud del PIN, atributos, certificados, documentos,
digest, firma ni APDU. No se habilita con `OPENSC_DEBUG`.

El nuevo objetivo `make firmador/jcop3-diagnose` precrea
`local/jcop3-firmador-api-*/api.log` con permisos 0600 en un directorio 0700,
junto con configuración y versión/estado Git. Activa exclusivamente
`OPENSC_API_TRACE_FILE` y conserva la configuración de la firma verificada:
debug 0, emulación primero, cachés de archivo/PIN desactivadas. No captura
stdout/stderr del firmador. `make firmador/jcop3` elimina esa variable del
entorno y mantiene el registro desactivado.

Pasan **108 comprobaciones** en
`build/opensc-offline/pkcs11-api-trace-tests.log`, incluidas cuatro para el
registro real: desactivado por defecto, contenido exacto limitado a
metadatos, rechazo de eventos desconocidos y conservación de `errno`.
La compilación completa sin PC/SC pasó y está registrada en
`pkcs11-api-trace-build.log`. Una aplicación simulada comprobó ambos modos
del helper, configuración idéntica a la firma, sustitución de variables
heredadas, permisos y propagación del código de salida. No accedió al lector.

Pendiente: guardar commits primero en el submódulo y luego en el repositorio
principal, compilar con `make test/iasecc JOBS="$(nproc)"`, cerrar instancias
anteriores del firmador y hacer **un intento** con
`make firmador/jcop3-diagnose`. Cerrar el programa y revisar el informe antes
de repetir login. No se necesita repetir las firmas técnicas verificadas.
Todavía no se ha capturado este diagnóstico en hardware ni se ha corregido
el fallo de autenticación. No activar caché de PIN, `atomic` ni `lock_login`
sin resolver qué interacción falla.

## Diagnóstico del firmador revisado: 2026-10-11, 21:06 de Costa Rica

`local/jcop3-firmador-api-1LBmDPbP/report.txt`, de las 03:06:20 UTC del 12
de octubre, registra OpenSC `f7752be36`, sin cambios locales. El repositorio
principal está en `7eb8fb0`. Ambos árboles estaban limpios al iniciar esta
revisión. `api.log` contiene 88 líneas de metadatos, con la siguiente secuencia:

1. La enumeración inicial abre y cierra sesiones. Un `C_Logout` previo al
   login devuelve `CKR_USER_NOT_LOGGED_IN`; no es el fallo de firma posterior.
2. `C_Login` con `CKU_USER` devuelve **`CKR_OK`**. No hay otro login en el registro.
3. Después del login, cuatro `C_GetSessionInfo` informan `CKS_RW_USER_FUNCTIONS`.
   Las consultas existentes de PIN retornan éxito y `SC_PIN_STATE_LOGGED_IN`.
4. `C_SignInit` con `CKM_SHA256_RSA_PKCS (0x40)` pasa; `C_SignUpdate` también.
5. `SIGN_KEY` confirma referencia **3**, RSA **2048**: es `LlaveDeFirma`.
6. El único `C_SignFinal` devuelve **`CKR_USER_NOT_LOGGED_IN (0x101)`**.

No hay logout, cierre de sesión ni reinserción indicada entre el login y
la firma. La sesión utilizada para firmar es distinta de la de login;
PKCS#11 comparte autenticación entre sesiones del token, y las consultas
la reconocen en la nueva sesión. El usuario comunicó tres mensajes iguales,
pero esta captura contiene una sola operación final de firma, no tres
verificaciones del PIN. No consta una firma PDF válida.

La evidencia descarta una elección de la llave de autenticación para esta
operación y confirma que el login pasó. Las consultas de estado indican
autenticación en ese momento; no prueban que conserve todas las condiciones
de acceso al terminar las selecciones posteriores. No se conoce todavía el
punto interno del rechazo. No modificar PIN, ACL o sesiones para eludirlo.

La ruta `pkcs15_prkey_sign → sc_pkcs15_compute_signature → use_key` puede
recibir `SC_ERROR_SECURITY_STATUS_NOT_SATISFIED` al seleccionar la llave,
preparar MSE o ejecutar RSA. PKCS#11 lo traduce a `CKR_USER_NOT_LOGGED_IN`.
El intento de revalidación de PKCS#15 queda rechazado al estar desactivada
la caché PIN; no se debe habilitar para ocultar la causa. El registro de
sesiones inicial no distingue esas etapas.

Se amplió el diagnóstico existente, sin comandos adicionales ni cambios
de autenticación. `libopensc/api-trace.h` comparte el mismo logger de nombres
fijos y enteros entre PKCS#11, PKCS#15 y el driver. Continúa desactivado por
defecto y conserva `errno`; `sc_pkcs11_api_trace()` delega en él. El nuevo
header está incluido en la distribución mediante `noinst_HEADERS`.

| Evento nuevo | Resultado `rv` | `a` | `b` |
| --- | --- | --- | --- |
| `KEY_SELECT` | selección de la ruta de llave | operación PKCS#15 solicitada | 0 |
| `KEY_MSE` | preparación del entorno de seguridad | operación solicitada | 0 |
| `KEY_COMPUTE` | operación criptográfica, longitud si tuvo éxito | operación solicitada | 0 |
| `KEY_REVALIDATE` | llamada existente de revalidación; se espera rechazo con caché desactivada | operación solicitada | 0 |
| `CARD_KEY_DOCP` | lectura de metadatos públicos de la llave ChipDoc | referencia de llave | 0 |
| `CARD_MSE` | verificación del resultado de MSE | palabra de estado de respuesta | operación efectiva del driver |
| `CARD_SIGN_AT` | retorno del transporte o verificación de resultado RSA AT | palabra de estado, 0 si falla transporte | 0 |

Estos resultados son enteros internos de OpenSC convertidos a `unsigned long`,
no códigos PKCS#11. El retorno negativo conserva su representación en
hexadecimal. Las palabras de estado son únicamente códigos de resultado:
no se registran encabezados, datos ni buffers APDU, PIN, longitud del PIN,
documento, digest, firma o componentes de llaves. El logger no reintenta
ninguna operación ni cambia los errores que recibe el llamador.

Pasan **112 comprobaciones**. Las cuatro nuevas usan transporte simulado
y comprueban la separación de selección/MSE/RSA, un rechazo de MSE que
detiene RSA, un rechazo de seguridad simulado `6982` con revalidación
bloqueada y una selección rechazada que no alcanza MSE. El estado `6982`
de esta prueba es sintético: **todavía no se observó en esta sesión real**.
Se comprueban también los contadores de transporte para asegurar que el
registro no añade comandos ni repite RSA. Las regresiones están en
`build/opensc-offline/chipdoc-sign-stages-tests.log`; la compilación completa
sin PC/SC está en `chipdoc-sign-stages-build.log`.

Falta ejecutar la ampliación en hardware: guardar sus commits, compilar
con `make test/iasecc JOBS="$(nproc)"`, cerrar instancias anteriores del
firmador y hacer una sola prueba con `make firmador/jcop3-diagnose`.
Cerrar el programa y revisar el nuevo registro antes de otro intento.
No repetir las firmas técnicas verificadas ni ejecutar el diagnóstico
con la versión anterior esperando obtener estos eventos. El problema de
autenticación sigue pendiente de localizar y corregir.

## RSA rechazado y prueba aislada de estado de sesión: 2026-10-11, 21:24

`local/jcop3-firmador-api-hBIzcp79/report.txt`, de las 03:24:32 UTC del 12
de octubre, usa OpenSC `a19ee5927`, sin cambios locales. El repositorio
principal está en `e12692b`; ambos árboles estaban limpios al revisar la
captura. El registro contiene 95 líneas y un solo login/firma:

- `C_Login CKR_OK`; cuatro consultas de sesión informan PIN autenticado
  y `CKS_RW_USER_FUNCTIONS`, sin logout/cierre intermedio.
- `CKM_SHA256_RSA_PKCS`, llave 3, RSA 2048.
- `KEY_SELECT`, `CARD_KEY_DOCP`, `CARD_MSE (9000)` y `KEY_MSE` pasan.
- **`CARD_SIGN_AT` devuelve `6982`**, traducido a
  `SC_ERROR_SECURITY_STATUS_NOT_SATISFIED (-1211)`.
- `KEY_COMPUTE` conserva ese error; la llamada existente de revalidación
  queda rechazada, con caché PIN desactivada. No hay segunda operación RSA.
- `C_SignFinal` lo traduce a `CKR_USER_NOT_LOGGED_IN`.

Esto localiza el rechazo en la tarjeta al ejecutar RSA AT, después de una
selección y configuración MSE exitosas. No es un fallo de enumeración,
elección de llave ni negociación de SHA-256 en esta sesión. `6982` no
determina por sí solo qué condición de autenticación quedó insatisfecha.
No hay firma PDF verificable; los avisos de `signatureValue cannot be null!`
son posteriores a ese rechazo y no demuestran un problema de Internet.

Una diferencia observable con la firma técnica verificada es que Java
llama repetidamente a `C_GetSessionInfo` después del login. Cada consulta
ejecuta `sc_pkcs15_get_pin_info`, que selecciona la ruta MF corregida, y
`iasecc_pin_get_info`, que consulta estado y después política. La lectura de
política vuelve a seleccionar MF y restaura el DF guardado. Las consultas
de estado informan PIN autenticado, pero no muestran si esas selecciones
afectan condiciones adicionales necesarias para firmar. Esta relación
es una **hipótesis**, no una causa demostrada.

Se añadió `make firmador/jcop3-session-state`, una prueba aislada optativa
que activa `OPENSC_JCOP3_SESSION_STATE=yes`, además del mismo registro de
metadatos. El cambio en `framework-pkcs15.c` solo omite la consulta física
de PIN de `C_GetSessionInfo` si ya hay login CKU_USER y coinciden Amos,
AID Generic PKI, PIN de tipo CHV global referencia 1 y ruta MF `3F00`
sin AID. PIN local, SO, PUK, otros perfiles y sesiones sin login siguen
el comportamiento anterior. El login inicial mantiene su verificación
normal; el experimento no acredita ni crea un login.

Al omitir la consulta retorna `SC_PIN_STATE_UNKNOWN`, y el código existente
de `C_GetSessionInfo` conserva el estado de sesión a partir de
`slot->login_user`. No presenta ese valor como una lectura física del PIN.
`SESSION_LOGIN_STATE session=0 rv=0 a=0 b=1` distingue el experimento de
`PIN_STATUS`. La detección de presencia/cambio de tarjeta de
`C_GetSessionInfo`, login/logout y errores de firma siguen ejecutándose.
No se añaden comandos, autenticaciones, reintentos ni almacenamiento de PIN.

Los objetivos normal y `firmador/jcop3-diagnose` eliminan la variable
experimental heredada. Solo el objetivo nuevo la activa; `report.txt`
incluye `session_state_experiment=yes` para identificar la comparación.
Se conservan depuración 0, emulación primero, cachés de archivos/PIN
desactivadas y permisos privados. El modo normal no cambia de comportamiento.

Pasan **129 comprobaciones**, incluidas 17 nuevas en
`tests/pkcs11-session-state.c`. Usan la función real del framework con
consulta física simulada y comprueban: comportamiento predeterminado,
ausencia de consultas repetidas tras login al activar el experimento,
estado físico UNKNOWN, imposibilidad de crear login y exclusión de los
otros perfiles/PIN/usuarios. No utilizan PIN ni lector. La compilación
completa sin PC/SC pasó; los informes son
`build/opensc-offline/chipdoc-session-state-tests.log` y
`chipdoc-session-state-build.log`. El helper también pasó una prueba con
aplicación simulada en sus tres modos: entorno aislado, misma configuración,
permisos, marca del experimento y propagación del código de salida.

Pendiente: guardar commits, compilar con
`make test/iasecc JOBS="$(nproc)"`, cerrar instancias anteriores del firmador
y hacer **un intento** con `make firmador/jcop3-session-state`. Cerrar y
revisar el nuevo registro antes de repetir login. Si funciona, verificar
el PDF y diseñar la corrección definitiva con esa evidencia; si falla,
la hipótesis no se confirma y habrá que revisar las otras diferencias de
contexto. No activar este experimento por defecto ni reclamar una solución
validada antes de esa comparación. No repetir la firma técnica previa.

## El experimento no resuelve RSA: 2026-10-11, 21:39

`local/jcop3-firmador-api-HQ6utMiv/report.txt` registra 03:39:50 UTC del
12 de octubre, OpenSC `ca8f65684`, sin cambios locales, y
`session_state_experiment=yes`. El repositorio principal está en `78189a3`;
ambos árboles estaban limpios al revisar la captura.

El registro contiene un login exitoso y una operación de firma fallida.
Tres eventos `SESSION_LOGIN_STATE` confirman que se omitieron las consultas
físicas de `C_GetSessionInfo`; la sesión informa RW USER. Se selecciona
llave 3, RSA 2048, mecanismo SHA256-RSA-PKCS. Selección, DOCP y MSE pasan,
pero RSA AT vuelve a devolver **`6982`**, traducido a `-1211` y después
`CKR_USER_NOT_LOGGED_IN`. La revalidación queda rechazada por caché PIN
desactivada y no repite RSA. No hay logout ni cierre intermedio.

El experimento se ejecutó, pero **no confirmó que omitir las consultas de
`C_GetSessionInfo` resuelva la condición de seguridad**. No volver a
recomendarlo como corrección ni repetirlo con la misma compilación.
Eso no descarta todas las consultas de PIN: en `framework-pkcs15.c`,
`C_GetTokenInfo` llama directamente a `sc_pkcs15_get_pin_info`. El
experimento no afecta esa llamada y el registro anterior no la observaba.
Tampoco mostraba selecciones individuales ni resets detectados al obtener
el bloqueo del lector. El código del driver realiza SELECT incluso al
volver a un mismo AID; aún no hay evidencia que justifique omitirlo.

Se amplió únicamente el diagnóstico optativo, sin modificar la secuencia
de comandos, autenticación, reintentos o resultados:

| Evento | `rv` | `a` | `b` |
| --- | --- | --- | --- |
| `C_GetTokenInfo_BEGIN` | 0, marcador tras obtener mutex | slot | 0 |
| `TOKEN_PIN_STATUS` | resultado OpenSC de la consulta existente | estado de PIN almacenado | usuario software del slot |
| `C_GetTokenInfo` | resultado PKCS#11 | slot | 0 |
| `CARD_SELECT` | resultado OpenSC de transporte o comprobación SW | categoría pública fija del destino | SW, 0 si falla transporte |
| `CARD_PIN_INFO_BEGIN` | 0, marcador de la consulta existente | referencia pública de PIN | 0 |
| `CARD_PIN_INFO` | resultado OpenSC de la etapa | estado numérico observado | 1 estado, 2 política |
| `CARD_LOCK` | resultado OpenSC del bloqueo | cantidad de reset/reattachment detectados | bloqueo del lector obtenido, booleano |
| `CARD_RESET` | resultado de la llamada existente de reset | cold reset solicitado, booleano | 0 |

Las categorías SELECT son **0** otro archivo, **1** MF, **2** AID raíz
IAS ECC, **3** AID ChipDoc y **4** otro AID. No se escriben rutas, AIDs,
identificadores de archivos ni buffers. Cada evento representa una
transmisión SELECT existente; no afirma que el parsing posterior de FCP
haya pasado. En las consultas de PIN, interpretar el estado solo si
`rv=0`; un error puede dejar un estado anterior almacenado.
`C_GetTokenInfo` conserva su comportamiento previo de no propagar el
error de la consulta PIN: ambos resultados se registran por separado.
`CARD_LOCK` observa lo que recibe OpenSC al bloquear el lector; no prueba
que no hayan ocurrido cambios de contexto por otros procesos. `CARD_RESET`
no provoca ningún reset nuevo. Todo el registro sigue apagado por defecto,
con nombres fijos y enteros, preservando errno y sin PIN, longitud del PIN,
atributos, certificados, documentos, digest, firma ni datos APDU.

La siguiente prueba de hardware utiliza el modo diagnóstico ordinario,
**`make firmador/jcop3-diagnose`**, después de compilar los cambios con
`make test/iasecc JOBS="$(nproc)"`. Cerrar otras instancias, intentar una
sola firma y cerrar. Analizar el registro completo antes de otra prueba;
no activar caché PIN ni cambiar parámetros persistentes para eludir `6982`.
La firma PDF sigue pendiente, mientras las dos firmas técnicas verificadas
continúan siendo evidencia de la ruta RSA SHA-256 básica.

Pasan **136 comprobaciones** sin tarjeta: las dos nuevas del framework
comprueban que token-info conserva su consulta física al activar el
experimento y que el resultado PKCS#11 se mantiene aunque la consulta
falle. Cinco pruebas nuevas en `tests/iasecc-context-trace.c` usan SELECT
real con transporte simulado y el bloqueo real de la biblioteca compilada:
registro apagado por defecto, selección exitosa, rechazo de seguridad,
fallo de transporte y reset detectado durante bloqueo. Comprueban los
contadores para detectar comandos/reintentos añadidos y la conservación
de resultados. El reset y los rechazos son simulados; no son nuevas
observaciones de hardware. La compilación completa sin PC/SC pasó.
Informes: `build/opensc-offline/chipdoc-context-trace-tests.log` y
`chipdoc-context-trace-build.log`. Falta ejecutar este registro ampliado
en el lector del usuario; no se ha corregido aún el rechazo RSA del firmador.

## Consulta del token después del login: 2026-10-11, 21:54

`local/jcop3-firmador-api-D83Fsxrk/report.txt` registra 03:54:31 UTC del
12 de octubre, OpenSC `a3b001ea2`, sin cambios locales y experimento apagado.
El repositorio principal está en `e99218b`; ambos árboles estaban limpios
al revisar la captura. El registro ampliado contiene 211 líneas:

- Un `C_Login CKR_OK` y una firma fallida con llave 3, RSA 2048,
  mecanismo SHA256-RSA-PKCS. No hay logout ni cierre de sesión intermedio.
- **Una consulta `C_GetTokenInfo` inmediatamente después del login**,
  que ejecuta SELECT raíz IAS, consulta estado y política del PIN, y
  vuelve a seleccionar la raíz. `TOKEN_PIN_STATUS` informa PIN verificado.
- Cuatro `C_GetSessionInfo` posteriores, con secuencia equivalente,
  PIN verificado y sesión RW USER. En total son **15 SELECT raíz IAS**
  después del login y antes del SELECT ChipDoc de la firma.
- No se observan selecciones de archivo provocadas por enumeración o lectura
  de atributos tras login. Esto concuerda con los certificados
  ya cargados en los objetos del framework; no confundirlo con caché PIN.
- Se observan seis bloqueos del lector después del login, todos sin resultado
  reset/reattachment; no hay evento `CARD_RESET`. Esto no excluye cambios de
  contexto externos que OpenSC no detecte.
- Selección ChipDoc, DOCP y MSE pasan; RSA AT devuelve `6982`. La caché PIN
  desactivada bloquea revalidación y no se repite RSA.

El registro confirma el punto ciego del experimento anterior: omitía las
consultas de sesión, **pero conservaba la consulta física del token**. No
prueba que esa consulta cause el rechazo RSA, ni explica por qué el PIN
aparece verificado en la raíz. La hipótesis de efectos de consultas/selecciones
posteriores al login sigue requiriendo una comparación controlada.

Se amplió el mismo experimento optativo, `OPENSC_JCOP3_SESSION_STATE=yes`,
para omitir también la consulta de `C_GetTokenInfo` después de login CKU_USER,
con exactamente las mismas restricciones de perfil, CHV global 1 y ruta MF.
Antes del login y en otros perfiles conserva la consulta real. El método
sigue comprobando presencia/token y calculando banderas de intentos usando
los metadatos públicos disponibles; **esas banderas pueden estar
desactualizadas** en este experimento. No modifica el estado físico de la
tarjeta ni presenta los datos anteriores como una nueva observación.

El evento **`TOKEN_PIN_CACHED session=0 rv=0 a=0 b=1`** marca la omisión;
`a=SC_PIN_STATE_UNKNOWN` evita afirmar que se leyó el estado físico, y
`b=CKU_USER` identifica el login software existente. La consulta omitida no
produce `TOKEN_PIN_STATUS` ni SELECT/GET INFO adicionales. Las omisiones
previas de sesión siguen marcadas con `SESSION_LOGIN_STATE`. La configuración
mantiene caché de PIN desactivada: almacenar metadatos de intentos no
almacena el valor del PIN. Login, logout, detección de presencia y rechazo
RSA siguen sus caminos existentes; no hay nuevos VERIFY ni reintentos.

El helper añade `pin_query_experiment=session_and_token_info` al informe
para distinguir esta versión del experimento anterior; los modos normal y
diagnóstico conservan consultas físicas y eliminan la variable experimental
heredada. La versión ampliada sigue desactivada por defecto y no es todavía
una corrección validada. La siguiente intervención es compilar cambios y
hacer **un solo intento** con `make firmador/jcop3-session-state`, cerrar
firmador y revisar el registro antes de repetir PIN. Las capturas anteriores
y las firmas técnicas ya se revisaron; no pedir repetirlas primero.

Pasan **151 comprobaciones** sin tarjeta. Las 15 nuevas comprueban omisión
optativa de token-info sin inventar login físico, conservación de consulta
antes de login, y consulta real en los 13 perfiles/condiciones excluidos.
El comportamiento normal y los errores de consulta permanecen cubiertos.
La compilación completa sin PC/SC pasó; informes
`build/opensc-offline/chipdoc-token-state-tests.log` y
`chipdoc-token-state-build.log`. El helper pasó sintaxis y una ejecución
simulada en los tres modos: variables heredadas eliminadas, mismo módulo y
configuración, caché PIN desactivada, permisos privados, marca específica
solo en experimento y propagación del código de salida. No se abrió Java,
no se usó lector y no se pidió PIN en esas verificaciones.

## Primera operación exitosa del firmador: 2026-10-11, 23:34

`local/jcop3-firmador-api-WV8KnIjI/report.txt` registra 05:34:00 UTC del
12 de octubre, OpenSC `da1edb457`, sin cambios locales, y las marcas
`session_state_experiment=yes` y
`pin_query_experiment=session_and_token_info`. El repositorio principal está
en `9c4281a`; ambos árboles estaban limpios al revisar. El usuario informa
que parece funcionar. El registro de 173 líneas confirma:

- Un `C_Login CKR_OK`, seguido de `TOKEN_PIN_CACHED` y tres
  `SESSION_LOGIN_STATE`; se activó la omisión ampliada de consultas.
- Después del login no hay SELECT raíz, consultas físicas de PIN ni
  eventos de reset. La firma realiza un único SELECT ChipDoc, con `9000`.
- `CKM_SHA256_RSA_PKCS (0x40)` y **llave 3, RSA 2048**, `LlaveDeFirma`.
- `KEY_SELECT`, `CARD_KEY_DOCP`, `CARD_MSE (9000)` y `KEY_MSE` pasan.
- **`CARD_SIGN_AT rv=0, SW=9000`** y **`KEY_COMPUTE rv=0x100`**:
  la operación RSA devuelve 256 bytes.
- **`C_SignFinal CKR_OK`** con buffer de salida. No hay revalidación PIN,
  segunda operación RSA, logout ni cierre de sesión intermedios.

Es la primera operación de firma exitosa observada en el firmador con este
módulo. El logout `0x101` anterior al login no es un fallo de la firma.
El resultado valida la ruta PKCS#11 bajo el experimento y concuerda con
el mensaje del usuario; **aún no verifica el PDF guardado**, su integridad,
la cobertura de sus rangos, la confianza del certificado ni sello/revocación.
No hay PDF en `local/` al revisar. Se pidió su ruta para validación local;
no repetir login/firma o compilación como primer paso.

Comparación con las 21:54: aquel registro tenía una consulta token y cuatro
consultas sesión tras login, 15 SELECT raíz y rechazo RSA `6982`; este omite
las dos clases de consulta y obtiene `9000`. El experimento anterior que
solo omitía consultas de sesión había fallado. Esto sustenta investigar
el efecto de consultas/selecciones posteriores al login, pero no aísla
SELECT frente a VERIFY vacío o GET DATA, ni define `C9`. Tampoco valida
concurrencia, recuperación ante reset o todos los algoritmos de la tarjeta.
No promover aún el experimento a comportamiento normal: sus banderas de
intentos utilizan metadatos anteriores y su estado físico queda UNKNOWN.

El siguiente paso es verificar el PDF localmente. El `pdfsig` del sistema
(26.09.0) admite `-nocert` para omitir validación del certificado y
`-no-ocsp` para omitir OCSP en línea. Esa comprobación debe revisar estado
de firma y cobertura de rangos, conservando resultados identificables en
`local/`; no acredita confianza/revocación al omitir esas verificaciones.
Después se diseñará la corrección definitiva y sus regresiones para
consultas de estado/política. No hubo cambios de código ni nuevas pruebas
de tarjeta durante esta revisión; siguen pasando las 151 comprobaciones
registradas antes de este intento.

## PDF y sello de tiempo verificados localmente: 2026-10-11, 23:39

El usuario aportó `doc.pdf` y `doc-firmado.pdf` en la raíz. El repositorio
principal estaba en `15c183a` y OpenSC en `da1edb457`, sin nuevos cambios del
driver. Los archivos se conservan en su ubicación y se añadieron sus rutas
exactas a `.gitignore` para evitar incorporarlos al historial. La evidencia
identificable queda en `local/jcop3-pdf-verification-ydbv_z5o/`, con permisos
privados; su resumen es `report.json`, fechado 05:39:50 UTC del 12 de octubre.
No se accedió al lector ni se solicitó PIN.

Los archivos tienen 6967 y 80485 bytes y una página de tamaño carta. El PDF
firmado conserva todos los bytes originales como prefijo. El texto original
normalizado permanece en el firmado; el texto extraído completo incluye
información adicional. Ambos se renderizaron con `pdftoppm -hide-annotations`
a 72 dpi y sus imágenes PPM son **idénticas byte por byte**. Esto comprueba
la conservación de la página subyacente, excluyendo las anotaciones añadidas.

Hay dos objetos de firma:

| Objeto | SubFilter | Rangos `[offset, longitud, offset, longitud]` | Resultado |
| --- | --- | --- | --- |
| Firma de la tarjeta | `ETSI.CAdES.detached` | `[0, 7346, 33972, 4431]` | CMS verificado, certificado igual al de `LlaveDeFirma` |
| Sello del documento | `ETSI.RFC3161` | `[0, 60499, 79445, 1040]` | CMS verificado, huella SHA-512 coincide con los rangos finales |

`pdfsig -backend NSS -nocert -no-ocsp` informa **Signature is Valid** para
la primera firma. Su revisión termina en el byte 38403; no cubre las
adiciones posteriores. Para el sello informa cobertura del archivo completo,
pero **Signature has not yet been verified** y retorna 1. Ese resultado
no se tomó como prueba suficiente ni como firma principal inválida.

Se extrajo cada CMS de su hueco Contents y se validaron límites/rangos,
longitud DER y relleno nulo. La primera firma se verificó con
`openssl cms -verify -binary -noverify -inform DER`, suministrando como
contenido la concatenación exacta de sus dos rangos: **CMS Verification
successful**, retorno 0, contenido verificado idéntico. El certificado
firmante extraído coincide byte por byte con
`local/jcop3-signature-kiHbc1Qh/certificate.der`, usado en la firma técnica
verificada; así se confirma el certificado de la tarjeta correspondiente.

El sello se verificó como CMS con TSTInfo encapsulado, sin tratarlo como
firma detached del PDF. OpenSSL devolvió **CMS Verification successful**,
retorno 0. Después se leyó el messageImprint del TSTInfo autenticado: usa
**SHA-512** y coincide con el hash de la concatenación de sus dos rangos.
El segundo rango termina en el byte 80485, fin del archivo actual. Por tanto,
se verificaron la firma criptográfica del sello y su vínculo con el PDF
final, salvo el hueco de su propio Contents, esperado en el formato.

**El PDF generado por firmador usando este módulo en modo experimental
contiene una firma de la tarjeta válida y un sello de tiempo cuyo CMS y
vínculo con el documento se verificaron.** No se validaron confianza de
certificados, revocación, vigencia o una política completa de validación de
larga duración: `-noverify` y `-nocert` omiten confianza y no se consultó OCSP.
La cobertura final la aporta el sello; no atribuirla a la firma original de
la tarjeta. No afirmar todavía soporte completo de OpenSC/JCOP3.

No hubo cambios de código ni nuevas firmas. Continúan las 151 comprobaciones
previas. El siguiente trabajo es diseñar la corrección definitiva del estado
PIN/contexto sin banderas de intentos obsoletas, y comprobar recuperación
ante cambios de tarjeta y concurrencia. El experimento no se activó por
defecto durante esta revisión.

## Prueba preparada: consulta vacía aislada después de VERIFY

Se añadió `make sign/jcop3-pin-status` para distinguir una consulta vacía de
estado de las selecciones y lecturas de política que ejecuta GET INFO completo.
**Aún no se ejecutó en hardware.** Usa el texto técnico, DigestInfo SHA-256,
RSA-PKCS y verificación OpenSSL del helper existente. Antes de ejecutar el
nuevo objetivo hay que compilar los cambios; no repetir firmas anteriores.

El opt-in `OPENSC_JCOP3_POST_LOGIN_STATUS=yes` añade una sola llamada a
`iasecc_pin_get_status()` después del VERIFY exitoso y del registro existente
de verificación. Se limita a Amos, AID Generic PKI enumerado, CHV global 1 y
la política ChipDoc observada ya leída antes de autenticar. No asigna un
significado a `C9`. La estructura del comando se construye desde cero, sin
copiar credenciales ni flags de pinpad. No añade SELECT, GET DATA, lectura de
política ni reintentos. Un rechazo de VERIFY o su atajo existente por PIN
ya verificado no ejecutan el hook; un error de la consulta detiene el login.

El informe contiene `post_login_status_experiment=yes`. El archivo privado
`api.log` incluye `POST_LOGIN_STATUS_BEGIN` (`a=1`, referencia pública) y
`POST_LOGIN_STATUS` (resultado en `rv`, estado físico en `a`), además del
diagnóstico numérico existente. Nunca registra PIN, su longitud o APDU.
El helper comprueba la marca de ejecución para detectar un módulo antiguo
o un perfil excluido. Los modos normales de firma y los tres lanzadores de
firmador eliminan la nueva variable heredada; el modo experimental de sesión
no se activa en esta prueba.

Si la firma falla después de esa consulta, se tendrá un caso reducido para
investigar el estado de seguridad. Si pasa, únicamente demuestra que una
consulta inmediata, en ese contexto y transacción, permite firmar; quedan
pendientes SELECT, la rutina de política, repeticiones y cambios de transacción.
No convierte el experimento de sesión/token en corrección definitiva.
Cerrar firmador previamente, ejecutar un único intento y revisar el informe
antes de repetir PIN.

Pasan **168 comprobaciones** sin tarjeta: 17 nuevas cubren secuencia real del
driver con transporte simulado, consulta sin credenciales, exclusiones de
perfil, ausencia de comandos adicionales de política/selección después de
VERIFY y parada sin reintentos ante errores. También pasaron ocho ejecuciones
simuladas de los lanzadores, incluidos módulo sin marca y error de login,
con stdout TTY y variables heredadas adversas; no se pidió PIN. La compilación
completa sin PC/SC pasó. Informes locales:
`build/opensc-offline/chipdoc-status-probe-build.log` y
`build/opensc-offline/chipdoc-status-probe-tests.log`.

## Consulta vacía permite firmar; próxima prueba SELECT: 2026-10-12, 00:04

Los informes `local/jcop3-signature-rwfMn3zh/report.txt` y
`jcop3-signature-ELmY1jCa/report.txt`, de 06:02:54 y 06:03:14 UTC,
registran OpenSC `0e7549960`, árbol limpio y experimento de estado activado.
Ambos fallaron en la lectura previa del certificado: `C_OpenSession`
devuelve `CKR_TOKEN_NOT_PRESENT (0xe0)` para slot índice 0, ID 0. No
ejecutaron el comando con `--login --sign`, ni VERIFY con el PIN, ni el hook.
No hay certificado, firma ni `api.log` en esas carpetas. No demuestran un
fallo de consulta de estado o firma, ni explican por sí solos por qué el
token no estaba disponible. No se confirmó si fue conexión, inserción,
enumeración del slot o binding del token.

El siguiente informe, `local/jcop3-signature-QfdEBgds/report.txt`, de
06:04:31 UTC (00:04 de Costa Rica), usa el mismo commit limpio. Confirma
`RSA-PKCS`, `post_login_status_experiment=yes`, `Verified OK` y estado 0.
El `api.log` confirma:

- `POST_LOGIN_STATUS_BEGIN` y `POST_LOGIN_STATUS` son consecutivos; el
  segundo devuelve 0 y estado físico `SC_PIN_STATE_LOGGED_IN (2)`.
- `C_Login CKR_OK`, después del hook; no hay consultas adicionales de PIN
  ni SELECT raíz entre ese login y la firma.
- Un SELECT ChipDoc `9000`, llave nativa 3, RSA 2048; DOCP y MSE pasan.
- `CARD_SIGN_AT SW=9000`, `KEY_COMPUTE=256` y `C_Sign CKR_OK` con salida;
  no hay revalidación ni segunda operación RSA.

Una comprobación independiente con OpenSSL también devolvió `Verified OK`.
Se recalculó DigestInfo SHA-256, se comparó la llave pública con la extraída
del certificado y se confirmó firma de 256 bytes. El resumen privado está
en `local/jcop3-signature-QfdEBgds/verification.json`. No se validó confianza
ni revocación, ni se volvió a usar lector/PIN en esa comprobación.

**Una consulta vacía inmediata en ese contexto permite firmar.** El resultado
no descarta efectos de consultas repetidas, transacciones separadas, SELECT
o lecturas de política, y no resuelve el fallo normal del firmador. No
convertir el experimento de sesión/token en comportamiento predeterminado.

Se preparó `make sign/jcop3-select-root`: mismo texto técnico, DigestInfo,
RSA-PKCS y verificación, con `OPENSC_JCOP3_POST_LOGIN_SELECT=yes` después del
VERIFY exitoso. Conserva las restricciones de perfil observadas y añade
solo la selección del AID raíz `F0496173456363526F6F74` mediante el driver;
no añade consulta vacía ni GET DATA/política. Los eventos nuevos son
`POST_LOGIN_SELECT_BEGIN` y `POST_LOGIN_SELECT`, con destino público 2 en
`a`; entre ellos debe aparecer `CARD_SELECT a=2`. Ambos hooks comparten la
validación de perfil y son mutuamente excluyentes. Un error de selección
detiene el login, sin repetir VERIFY ni el comando de prueba.

El informe lleva `post_login_select_experiment=yes` y
`post_login_status_experiment=no`. La marca de selección exitosa es
obligatoria para detectar módulos antiguos; una marca de estado no la
sustituye. Todos los modos de lanzadores eliminan ambos opt-ins heredados,
salvo la selección explícita del experimento correspondiente.

**SELECT aún no se ha probado en hardware.** Antes de esa intervención hay
que compilar con `make test/iasecc JOBS="$(nproc)"`, guardar commits,
cerrar firmador y ejecutar `make sign/jcop3-select-root` una sola vez.
Pasan **178 comprobaciones** sin tarjeta, incluidas diez nuevas de selección:
un solo comando raíz, ausencia de consultas posteriores de estado/política,
fallos de transporte/estado, VERIFY rechazado, exclusiones y opt-ins en
conflicto. Pasaron diez ejecuciones simuladas de lanzadores, incluida la
detección de marca incorrecta o ausente y propagación de error sin reintento.
La compilación completa sin PC/SC pasó. Informes locales:
`build/opensc-offline/chipdoc-select-probe-build.log` y
`build/opensc-offline/chipdoc-select-probe-tests.log`.

## SELECT aislado reproduce el rechazo RSA: 2026-10-12, 00:13

`local/jcop3-signature-zPoqBbBZ/report.txt` y
`local/jcop3-signature-rb2JHk1X/report.txt` registran 06:13:20 y 06:13:35 UTC,
OpenSC `416c11c7b` limpio, `post_login_select_experiment=yes` y estado 1.
En ambos, el hook ejecuta **un único SELECT de raíz IAS** con `9000` después
de un VERIFY exitoso; `C_Login` también termina con `CKR_OK`. No incorpora
consultas de estado ni lecturas de política. La firma selecciona ChipDoc,
lee DOCP y prepara MSE correctamente; RSA AT devuelve `6982`, traducido a
`SC_ERROR_SECURITY_STATUS_NOT_SATISFIED` y `CKR_USER_NOT_LOGGED_IN`.
No se detectó reset durante el bloqueo del lector; esto no excluye cambios
externos de contexto. No hay firma para verificar.

Frente a la firma exitosa con consulta vacía de las 00:04, esto identifica
la **reselección raíz como operación suficiente para reproducir el fallo
en el contexto observado**. No define el significado de `C9`, no demuestra
que todo SELECT pierda autenticación y no sustituye pruebas de concurrencia.
El estado verificado del CHV por sí solo tampoco garantiza permiso RSA.

Los registros revelan una segunda operación RSA en **cada ejecución**:
`pkcs11-tool sign_data()` cambia de C_Sign a C_SignInit/Update/Final cuando
falla la firma directa. Ambas rutas reciben `6982`; el mensaje final menciona
C_SignFinal. No hubo segundo login ni VERIFY con PIN. Los eventos de
revalidación PKCS#15 corresponden al rechazo por caché de PIN desactivada.
No confundir dos ejecuciones del usuario con las cuatro operaciones RSA.

Se añadió `OPENSC_PKCS11_TOOL_NO_SIGN_FALLBACK=yes` al helper de firma y una
comprobación optativa en el CLI: tras un error C_Sign termina inmediatamente,
incluida cancelación. Conserva el comportamiento predeterminado del CLI y
su ruta multipart para entradas grandes que no intentaron C_Sign. Requiere
recompilar. El informe del helper nuevo lleva `sign_fallback=disabled`.
Cinco pruebas con módulo PKCS#11 falso verifican el fallback normal, parada
sin multipart ante error de autenticación/dispositivo/cancelación y éxito.
No usan PIN, lector, llaves ni firmas reales.

## Candidata preparada: leer estado sin reseleccionar

El nuevo objetivo `make firmador/jcop3-pin-status-in-place` activa únicamente
`OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes`, con diagnóstico numérico y sin el
experimento de omisión de sesiones/token. En esta etapa aún no se había
probado en hardware; el resultado está en la sección siguiente.
Mantiene login real, caché del valor del PIN desactivada y depuración 0.

`sc_pkcs15_get_pin_info()` solo cambia para Amos + AID Generic PKI + CHV
global 1 + ruta MF `3F00` de tipo FILE_ID, excluyendo SO/PUK. Solicita
`SC_PIN_CMD_GET_INFO` con el nuevo flag `SC_PIN_CMD_STATUS_ONLY`, sin seleccionar
la ruta. El driver IAS/ECC exige que se haya leído la política ChipDoc
observada y un máximo público de intentos positivo. La rutina consulta el
estado ISO vacío sin SELECT ni política; conserva solo ese máximo público.
El estado físico y los intentos restantes son los de la respuesta actual.
En una respuesta autenticada sin contador, usa `-1` y no recupera un contador
anterior para pintar banderas. El máximo es un dato de política ya observado,
no una nueva lectura ni un secreto. No cachea estado físico, PIN ni su valor.

Si la política aún no se conoce, el driver rechaza el flag antes de emitir
comandos, y PKCS#15 usa la consulta completa de inicialización. El VERIFY
real conserva sus lecturas de política y selecciones antes de autenticar.
Un nuevo objeto de tarjeta empieza sin la política pública conocida; un
SELECT no borra ese dato estático. Esto no valida cambios administrativos
externos de política. Ante error de consulta/bloqueo, se invalidan el estado
físico y el contador restantes; no afirmar que eso resuelve por sí solo
el estado software de todas las sesiones PKCS#11 o la recuperación del lector.

`PIN_STATUS_IN_PLACE` registra resultado (`rv`), estado físico (`a`, UNKNOWN
si falla) y referencia pública 1 (`b`). Antes de conocer la política puede
registrar NOT_SUPPORTED y después la consulta completa. Tras un login real
debe verse resultado 0 sin selecciones raíz alrededor de las consultas. No
deben aparecer TOKEN_PIN_CACHED o SESSION_LOGIN_STATE en esta candidata.
El informe lleva `pin_status_in_place_experiment=yes`,
`session_state_experiment=no` y `pin_query_experiment=status_without_selection`.
Todos los otros lanzadores eliminan el nuevo opt-in heredado. El objetivo
usa la biblioteca existente: primero compilar con
`make test/iasecc JOBS="$(nproc)"`, guardar commits, cerrar instancias previas,
hacer una sola firma de un PDF de prueba y cerrar el programa. No repetir
los experimentos SELECT o consulta vacía como primer paso.

Pasan **210 comprobaciones** sin tarjeta. Cubren la consulta física sin
selección, contador desconocido y actualizado, pérdida de autenticación,
errores/reset simulado sin reintento, inicialización, exclusiones de perfil
y fallback del CLI. El fixture aborta si se intenta cachear contenido de PIN.
El lanzador pasó cuatro modos simulados con variables heredadas adversas,
configuración sin caché de PIN, permisos privados y salida propagada.
La compilación completa sin PC/SC pasó; informes
`build/opensc-offline/chipdoc-in-place-build.log` y
`build/opensc-offline/chipdoc-in-place-tests.log`.
En esta etapa faltaban consultas repetidas y firma en hardware. Recuperación
real, concurrencia y estado después de usar la llave siguen pendientes.

## Candidata con consultas físicas: firma PDF verificada, 2026-10-12, 00:37

`local/jcop3-firmador-api-MWUUuIrV/report.txt` registra 06:37:01 UTC,
OpenSC `23b3d25fa` limpio, `pin_status_in_place_experiment=yes`,
`session_state_experiment=no` y `pin_query_experiment=status_without_selection`.
El repositorio principal está en `a275709`, también limpio al iniciar la
revisión. El usuario ya compiló y ejecutó la candidata; no pedir repetir
compilación o la primera firma sin un motivo nuevo.

La captura contiene 192 eventos. Antes del login, la primera consulta sin
política conocida recibe NOT_SUPPORTED y usa la inicialización completa;
las siguientes consultas físicas devuelven NOT_LOGGED_IN sin reselección.
Un `C_Logout 0x101` anterior al login no es un fallo de la firma. Después
del único `C_Login CKR_OK`:

- Una consulta de token y tres de sesión ejecutan `PIN_STATUS_IN_PLACE`,
  todas con retorno 0, estado físico LOGGED_IN y referencia 1. Las sesiones
  informan RW USER. Son lecturas reales, no estado físico supuesto.
- No hay SELECT raíz ni eventos TOKEN_PIN_CACHED/SESSION_LOGIN_STATE.
  No se detectó reset durante los bloqueos registrados; esto no excluye
  cambios externos que el diagnóstico no detecte.
- La firma usa SHA256-RSA-PKCS, llave nativa 3 y RSA 2048. El único SELECT
  ChipDoc devuelve `9000`; DOCP, MSE y RSA AT también pasan.
- `KEY_COMPUTE` devuelve 256 bytes y `C_SignFinal CKR_OK` entrega la salida.
  Hay una sola operación RSA, sin revalidación ni reintento.

El resumen numérico privado está en
`local/jcop3-firmador-api-MWUUuIrV/analysis.json`. Frente al diagnóstico normal
fallido, que reseleccionaba la raíz después del login, esta candidata mantiene
consultas físicas repetidas y permite firmar sin esa reselección. Junto a
los probes aislados, sustenta la corrección del camino de consulta para el
perfil observado; no atribuye un significado a `C9` ni demuestra soporte
general de JCOP3.

El usuario actualizó `doc-firmado.pdf` en la raíz: ahora tiene **80486 bytes**
y una huella distinta del archivo anterior de 80485 bytes. Se verificó este
nuevo archivo, no el PDF del experimento de omisión. La evidencia, incluidas
copias privadas de original y firmado, quedó en
`local/jcop3-pdf-verification-a3zhmd8r/`, con resumen `report.json` fechado
06:43:03 UTC. No se usó tarjeta, PIN ni red para esta verificación.

| Objeto | Rangos `[offset, longitud, offset, longitud]` | Verificación |
| --- | --- | --- |
| `ETSI.CAdES.detached` | `[0, 7346, 33972, 4432]` | CMS válido; certificado idéntico al de `LlaveDeFirma` |
| `ETSI.RFC3161` | `[0, 60500, 79446, 1040]` | CMS válido; messageImprint SHA-512 coincide con los rangos finales |

Se comprobó que cada hueco corresponde al Contents del mismo objeto de firma,
los rangos están dentro del archivo y el relleno después del DER es nulo.
OpenSSL verificó ambos CMS con `-noverify`. La firma principal cubre su
revisión de 38404 bytes; el sello vincula el PDF final de 80486 bytes salvo
su propio Contents. `pdfsig` confirma la firma principal válida, pero no
verifica el sello (retorno 1); la verificación CMS y del imprint con OpenSSL
resuelve esa comprobación criptográfica. No se validaron confianza,
revocación, vigencia ni una política de larga duración.

El PDF conserva el original de 6967 bytes como prefijo, su texto y el
renderizado de la página sin anotaciones, idéntico byte por byte. Las copias
privadas permiten conservar este resultado aunque se reemplace el PDF de la
raíz en una prueba posterior.

**La candidata ya produjo un PDF con firma y sello verificados manteniendo
consultas físicas después del login.** Sigue siendo optativa: el registro
termina en C_SignFinal y no contiene consultas posteriores a usar la llave.
La siguiente intervención debe cubrir dos firmas en la misma instancia,
guardadas con nombres distintos, y observar si hay otro login. No repetir
una operación fallida. Después faltan recuperación tras retirada/reinserción,
estado de sesiones, concurrencia y contexto cambiado por otro cliente antes
de decidir su activación por defecto. No hubo cambios de código ni nuevas
pruebas de tarjeta por parte del agente; siguen las 210 regresiones previas.

## Segundo login en la misma instancia: rechazo RSA, 2026-10-12, 00:48

El usuario informa tres aperturas de la candidata: una firma seguida de
cierre, luego una firma sin cerrar y un intento sobre otro documento en la
misma instancia. Volvió a pedir PIN y la segunda firma consecutiva recibió
CKR_USER_NOT_LOGGED_IN y los avisos derivados de `signatureValue cannot be null`.
Se revisaron los dos registros nuevos, ambos con OpenSC `23b3d25fa` limpio y
el mismo opt-in de consulta física sin selección:

- `local/jcop3-firmador-api-UpCXoCn0/`, 06:47:51 UTC: un login, cuatro consultas
  físicas autenticadas antes de la firma y una operación RSA exitosa de
  256 bytes. El registro termina en C_SignFinal.
- `local/jcop3-firmador-api-LB7oJvqS/`, 06:48:43 UTC: dos logins exitosos y
  dos operaciones RSA. La primera devuelve `9000` y 256 bytes. La segunda
  devuelve `6982`, error OpenSC -1211 y C_SignFinal `0x101`.

El archivo `doc-firmado.pdf` de la raíz también se volvió a guardar y tiene
una nueva huella, aunque su tamaño de 80485 bytes coincide con otro resultado
anterior. Se verificó esta versión y se archivó una copia privada en
`local/jcop3-pdf-verification-ueuv4dtd/`: CAdES, certificado de LlaveDeFirma,
CMS RFC 3161 y su imprint pasan; el original y la página se conservan.
No identificar versiones de PDF solo por su nombre o tamaño.

En el registro consecutivo, después de la primera firma hay una consulta
token con estado físico LOGGED_IN. Después `C_Logout CKR_OK`, seguido de
otra consulta token que sigue indicando LOGGED_IN, mientras el usuario
software ya está deslogueado. Esto concuerda con `SW_PIN_LOGOUT_ONLY=1` en
`framework-pkcs15.c`: logout PKCS#11 limpia su caché PKCS#15, pero no llama
al logout físico del driver. No modificar este comportamiento global como
solución para una sola tarjeta.

El segundo login tiene **un SELECT raíz** inmediatamente antes de devolver
CKR_OK. A continuación, una consulta token y tres de sesión siguen leyendo
LOGGED_IN sin reseleccionar. ChipDoc, DOCP y MSE pasan; RSA AT recibe `6982`.
No hay reset detectado, omisión de consultas ni segundo intento RSA después
del error. El evento KEY_REVALIDATE refleja el rechazo por caché PKCS#15
desactivada, sin VERIFY automático. Los resúmenes numéricos privados están
en `analysis.json` de cada carpeta. El diagnóstico anterior no registraba
VERIFY real ni la rama de su atajo, por lo que por sí solo no prueba si el
segundo PIN llegó a la tarjeta o si era correcto.

La revisión del código identifica un atajo adicional en
`card-iasecc.c:iasecc_pin_verify()`: consulta vacía con CHV autenticado más
hash coincidente en `checked_pins` devuelve éxito **antes de leer la política
y ejecutar iasecc_chv_verify()**. Ese registro SHA-1 es propio del driver,
distinto de la caché PKCS#15 desactivada en la configuración. El primer login
lo conserva y el logout software no lo borra. El nuevo login selecciona raíz
en `sc_pkcs15_verify_pin_with_session_pin()` antes de entrar al driver. Los
probes anteriores ya mostraron que la reselección puede dejar el CHV
autenticado y, aun así, impedir RSA. La secuencia y la ausencia de las
selecciones de política del primer login son consistentes con ese atajo;
se reproduce en transporte simulado, sin inferir el significado de C9.

El primer PDF consecutivo, `local/jcop3-consecutive-1.pdf`, tiene 80465 bytes.
OpenSSL verificó CAdES, certificado idéntico a LlaveDeFirma, CMS RFC 3161 y
su imprint SHA-512. La revisión firmada termina en 38403 bytes y el sello
se vincula al archivo final. Se conservan original, texto y renderizado sin
anotaciones. La evidencia y copia privada están en
`local/jcop3-pdf-verification-h_m47tdj/report.json`. No se validaron confianza
ni revocación. No hay un segundo PDF consecutivo guardado que verificar.

### Candidata ampliada: nuevo login con VERIFY real

El mismo `OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes` ahora también desactiva la
consulta y escritura del atajo SHA-1 para VERIFY explícito, limitado a Amos,
AID Generic PKI completo y CHV global 1. La rama conserva consulta de estado,
lectura/validación de política y límites, y ejecuta **un VERIFY** con la
entrada del usuario. Elimina un registro heredado de ese PIN antes de la
operación y no lo vuelve a almacenar en la candidata. No copia ni guarda
el PIN, no genera un reintento, no fuerza un logout físico y no cambia
el comportamiento predeterminado de otros perfiles o modos.

Los nuevos eventos numéricos son `CARD_PIN_VERIFY` para el resultado del
VERIFY a través del driver ISO, y `CARD_PIN_VERIFY_CACHED` para el retorno
anticipado por hash del camino anterior; `a` es la referencia pública y `b=0`.
No incluyen valor, longitud o hash del PIN, ni buffers APDU. En la candidata
actualizada se esperan dos CARD_PIN_VERIFY exitosos, uno por login, y ningún
CARD_PIN_VERIFY_CACHED. El informe nuevo del lanzador añade
`pin_verify_experiment=fresh_verify`; esa marca no sustituye comprobar que
la biblioteca se recompiló y emitió los nuevos eventos.

Pasan **228 comprobaciones** sin tarjeta: 18 nuevas reproducen el atajo
con CHV autenticado y permiso RSA perdido tras SELECT, prueban VERIFY real
inicial y repetido, eliminación de hash sintético heredado, rechazo del
VERIFY sin reintento, errores de estado/política antes de enviar el PIN,
consulta vacía y exclusiones de perfil/opt-in. El fixture usa únicamente
un PIN sintético y no se comunica con lectores. La compilación completa
sin PC/SC pasó, al igual que la sintaxis del lanzador. Informes:
`build/opensc-offline/chipdoc-fresh-verify-build.log` y
`build/opensc-offline/chipdoc-fresh-verify-tests.log`.

En esta etapa la ampliación aún no se había probado en hardware; el resultado
está en la sección siguiente. La intervención preparada consistía en compilar con
`make test/iasecc JOBS="$(nproc)"`, guardar los commits del submódulo y después
del principal, y repetir únicamente la prueba de dos documentos en una
misma instancia de `make firmador/jcop3-pin-status-in-place`, guardando nuevos
nombres privados. Detenerse ante el primer error, sin repetir la operación
fallida. Leer los eventos de VERIFY y ambas firmas antes de otra intervención.
La candidata sigue desactivada por defecto; recuperación y concurrencia
siguen pendientes.

## Dos firmas consecutivas verificadas: 2026-10-12, 01:07

`local/jcop3-firmador-api-fgRfkdCF/report.txt` registra 07:07:46 UTC,
OpenSC `381c469b2` limpio, `pin_status_in_place_experiment=yes`,
`session_state_experiment=no`, `pin_query_experiment=status_without_selection`
y `pin_verify_experiment=fresh_verify`. El repositorio principal está en
`2ab0042`, también limpio al iniciar esta revisión. El usuario compiló y
ejecutó dos firmas del original en la misma instancia; confirmó que pidió
PIN dos veces y ambas parecían funcionar. El registro y la verificación
independiente confirman ese resultado.

La captura contiene 299 eventos:

- Dos `CARD_PIN_VERIFY rv=0`, referencia 1, inmediatamente antes de los
  dos `C_Login CKR_OK`. No hay `CARD_PIN_VERIFY_CACHED`.
- Cada login tiene después una consulta token y tres de sesión, todas con
  estado físico LOGGED_IN. No hay SELECT raíz entre cada login y su firma.
- La primera firma termina en C_SignFinal exitoso; después hay una consulta
  token, `C_Logout CKR_OK` y otra consulta token. El CHV físico sigue
  autenticado tras el logout software, como en la captura fallida anterior.
- El segundo login vuelve a seleccionar raíz y leer política antes del
  **VERIFY real**. Esta vez se observan tres SELECT raíz previos, no solo
  el SELECT anterior al atajo del módulo viejo.
- Ambas firmas usan llave 3, RSA 2048 y SHA256-RSA-PKCS. SELECT ChipDoc,
  DOCP, MSE y RSA AT pasan; cada KEY_COMPUTE devuelve 256 bytes y cada
  C_SignFinal entrega la firma con CKR_OK. No hay revalidación ni reintento.
- No hay omisión de consultas ni reset detectado. El registro termina en
  la segunda firma, sin otra consulta posterior a ella.

Resumen privado: `local/jcop3-firmador-api-fgRfkdCF/analysis.json`.
**Pedir el PIN dos veces es esperado en esta secuencia del firmador**:
ejecuta logout entre documentos y un nuevo login para el segundo. La
candidata usa la entrada actual del usuario para VERIFY; no introduce un
segundo diálogo ni repite una autenticación fallida. No atribuir esa secuencia
a todos los firmadores ni a una regla de una sola firma por PIN de la tarjeta.

Se verificaron ambos PDF sin tarjeta, PIN ni red, guardando copias privadas:

| Archivo | Informe privado | Resultado |
| --- | --- | --- |
| `local/jcop3-fresh-verify-1.pdf` | `local/jcop3-pdf-verification-w2t9vw2q/report.json` | CAdES y sello RFC 3161 verificados |
| `local/jcop3-fresh-verify-2.pdf` | `local/jcop3-pdf-verification-_5f871pd/report.json` | CAdES y sello RFC 3161 verificados |

Ambos tienen 80485 bytes; cada uno contiene una firma CAdES de la tarjeta
y un sello de documento. La revisión CAdES termina en 38403 bytes, con
ByteRange `[0, 7346, 33972, 4431]`. El sello tiene rangos
`[0, 60499, 79445, 1040]` y se vincula al PDF final salvo su propio Contents.
En ambos, OpenSSL verificó los CMS, el certificado firmante coincide con
LlaveDeFirma y el imprint SHA-512 coincide con los rangos finales. Cada PDF
conserva todos los bytes originales como prefijo, el texto y el renderizado
de la página sin anotaciones. No se validaron confianza, revocación ni
vigencia; el resultado no afirma validación completa de larga duración.

**La candidata mantiene consultas físicas y ya pasó dos firmas consecutivas
tras dos logins reales, con ambos PDF y sellos verificados.** Esto respalda
desactivar el atajo del driver en este perfil, junto con las consultas sin
reselección. Sigue optativa; no afirmar soporte completo de JCOP3.
No hubo cambios de código ni nuevas firmas por parte del agente; las 228
regresiones y la compilación previas siguen siendo las comprobaciones vigentes.
No pedir recompilar ni repetir esta misma prueba como primer paso.

La siguiente intervención cubrirá retirada/reinserción en la misma instancia
ya usada: abrir el objetivo existente, firmar el original como control y
guardar `local/jcop3-reinsert-before.pdf`; retirar solo la tarjeta, mantener
el lector conectado y firmador abierto, reinsertar la misma tarjeta y volver
a abrir el original para guardar `local/jcop3-reinsert-after.pdf`. Observar
si la aplicación detecta la tarjeta y pide PIN. Detenerse ante un error sin
repetir la operación fallida. El registro permitirá comprobar recuperación,
nuevo VERIFY y firma tras el cambio de estado; después seguirán concurrencia
y contexto cambiado por otro cliente. No activar la candidata por defecto
antes de validar estos límites.

## Retirada/reinserción: error antes de VERIFY, 2026-10-12, 01:15

`local/jcop3-firmador-api-9ujB6ts6/report.txt` registra 07:15:42 UTC,
OpenSC `381c469b2` limpio y candidata de consulta física/VERIFY real activa.
El principal está en `ecc6f22`, también limpio al comenzar la revisión.
El usuario comunica NullPointerException de Java: no puede llamar
`sun.security.pkcs11.KeyCache.clear()` porque `this.secretCache` es null.
También informa que firmador admite `-dargs entrada.pdf salida.pdf` y PIN
desde stdin mediante `pass personal/firma-digital/pin | firmador ...`.
Luego confirma que desea conservar la GUI para probar reinserción.

Los 238 eventos muestran una primera firma exitosa con un VERIFY real,
C_Login OK, llave 3, RSA AT `9000`, 256 bytes y C_SignFinal OK. El PDF
`local/jcop3-reinsert-before.pdf` de 80465 bytes tiene CAdES, certificado de
LlaveDeFirma, CMS RFC 3161 e imprint SHA-512 verificados con OpenSSL. Conserva
original y página sin anotaciones. Copia privada e informe:
`local/jcop3-pdf-verification-gp6kyfuv/report.json`. No se validaron confianza
ni revocación. No hay PDF posterior a reinserción guardado.

Después de esa primera firma:

- Cinco CARD_LOCK devuelven **SC_ERROR_CARD_REMOVED (-1105)**. Cuatro pertenecen
  a consultas token y uno al segundo login; no son rechazos del PIN ni RSA.
- Las consultas invalidan las observaciones físicas, pero C_GetTokenInfo
  devuelve CKR_OK porque el framework no propaga ese error de lectura del PIN.
  Esto es comportamiento del camino existente, no una corrección de recuperación.
- Un logout software pasa. El segundo C_Login devuelve **CKR_DEVICE_REMOVED
  (0x32)** desde sc_lock, antes de seleccionar ruta o ejecutar VERIFY real.
- Una nueva C_OpenSession devuelve OK, pero C_GetSessionInfo termina con
  **CKR_SESSION_HANDLE_INVALID (0xB3)**. No hay segundo VERIFY, operación RSA,
  revalidación ni firma. No se registra CARD_RESET.

Resumen privado: `local/jcop3-firmador-api-9ujB6ts6/analysis.json`.
La captura no contiene C_GetSlotInfo, detección de presencia o invalidación
del token; no permite reconstruir exactamente cuándo ocurrió el cambio de
tarjeta o por qué se invalidó la última sesión. No afirmar todavía que toda
reinserción falla, ni que sea un PIN incorrecto o un problema de Internet.

Se revisaron **sin ejecutar la aplicación** el lanzador y el bytecode del
firmador 2.0.0 instalado, y el código fuente local del OpenJDK
21.0.12.1+1 fijado por su wrapper. En `Token.java`, el constructor obtiene
una sesión antes de asignar `secretCache`, `privateCache` y `sessionManager`.
`Session.id()` llama a `Token.isPresent()`, que consulta slot y sesión; ante
error puede llamar a `destroy()`. Esta rutina ejecuta `secretCache.clear()`
sin comprobar inicialización. Es una explicación consistente con la excepción
y la sesión inválida durante inicialización; falta un stack trace completo
para confirmar la cadena exacta de esa ejecución. No se confirmó una versión
Java que lo resuelva ni se cambió Java o firmador. La consulta de fuentes
externas no aportó un reporte exacto; la evidencia usada es el código local.

### Diagnóstico GUI preparado y modo CLI adicional

Se ampliaron exclusivamente los metadatos optativos, sin nuevos comandos
a la tarjeta ni cambios de resultados/recuperación:

| Evento | Campos públicos |
| --- | --- |
| C_GetSlotInfo_BEGIN | a = slot ID, b = 0 |
| C_GetSlotInfo | rv = resultado PKCS#11, a = slot ID, b = flags solo si pasa |
| CARD_PRESENCE | rv = resultado firmado de sc_detect_card_presence, a/b = 0 |
| SLOT_TOKEN_REMOVED | a = slot ID, b = si antes estaba presente; inicio de limpieza de sesiones/objetos |

El diagnóstico conserva PIN y APDU fuera de los informes, depuración 0,
consultas físicas y VERIFY real. El próximo caso sigue siendo **la GUI**:
compilar, guardar commits y abrir `make firmador/jcop3-pin-status-in-place`.
Firmar el original hacia `local/jcop3-hotplug-trace-before.pdf`; retirar
solo la tarjeta, esperar cinco segundos, reinsertar la misma, esperar otros
cinco y abrir de nuevo el original para
`local/jcop3-hotplug-trace-after.pdf`. Mantener lector conectado y GUI abierta.
Anotar si la interfaz indicó retirada/reinserción. Detenerse ante un error
sin repetir la operación y cerrar al terminar. Las esperas permiten observar
presencia fuera de la tarjeta; un pase no valida reinserción rápida o todos
los cambios de sesión. No son una corrección implementada en el driver.

Se añadió `make firmador/jcop3-cli`, con `PDF_INPUT=doc.pdf` y
`PDF_OUTPUT=local/jcop3-cli.pdf` por defecto. El wrapper activa la misma
candidata, configuración privada y diagnóstico, y ejecuta
`firmador -dargs entrada salida` preservando stdin. No invoca pass ni lee,
copia, registra o exporta el PIN; rechaza entrada ausente, salida existente
y directorio de salida inexistente antes de llamar a firmador. El informe
incluye `launch_mode=cli` o `gui`. Las rutas PDF son argumentos, el PIN no.

La revisión estática de GUISelector/GUIArgs confirma `-dargs`, lectura
sin eco mediante Console si es interactivo, y lectura de stdin por caracteres
si recibe un pipe. GUIArgs limpia su char[] y destruye la protección del PIN
al terminar. Esto no prueba todo el tratamiento de credenciales en Java;
no se registró una sesión autenticada ni se ejecutó pass desde el agente.
Su consulta pública de lectores terminó con `No smart card readers found`.
El usuario puede ejecutar explícitamente:

```bash
set -o pipefail
pass personal/firma-digital/pin | make firmador/jcop3-cli
```

Es una prueba adicional en un proceso nuevo, no sustituto del caso de GUI
con retirada/reinserción. No incorporó el almacén de pass al proyecto ni
guardó secretos en archivos. El helper técnico sign-jcop3.sh sigue exigiendo
TTY y no admite ese pipe.

Pasan **237 comprobaciones** sin tarjeta: las 228 anteriores, con cobertura
de nombres/metadatos nuevos, y nueve pruebas del lanzador con firmador/git
simulados y stdin sintético. Cubren argumentos con espacios, flujo stdin sin
incluirlo en informe/argv, propagación de salida, rechazo de archivos inválidos,
permisos privados y conservación de los cuatro modos GUI con variables
heredadas adversas. No usan el pass real ni Java, lector o PIN real.
La compilación completa sin PC/SC y sintaxis Bash pasaron; informes
`build/opensc-offline/chipdoc-hotplug-trace-build.log` y
`build/opensc-offline/chipdoc-hotplug-trace-tests.log`.
El diagnóstico ampliado y el CLI aún no se probaron en hardware.

## Retirada/reinserción con espera: pase verificado, 2026-10-12, 01:39

El usuario terminó el caso GUI propuesto e informa que parece exitoso.
`local/jcop3-firmador-api-oJtaNn1s/report.txt` registra 07:39:06 UTC,
modo GUI y candidata de estado físico sin selección/VERIFY real activa.
La captura comenzó sobre `381c469b2` con `api-trace.h`, `pkcs11-global.c`
y `slot.c` modificados. Al revisar, ambos árboles están limpios: principal
`f7b74e3` y submódulo `1736ce3a9`. Este último compromete exclusivamente
las trazas nuevas, sin cambiar comandos, PIN ni recuperación. Los informes
privados distinguen la base y estado modificado al abrir del commit posterior.

Los **430 eventos** confirman la recuperación en esta ejecución:

- VERIFY real en línea 114, C_Login OK en 115 y primera C_SignFinal OK en 208.
- CARD_PRESENCE devuelve 0 en 209 y 214: el lector observó ausencia.
  SLOT_TOKEN_REMOVED en 210 marca el comienzo de limpieza del slot 0 que
  estaba presente. Hay 12 marcas en total, también para slots vacíos y
  limpieza repetida; solo una tiene b=1. No son 12 retiradas de tarjeta.
- CARD_PRESENCE devuelve 3 en 219: PRESENT|CHANGED. Se reinicializa la tarjeta
  y se abren sesiones nuevas. Un STATUS_ONLY devuelve NOT_SUPPORTED porque
  todavía no se ha leído su política pública; la inicialización completa
  posterior pasa. Ocurrió también antes del primer login, como estaba previsto.
- Segundo VERIFY real en 336, C_Login OK en 337 y segunda C_SignFinal OK en 430.
  Ambas usan llave 3, RSA 2048, SHA256-RSA-PKCS, RSA AT `9000` y 256 bytes.
  Cada login tiene cuatro consultas físicas antes de firmar, sin SELECT raíz
  en ese intervalo. No hay CARD_PIN_VERIFY_CACHED ni omisión de consultas.
- Todos los CARD_LOCK, C_OpenSession y C_GetSessionInfo registrados pasan.
  Dos C_Logout `CKR_USER_NOT_LOGGED_IN` antes de los respectivos logins no
  impiden firmar. No aparece CKR_DEVICE_REMOVED o CKR_SESSION_HANDLE_INVALID.

Resumen privado: `local/jcop3-firmador-api-oJtaNn1s/analysis.json`.
Los eventos ordenan las operaciones, pero no tienen marcas de tiempo:
las esperas de cinco segundos corresponden al procedimiento indicado,
no a duraciones medidas por el registro. El código añadido solo diagnostica.
**El fallo anterior de las 01:15 no está explicado ni corregido por este pase.**
No extrapolar a reinserción rápida, otros lectores o cambios concurrentes.

Se verificaron ambos PDF sin lector, PIN ni red:

| PDF | Tamaño | Informe privado |
| --- | ---: | --- |
| `local/jcop3-hotplug-trace-before.pdf` | 80485 bytes | `local/jcop3-pdf-verification-z_xushy2/report.json` |
| `local/jcop3-hotplug-trace-after.pdf` | 80465 bytes | `local/jcop3-pdf-verification-sblid4m2/report.json` |

Cada firma CAdES cubre la revisión de 38403 bytes y usa el certificado exacto
de LlaveDeFirma. OpenSSL verifica la firma CMS y el CMS del sello RFC 3161;
su imprint SHA-512 coincide con los rangos del PDF y cubre la revisión final,
excluyendo su propio Contents. Se conserva el original de 6967 bytes como
prefijo, su texto y el render sin anotaciones. Se guardaron copias privadas
con permisos restrictivos. No se validaron confianza, revocación o LTV.

La siguiente intervención mantiene **la GUI y el lector conectado**, y acorta
solo la ausencia para delimitar el fallo anterior. No recompilar ni repetir
el caso de cinco segundos sin motivo nuevo. Cerrar otras instancias, abrir
`make firmador/jcop3-pin-status-in-place`, firmar el original doc.pdf hacia
`local/jcop3-hotplug-fast-before.pdf`, retirar solo la tarjeta y reinsertar
después de aproximadamente un segundo. Esperar cinco segundos tras insertar,
abrir otra vez el original y firmar hacia `local/jcop3-hotplug-fast-after.pdf`.
Anotar si la GUI muestra retirada/reinserción; detenerse al primer error,
sin reintentar, y cerrar al terminar. Revisar cambio de presencia, limpieza,
sesiones y nuevo VERIFY antes de atribuir un pase a recuperación correcta.

No se cambió código en esta revisión: siguen vigentes las 237 regresiones.
No se ejecutó pass ni una firma CLI. La candidata continúa optativa.

## Reinserción rápida: cambio ignorado y corrección local, 2026-10-12, 01:46

El usuario informa de nuevo NullPointerException al limpiar Token.secretCache.
`local/jcop3-firmador-api-y17eLwTD/report.txt` registra 07:46:21 UTC,
modo GUI, candidata activa y OpenSC `1736ce3a9` limpio. Principal `9bd4570`.
La captura tiene **260 eventos**:

- Primer VERIFY real en línea 114, login OK en 115 y firma OK en 208.
- Después de firmar, **diez CARD_PRESENCE devuelven 3 (PRESENT|CHANGED)**.
  No hay SLOT_TOKEN_REMOVED, ausencia observada, reconexión ni inicialización
  de una tarjeta nueva. C_GetSlotInfo sigue devolviendo flags `7`.
- Cuatro CARD_LOCK devuelven **SC_ERROR_CARD_REMOVED (-1105)**. Tres son
  consultas token, cuyo C_GetTokenInfo sigue retornando OK; el cuarto es
  el segundo login. C_Login devuelve **CKR_DEVICE_REMOVED (0x32)** en 247,
  antes de un segundo VERIFY. No se ejecuta una segunda operación RSA.
- Después abre otra sesión con OK; la consulta de presencia aún devuelve 3
  y C_GetSessionInfo termina en **CKR_SESSION_HANDLE_INVALID (0xB3)** en 260.
  En el código local, esta consulta invalida todas las sesiones al observar
  CARD_CHANGED. La inicialización incompleta de Token en Java sigue siendo
  consistente con el NPE; no se dispone del stack trace completo.

Resumen privado: `local/jcop3-firmador-api-y17eLwTD/analysis.json`.
Solo existe `local/jcop3-hotplug-fast-before.pdf` (80485 bytes), verificado
con OpenSSL: firma CAdES con certificado LlaveDeFirma, CMS RFC 3161 e imprint
SHA-512 correctos, original/texto/render preservados. Informe privado y copia:
`local/jcop3-pdf-verification-bjaldk_f/report.json`. No se validaron confianza,
revocación o LTV. No hay PDF posterior; el error no demuestra PIN incorrecto.

### Causa y cambio preparado

En `src/pkcs11/slot.c`, `card_detect()` primero consulta la espera de eventos.
Si hay timeout o se notifica otro lector, marca `no_change=1`. Después consulta
presencia, pero el atajo de no_change retorna OK **antes de procesar
SC_READER_CARD_CHANGED**. Así un cambio detectado por el propio lector deja
activo el token y la conexión anteriores. Esa ruta explica las diez lecturas
de cambio sin limpieza. El registro no incluye el resultado de la espera,
por lo que no distingue timeout de evento de otro lector en esta ejecución.

La corrección local:

1. Solo toma el atajo de no_change si presencia **no** indica CARD_CHANGED.
2. Cuando procesa un cambio, descarta no_change para que, tras limpiar,
   la siguiente lectura continúe por conexión y binding del framework.
3. Usa un presupuesto de reintentos local a la invocación. El presupuesto
   estático anterior se gastaba entre cambios independientes; la prueba de
   cinco reemplazos seguidos cubre que se renueve y que el bucle siga acotado.

Es una corrección general de la capa PKCS#11; afecta lectores que detecten
cambios aunque la espera de eventos no los notifique. Usa las rutas existentes
de limpieza/conexión, no modifica VERIFY o RSA ni autentica automáticamente.
Al reconectar se vuelve a inicializar el perfil; las sesiones y autenticación
anteriores no se conservan. Java, firmador y la candidata de firma optativa
no se modificaron. No se han preparado reportes upstream ni actualizado master.

### Regresión y siguiente prueba con tarjeta

`tests/pkcs11-card-change.c` ejecuta el código real de slot.c, gestión de
sesiones y limpieza, con lector y framework simulados. Las **19 comprobaciones**
cubren timeout, eventos de otro lector, evento coincidente, tarjeta sin cambio,
ausencia, error de presencia, fallo al conectar, cambio persistente con límite
y cinco reemplazos sucesivos. Comprueba sesión anterior inválida, nueva sesión
pública usable y ausencia de autenticación heredada. No usa PIN, APDU o Java.
Los procesos aislados separan el estado simulado entre escenarios.

Sobre slot.c anterior, el mismo fixture devuelve nueve FAIL y salida 1:
`build/opensc-offline/card-change-before-tests.log`. Sobre la corrección,
las 19 comprobaciones pasan. La suite completa suma **256 PASS, cero FAIL**:
`build/opensc-offline/card-change-tests.log`. Terminó la compilación sin PC/SC,
`build/opensc-offline/card-change-build.log`, y sintaxis Bash y diff checks.
Esto reproduce y corrige la decisión de control en simulación; todavía no
demuestra recuperación real después de reinserción rápida.

La próxima intervención compila mediante `make test/iasecc JOBS="$(nproc)"`,
guarda primero slot.c en el submódulo y luego tests/documentación/puntero en
el principal. Cerrar otras instancias, abrir
`make firmador/jcop3-pin-status-in-place` y firmar el original doc.pdf hacia
`local/jcop3-hotplug-fixed-before.pdf`. Mantener GUI abierta y lector conectado,
retirar solo tarjeta y reinsertar después de aproximadamente un segundo.
Esperar cinco segundos tras insertar, abrir otra vez el original y firmar
hacia `local/jcop3-hotplug-fixed-after.pdf`. Observar si la GUI muestra cambio.
Detenerse al primer error sin repetir y cerrar al terminar. Comprobar limpieza,
conexión nueva, sesiones, VERIFY y PDF antes de declarar el caso resuelto.

## Reinserción rápida corregida: pase en hardware, 2026-10-12, 02:02

El usuario confirma que funciona. Captura
`local/jcop3-firmador-api-und1k27k/report.txt`, 08:02:29 UTC, modo GUI,
candidata activa y OpenSC `8842f394b` limpio. El principal está en `ceb9c32`;
corrección y regresiones ya están comprometidas y compiladas por el usuario.

Se guarda una copia estable `api-reviewed.log` con **463 eventos** y su
resumen `analysis.json`, ambos privados. El original api.log puede crecer
mientras la GUI siga abierta; estas referencias de línea corresponden
exclusivamente a la copia revisada.

- VERIFY real en 133, login OK en 134 y primera firma OK en 227.
- CARD_PRESENCE=3 en 228 y 233: PRESENT|CHANGED. Ahora cada detección pasa
  por limpieza antes de continuar; luego la presencia vuelve a 1 y se
  reinicializa el perfil. No se observó ausencia, a diferencia del control
  con cinco segundos fuera de la tarjeta.
- Ocho SLOT_TOKEN_REMOVED son dos rondas de cuatro slots; solo la primera
  marca del slot 0 tiene b=1. Indican limpieza y slots ya vacíos, no ocho
  retiradas físicas. La captura no incluye callbacks detallados de conexión;
  la reinicialización posterior y las sesiones válidas demuestran recuperación.
- Segundo VERIFY real en 331, login OK en 332 y segunda firma OK en 425.
  Ambas usan llave nativa 3, RSA 2048, SHA256-RSA-PKCS, RSA AT `9000` y
  producen 256 bytes. Cada login tiene cuatro consultas físicas antes de
  firmar, sin SELECT raíz en ese intervalo. No hay atajo por hash.
- Todos los CARD_LOCK, C_OpenSession, C_GetSlotInfo y C_GetSessionInfo pasan.
  Los dos STATUS_ONLY NOT_SUPPORTED preceden a la inicialización pública
  de la política; los dos C_Logout USER_NOT_LOGGED_IN preceden a los logins.
  No se registran DEVICE_REMOVED, SESSION_HANDLE_INVALID ni fallo RSA.

Se verificaron ambos PDF sin tarjeta, PIN o red:

| PDF guardado por el usuario | Tamaño | Informe privado |
| --- | ---: | --- |
| `local/jcop3-hotplug-fixed-before.pdf` | 80465 bytes | `local/jcop3-pdf-verification-o7f8m3jn/report.json` |
| `jcop3-hotplug-fixed-after.pdf` (raíz) | 80485 bytes | `local/jcop3-pdf-verification-m9wzm0yz/report.json` |

OpenSSL verificó CAdES con el certificado exacto de LlaveDeFirma y el CMS
RFC 3161 con imprint SHA-512 coincidente. CAdES cubre la revisión de 38403
bytes; el sello cubre la revisión final salvo su propio Contents. Ambos
preservan el original de 6967 bytes, texto y render sin anotaciones.
Los informes conservan copias privadas; no se validaron confianza,
revocación o LTV. El PDF posterior quedó en la raíz: se conserva su ubicación
y se añade /jcop3-*.pdf a .gitignore para mantener las pruebas fuera de Git.

Este pase valida la corrección en la tarjeta y lector observados, en una
reinserción rápida siguiendo el procedimiento indicado. Las duraciones no
están medidas en los eventos. No extrapolar a todas las reinserciones,
otros lectores o concurrencia. El caso anterior fallido conserva su captura.
No hay cambios nuevos del driver: siguen vigentes las 256 regresiones.

La próxima intervención prueba el CLI ya preparado. Cerrar antes la GUI y
otras instancias, no recompilar, ejecutar una sola vez:

```bash
set -o pipefail
pass personal/firma-digital/pin | make firmador/jcop3-cli
```

Usa doc.pdf y salida privada local/jcop3-cli.pdf; si existe, el wrapper la
rechaza y se debe elegir otro PDF_OUTPUT sin sobrescribir evidencia. Detenerse
ante un error sin reintentar PIN. Revisar certificado/llave seleccionada,
login, VERIFY, firma y PDF antes de declarar el CLI probado. No lo ejecutó
el agente ni se accedió al pass real. Después quedan ciclos reales repetidos,
concurrencia y selección del perfil por defecto antes de preparar upstream.

## Primer CLI: firma y PDF verificados, 2026-10-12, 02:11

El usuario ejecutó el pipe autorizado de pass hacia `make firmador/jcop3-cli`.
`local/jcop3-firmador-api-Z0aJ18Ge/report.txt` registra 08:11:29 UTC,
launch_mode=cli, candidata activa y OpenSC `8842f394b` limpio. Principal
`feaca6f`; ambos árboles limpios antes de actualizar esta documentación.
El agente no ejecutó pass ni firmador autenticado ni accedió al PIN.

Los **174 eventos** se conservaron en api-reviewed.log, con resumen privado
analysis.json. Confirman VERIFY real en 96, login OK en 97, selección de
llave nativa **3 (LlaveDeFirma), RSA 2048**, y C_SignFinal OK en 174.
El mecanismo es SHA256-RSA-PKCS (`0x40`), RSA AT devuelve `9000` y la
operación produce 256 bytes. Dos consultas físicas después del login pasan
sin SELECT raíz. No hay CARD_PIN_VERIFY_CACHED ni errores de lock/sesión.
La consulta STATUS_ONLY inicial devuelve NOT_SUPPORTED y se lee política
pública, como en la GUI. No hubo un segundo VERIFY o reintento de firma.

`local/jcop3-cli.pdf` tiene **76918 bytes**. Informe privado y copias:
`local/jcop3-pdf-verification-fttds461/report.json`. Verificación sin tarjeta,
PIN o red:

- CAdES con el certificado exacto de LlaveDeFirma, CMS verificado por OpenSSL
  sobre los rangos del PDF. La revisión firmada tiene 34916 bytes.
- Sello RFC 3161, CMS verificado y messageImprint SHA-512 coincidente con
  los rangos del PDF; cubre la revisión final salvo su propio Contents.
- Original de 6967 bytes preservado como prefijo, texto conservado y render
  idéntico sin anotaciones. Los distintos tamaños/revisiones respecto a la
  GUI no invalidan estos resultados; no se atribuye la diferencia a una
  causa concreta ni se exige identidad byte a byte entre modos.

No se validaron confianza, revocación o LTV. Estos resultados prueban una
firma CLI con el módulo local y la misma candidata ya usada en GUI; no
prueban otras selecciones de certificado, documentos o clientes concurrentes.
El wrapper conserva stdin directo hacia firmador y no ejecuta pass ni
almacena el PIN; las regresiones previas del lanzador siguen vigentes.
El helper técnico sign-jcop3.sh sigue exigiendo TTY. La salida CLI ya existe;
el wrapper la rechazaría en otro intento salvo elegir otro PDF_OUTPUT.

No se modificó código ni se repitieron comprobaciones sin necesidad: las
**256 regresiones** y compilación previas permanecen vigentes. La candidata
sigue optativa. GUI, firmas consecutivas, reinserción rápida y CLI están
verificados para la tarjeta/lector observados.

### Próxima intervención: recuperación repetida en una sola GUI

La simulación cubre cinco reemplazos, pero la prueba real corregida solo
observó una reinserción. El siguiente caso comprobará que sesiones, política
pública y autenticación vuelvan a inicializarse durante ciclos sucesivos.
No recompilar ni repetir por separado el primer CLI.

Cerrar otras instancias, abrir `make firmador/jcop3-pin-status-in-place` y
firmar el original doc.pdf hacia `local/jcop3-cycle-0.pdf`. Mantener lector
conectado y GUI abierta; retirar solo tarjeta, reinsertar tras aproximadamente
un segundo y esperar cinco tras insertar. Firmar nuevamente el original como
`local/jcop3-cycle-1.pdf`. Repetir el ciclo hasta `local/jcop3-cycle-4.pdf`:
cinco firmas y cuatro reinserciones. Detenerse al primer error sin reintentar
PIN y cerrar al terminar. Revisar nuevo VERIFY por login, limpieza/sesiones
y cada PDF antes de declarar recuperación repetida. Después, comprobar
interferencia de otro cliente y concurrencia, preservando esta evidencia.

## Ciclos repetidos: cuatro cambios y seis firmas verificadas, 2026-10-12, 02:18

`local/jcop3-firmador-api-tYcckn70/report.txt` registra 08:18:56 UTC,
modo GUI y candidata activa, OpenSC `8842f394b` limpio. Principal `e72fa35`;
ambos árboles limpios al comenzar la revisión. El usuario terminó el caso
de firmas y reinserciones sucesivas en la misma instancia.

La copia privada api-reviewed.log y analysis.json tienen **1170 eventos**:

| Firma en orden de eventos | VERIFY | Login OK | C_SignFinal OK | Consultas físicas tras login | Limpieza de token activo desde firma anterior |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 152 | 153 | 246 | 4 | 0 |
| 2 | 292 | 293 | 386 | 4 | 0 |
| 3 | 490 | 491 | 576 | 3 | 1 |
| 4 | 680 | 681 | 774 | 4 | 1 |
| 5 | 878 | 879 | 972 | 4 | 1 |
| 6 | 1076 | 1077 | 1170 | 4 | 1 |

Los números son líneas de la copia revisada, no marcas de tiempo. Las seis
operaciones usan LlaveDeFirma, referencia 3 RSA 2048, SHA256-RSA-PKCS, RSA AT
`9000` y 256 bytes. No hay CARD_PIN_VERIFY_CACHED ni SELECT raíz entre login
y firma. Todos los locks, sesiones abiertas y consultas de sesión pasan.

Se observan **cuatro limpiezas de token activo**, en 388, 578, 776 y 974,
seguidas cada una de nueva autenticación y firma. Las 32 marcas totales de
SLOT_TOKEN_REMOVED también incluyen slots vacíos y rondas repetidas; solo
cuatro tienen b=1. CARD_PRESENCE=3 y 7 incluyen CHANGED; 7 agrega INUSE
(`SC_READER_CARD_INUSE=4`), no es un código de error. No se observó ausencia.
El driver procesó los cambios sin mantener la conexión anterior. Las primeras
dos firmas no tienen limpieza intermedia y prueban también login/VERIFY de
nuevo después de logout software. Cinco consultas iniciales STATUS_ONLY
NOT_SUPPORTED permiten cargar política tras conexión inicial y cuatro cambios.

Había **seis PDF distintos**, incluidos los cinco previstos y un extra
`local/jcop3-cycle-.pdf`. Todos se verificaron sin lector, PIN o red:

| PDF | Tamaño | Informe privado bajo local/ |
| --- | ---: | --- |
| `jcop3-cycle-0.pdf` | 80485 | `jcop3-pdf-verification-fllc7uda/report.json` |
| `jcop3-cycle-1.pdf` | 80485 | `jcop3-pdf-verification-dunhg6fo/report.json` |
| `jcop3-cycle-2.pdf` | 80465 | `jcop3-pdf-verification-2060rvi6/report.json` |
| `jcop3-cycle-3.pdf` | 80465 | `jcop3-pdf-verification-bxydwf47/report.json` |
| `jcop3-cycle-4.pdf` | 80485 | `jcop3-pdf-verification-s_76kbg8/report.json` |
| `jcop3-cycle-.pdf` | 80485 | `jcop3-pdf-verification-2d2zvgbi/report.json` |

OpenSSL verifica CAdES con el certificado exacto de LlaveDeFirma y cada CMS
RFC 3161/imprint SHA-512. Original, texto y render sin anotaciones se conservan.
Las copias privadas conservan hashes distintos; no asignar cada evento a un
nombre por su sufijo, dado que el registro no lleva nombres ni tiempos de
firma. No se validaron confianza, revocación o LTV. El driver no cambió.
Este resultado valida los ciclos observados; no otros lectores o concurrencia.

### Segundo cliente público preparado

Se añade `make inspect/jcop3-peer-context` y `scripts/peer-context-jcop3.sh`,
usando opensc-tool de la compilación existente. Abre un cliente distinto,
selecciona solo la raíz IAS observada (`F0496173456363526F6F74`) con driver
default y termina. No solicita PIN, firma, lee componentes privados ni fuerza
reset. Stdin vacío, OPENSC_DEBUG y configuración debug a 0, cachés desactivadas
y variables heredadas de diagnóstico/experimentos eliminadas. El informe y
configuración quedan privados en local/, prefijo jcop3-peer-context-.
La salida normal de opensc-tool contiene solo el SELECT público y su respuesta;
no se activa una captura APDU autenticada.

El helper exige salida exitosa y la respuesta `SW1=0x90, SW2=0x00`, ya que
opensc-tool puede devolver cero aunque el SELECT sea rechazado. Ante transporte,
SW diferente o respuesta ausente se detiene, sin reintentos. La prueba incluye
conexión, SELECT y desconexión conjuntamente; no aísla sus respectivos efectos.

Seis regresiones nuevas con herramienta/git simulados comprueban un único
SELECT exacto, stdin vacío, diagnóstico heredado desactivado, privacidad,
propagación del fallo de transporte, rechazo de SW 6982 pese a salida cero,
respuesta ausente, lector/argumentos inválidos y herramienta no compilada.
Pasan **262 comprobaciones, cero FAIL** en
`build/opensc-offline/peer-context-tests.log`; sintaxis Bash y ejecución Make
simulada pasan. No se ejecutó el helper con tarjeta desde el agente y el
caso del segundo cliente todavía está pendiente en hardware.

Próxima intervención: cerrar otras instancias y abrir el objetivo GUI existente,
firmar el original doc.pdf hacia `local/jcop3-peer-before.pdf`, dejar la GUI
abierta y en reposo con tarjeta/lector conectados. Desde otra terminal con
nix develop, ejecutar una vez `make inspect/jcop3-peer-context`. Solo si confirma
SELECT 9000, abrir otra vez el original y firmar hacia
`local/jcop3-peer-after.pdf`. No retirar la tarjeta ni ejecutar el helper
durante login/firma. Detenerse al primer error sin repetir PIN y cerrar al
terminar. Revisar ambas capturas y PDF. Esto prueba un segundo cliente entre
firmas, no dos firmas simultáneas. Después se delimitará concurrencia real y
se decidirá activación del perfil; no se ha preparado un reporte upstream.


## Nuevo bloqueo de login: CKR_ARGUMENTS_BAD, 2026-10-12, 02:35–02:36

Principal `f214746` y OpenSC `8842f394b`, limpios al comenzar la revisión.
Las capturas GUI GbaOMELO (08:35:29 UTC, 241 eventos) y j0FPyGAS
(08:36:49 UTC, 145 eventos) usan la candidata y debug 0. Se conservan copias
privadas api-reviewed.log y analysis.json en cada captura. Hay respectivamente
cuatro y dos C_Login con rv=0x7, todos precedidos por CARD_PIN_VERIFY -1300
(SC_ERROR_INVALID_ARGUMENTS), referencia CHV 1. No hay C_SignInit, C_SignFinal
ni CARD_SIGN_AT; los locks pasan. No se encontró informe peer-context:
el fallo es anterior al paso del segundo cliente, que sigue pendiente.

El usuario indicó que pegó el PIN en la GUI; no conocemos ni registramos
su valor o longitud. No hay evidencia de PIN incorrecto ni de bloqueo de
la tarjeta. El marcador CARD_PIN_VERIFY está después de iasecc_chv_verify,
que llama al driver ISO; no distingue rechazo del encoder y transporte.
En sec.c, sc_build_pin devuelve -1300 si supera max_length, y en GLP también
por ciertas entradas inválidas. PKCS#15 construye ASCII para el tipo público
observado y IAS combina max_length con el límite público 14 de la tarjeta.
Esto ofrece una hipótesis comprobable, no demuestra la causa de estas capturas.
Los SW ISO normales de PIN incorrecto/longitud/parámetros tienen otros errores;
no equiparar -1300 a un PIN incorrecto ni afirmar que no fue enviado todavía.

Se añaden dos eventos optativos al driver ISO, sin operaciones adicionales:
PIN_APDU_BUILD justo después de construir VERIFY, antes del transporte,
y PIN_TRANSMIT después de transmitir o invocar el lector PIN-pad. Solo
resultado, referencia pública y SW si el transporte tuvo éxito; nunca PIN,
longitud, hash o bytes APDU. La validación y propagación de errores siguen
iguales. Build negativo demuestra rechazo previo al envío; build exitoso
seguido de TRANSMIT separa fallo de comunicación de una respuesta de tarjeta.
Si el llamador proporciona APDU, se conserva el camino existente sin build.

Siete regresiones usan el encoder ISO real con credenciales sintéticas y
transporte simulado: trazas desactivadas, éxito con dos etapas, límite excedido
sin envío, -1300 de transporte distinguible sin reintento, respuesta 63C6 sin
reintento, GET_INFO sin marcas VERIFY y archivo de diagnóstico inaccesible.
No usan tarjeta o PIN real. La prueba del segundo cliente queda suspendida.
Próxima intervención: cerrar instancias, compilar y ejecutar un solo intento
GUI con el PIN escrito manualmente, guardar en una salida nueva; detenerse
ante fallo para revisar las etapas. No ejecutar aún el helper peer-context.

Pasan **269 comprobaciones, cero FAIL**, incluidas las siete nuevas, en
`build/opensc-offline/pin-stage-tests.log`. Compilación completa sin PC/SC
en `build/opensc-offline/pin-stage-build.log`; esto no valida hardware.


## Marcas ISO: rechazo local confirmado, 2026-10-12, 02:51

Principal `1452044`, OpenSC `c052ee925` limpio. La GUI PkFq1kEy comienza
08:51:29 UTC y registra 147 eventos. Copia privada api-reviewed.log y
analysis.json preservan los resultados. PIN_APDU_BUILD en líneas 114 y 145
devuelve -1300; CARD_PIN_VERIFY 115/146 y C_Login 116/147 repiten -1300/0x7.
**No hay PIN_TRANSMIT:** iso7816_pin_cmd retorna por error de construcción
antes de llamar al transporte. Estos dos logins no enviaron el PIN. No se
alcanzó ninguna operación de firma. Las instancias EWSyeCKN y X6HHvZ3m,
a las 08:50:50 y 08:51:03 UTC, tienen 28/31 eventos y ningún login o firma.

El usuario confirma el mismo PIN que usaba antes; no atribuir el fallo al
portapapeles o a un PIN diferente. En sc_build_pin los posibles -1300 son
límite excedido y validaciones GLP; iso7816_build_pin_apdu también rechaza
tipos de autenticación desconocidos. IAS fuerza CHV y PKCS#15 elige ASCII
para el tipo público observado, pero aún no se verificaron los parámetros
concretos de estas llamadas. No inferir longitud o valor del PIN ni modificar
los límites por esta evidencia. El rechazo local tampoco valida el PIN.

La revisión estática del jar instalado (javap, sin ejecutar login) muestra
que RequestPinWindow.showandwait entrega JPasswordField.getPassword a
PasswordProtection y limpia el campo; no agrega relleno ni normaliza el
contenido explícitamente. No demuestra qué bytes llegan al encoder C.
No se abrió el gestor de contraseñas desde el agente y no se guardó un PIN.

No se modifica el módulo. Continúan vigentes las 269 comprobaciones y la
compilación previas; no repetirlas por documentación. Próxima comparación:
cerrar instancias GUI, usar el CLI existente con el flujo pass autorizado
por stdin y guardar en local/jcop3-pin-stage-cli.pdf. Una sola ejecución,
sin recompilar ni helper peer-context; ante fallo, detenerse y revisar
build/transporte antes de otro intento. Si produce PDF, verificarlo antes
de concluir que el problema está limitado a GUI. La prueba entre clientes
permanece pendiente y las firmas anteriores conservan su validez comprobada.


## Comparación CLI exitosa y PDF verificado, 2026-10-12, 02:56

Principal `5cf474b`, OpenSC `c052ee925` limpio. DgJlCm1N empieza
08:56:49 UTC, modo CLI, candidata activa y depuración 0. El usuario ejecutó
el flujo pass por stdin; el agente no abrió pass ni firmó. Copia privada
api-reviewed.log y analysis.json: 176 eventos; PIN_APDU_BUILD 96 OK,
PIN_TRANSMIT 97 con 9000, CARD_PIN_VERIFY 98 y C_Login 99 OK. C_SignInit 165,
CARD_SIGN_AT 174 con 9000, C_SignFinal 176 OK. Llave de firma referencia 3,
RSA 2048 y 256 bytes; sin fallos de lock o atajo VERIFY_CACHED.

local/jcop3-pin-stage-cli.pdf tiene 76900 bytes. Informe privado
local/jcop3-pdf-verification-0xwdzxbh/report.json: CMS CAdES verificado con el
certificado exacto de LlaveDeFirma; revisión firmada de 34916 bytes. CMS del
sello RFC 3161 válido e imprint SHA-512 coincide con los rangos de la revisión
final. Original como prefijo, texto y render sin anotaciones conservados.
No se valida confianza, revocación o LTV. No incorporar PDF/certificados a Git.

Este resultado confirma firma con la misma compilación y tarjeta tras el
rechazo GUI. Todavía no demuestra un defecto de GUI ni descarta diferencias
de los parámetros públicos de validación. La revisión estática adicional de
CardSignInfo muestra setPin/getPin directos; tampoco agrega relleno explícito.
No se modifica el módulo, ni se repiten regresiones por documentación.

Próxima comprobación acotada: GUI existente, limpiar por completo el campo,
copiar con pass -c personal/firma-digital/pin y pegar una sola vez. La revisión
del código instalado de password-store 1.7.4 muestra que -c toma la primera
línea por defecto, no imprime el secreto y usa el portapapeles; el agente solo
leyó código, no ejecutó pass. GUIArgs.readPin del CLI toma la primera línea
hasta CR/LF. Se busca comparar la misma fuente sin pedir, almacenar, medir o
registrar el PIN. Un intento hacia local/jcop3-pin-stage-gui.pdf; detenerse
ante fallo. Si pasa, verificar PDF antes de volver a la prueba entre clientes.
No recompilar ni relajar límites, recortar o normalizar el PIN en el módulo.


### GUI volvió a funcionar con el mismo módulo, 2026-10-12, 03:00

Durante la revisión CLI, el usuario comunicó otra firma exitosa con
firmador/jcop3-pin-status-in-place. Ie57KYQq empieza 09:00:25 UTC, modo GUI,
OpenSC c052ee925 limpio. Se preservan 210 eventos en api-reviewed.log y
analysis.json: build 114 OK, transporte 115 con 9000, VERIFY 116/login 117 OK,
SignInit 199, RSA AT 208 con 9000 y SignFinal 210 OK. Es la misma compilación
que rechazó localmente la entrada anterior; no se hizo una corrección nueva.
No sabemos el método de entrada de PIN de este éxito, por lo que no afirmar
que copiar una línea o escribirlo manualmente resolvió el error previo.

El nuevo doc-firmado.pdf, de 80482 bytes, queda copiado y verificado en
local/jcop3-pdf-verification-bkt8tbsu/report.json. CAdES con el certificado
exacto de LlaveDeFirma, revisión de 38402 bytes; sello CMS RFC3161 e imprint
SHA-512 de la revisión final válidos. Original/texto/render conservados.
No confianza/revocación/LTV. El archivo fuente puede ser reemplazado por el
usuario; la copia privada y sus hashes preservan esta evidencia.

La causa del rechazo previo sigue desconocida. El módulo conserva validación
y credenciales sin transformación. No repetir la comparación GUI/pass -c que
se había propuesto: ya hay otra GUI verificada. Si Ie57KYQq sigue abierta,
usar esa firma como baseline y ejecutar el helper del segundo cliente desde
otra terminal mientras GUI está en reposo y tarjeta conectada. Solo tras SW
9000, firmar el original doc.pdf hacia local/jcop3-peer-after.pdf en la misma
instancia. Si ya se cerró, no ejecutar helper aislado; reconstruir después
el caso en una sola GUI. Detenerse ante fallo. La copia revisada tiene 210
eventos; api.log puede crecer. No sobrescribirla al revisar el nuevo segmento;
conservar otra copia para la prueba peer. No recompilar ni repetir CLI/ciclos.
