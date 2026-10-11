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
| Mecanismo de firma usado por el firmador | Pendiente |

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
- [ ] Una firma con el mecanismo observado se verifica externamente.
- [ ] Firma reproducida sin cargar Idopte.
- [ ] Retirada, reinserción, errores y varias sesiones comprobados.

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
