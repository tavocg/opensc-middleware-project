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
