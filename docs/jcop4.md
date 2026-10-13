# Pruebas de JCOP4

El 13 de octubre de 2026 el usuario informó que dispone de una **JCOP4 con
certificados RSA 2048**. OpenSSL corroboró ese tamaño en ambos certificados
públicos leídos, como se registra abajo. También requiere que el diseño permita
modelos RSA 3072 y futuras generaciones, sin duplicar un driver por modelo.
Se investiga en este repositorio y con el mismo submódulo OpenSC, conservando
las capturas y pruebas de JCOP3. Ya se obtuvo el ATR; todavía no hay evidencia
de firma con su aplicación. Los certificados y directorios PKCS#15 ya
se leyeron sin autenticación. EF.DIR ya
anunció el AID y etiqueta de su aplicación, registrados abajo.
La etiqueta JCOP4 la aporta el usuario; el ATR por sí solo no demuestra la
generación de hardware ni que la aplicación sea idéntica a la JCOP3.

## Primera consulta

Cerrar firmador y otros clientes de tarjeta antes de sustituir la JCOP3 por
la JCOP4. El perfil `lock_login` puede reservar el lector hasta cerrar la GUI.
Con la compilación existente:

```bash
nix develop
make probe/jcop4 READER=0
```

El objetivo utiliza `build/opensc/src/tools/opensc-tool` para consultar
versión, lectores y ATR. No recompila ni instala. No identifica aplicaciones
ni inicializa el driver de tarjeta: `--atr` termina antes de
`sc_connect_card()`; `--list-readers` se ejecuta sin `--verbose`.
No envía APDU de exploración, no solicita PIN ni intenta autenticar o firmar.
Cada consulta recibe stdin vacío, depuración 0 y configuración privada, sin
las banderas experimentales de JCOP3 ni trazas API heredadas.

Los resultados quedan en `local/jcop4-probe-*/report.txt`, junto con una
configuración propia y los commits/estados Git. Los directorios tienen
permisos 0700 y los archivos 0600. Ante un error se detiene, sin reintentos.
El mensaje de consulta terminada indica que las herramientas salieron con
estado 0; revisar que el informe tenga lector y ATR antes de avanzar.

## Resultado inicial: 13 de octubre, 07:19:50 UTC

Informe `local/jcop4-probe-9XynOJDU/report.txt`: lector OMNIKEY 3x21,
índice 0, tarjeta presente; las tres consultas terminaron con estado 0.
OpenSC compilado `3dfef6704`, versión anunciada 0.27.1, PC/SC habilitado.
La consulta se ejecutó antes del commit del lanzador, que ahora está guardado
en el principal como `0c883c2`.

ATR observado:

```text
3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a
```

Coincide exactamente con la entrada de contacto de `eoi_atrs[]` en
`vendor/opensc/src/libopensc/card-eoi.c`. Difiere del ATR de la JCOP3 probado
en este proyecto y no aparece en `iasecc_known_atrs[]`. Esto es una coincidencia
de identificación, sin prueba de que sean iguales la aplicación, las llaves
o la política de autenticación.

Además, `build/opensc/config.h` tiene `ENABLE_SM`, pero no
`ENABLE_OPENPACE`; el script de compilación usa `--disable-openpace`.
Con esa combinación `sc_get_eoi_driver()` devuelve NULL, por lo que eOI
no está habilitado en esta compilación. No cambiar esa dependencia ni la
detección de iasecc sin identificar primero la aplicación de esta tarjeta.

## Consulta de AID conocidos

Con firmador cerrado y manteniendo la misma JCOP4 insertada:

```bash
make inspect/jcop4-aids READER=0
```

No recompila. Comprueba primero lector y ATR; si este no coincide con el
observado, se detiene antes de enviar SELECT. Después ejecuta cuatro SELECT
por nombre, cada uno con `--card-driver default`, para evitar la detección
automática y los caminos de PIN/CAN/PACE de drivers especializados:

| Candidato | AID | Fuente |
| --- | --- | --- |
| Raíz IAS/ECC | `F0496173456363526F6F74` | Capturas de la JCOP3 |
| ChipDoc Generic | `E828BD080FD25047656E65726963` | EF.DIR de la JCOP3 |
| NXP1 | `E828BD080F014E585031` | Documentación eOI de OpenSC |
| NXP0 | `E828BD080F014E585030` | Documentación eOI de OpenSC |

Los dos últimos AID están documentados para eOI en la
[wiki oficial de OpenSC](https://github.com/OpenSC/OpenSC/wiki/Slovenian-EID).
No se trasladan a esta tarjeta sus roles, algoritmos ni requisitos de PIN.
Es una lista acotada; no se hace un barrido de identificadores.

El informe y respuestas quedan en `local/jcop4-aids-*/`, con permisos privados,
stdin vacío, depuración 0 y experimentos JCOP3 desactivados. Solo se envía
SELECT; no hay VERIFY, MSE, lectura de componentes privados, firma ni
operaciones administrativas. `9000` indica SELECT aceptado y `6A82` que el
candidato no se encontró en esa consulta. Ninguno demuestra todavía que
funcione el perfil PKCS#15 o la firma. Otras respuestas, fallos de transporte
o un estado ausente/ambiguo detienen la consulta sin reintentos. Se revisa
el SW aunque `opensc-tool` devuelva estado de proceso 0.

## Resultado SELECT: 13 de octubre, 07:28:12 UTC

Informe `local/jcop4-aids-qixa9ZwC/report.txt`, OpenSC `3dfef6704`:
lector 0 con tarjeta presente y el mismo ATR. Los cuatro SELECT devolvieron
**6A82**; ninguno se seleccionó en esta consulta. Los comandos de herramienta
salieron con estado 0, pero se revisaron los SW reales.

Esto no demuestra que la tarjeta carezca de aplicaciones PKCS#15 ni que
requiera PACE; podrían ser otros AID o depender del contexto de selección.
No ampliar la tabla ATR de iasecc, reutilizar las excepciones JCOP3 o
habilitar eOI únicamente por la coincidencia del ATR.

## Lectura pública de EF.ATR y EF.DIR

Con firmador cerrado y la misma JCOP4 insertada:

```bash
nix develop
make inspect/jcop4-files READER=0
```

Se compila únicamente `tools/card-public-info.c`, contra el OpenSC existente,
en `build/opensc/card-public-info`. No recompila OpenSC ni instala nada.
Después de la comprobación inicial del ATR, la herramienta conecta con driver
`default` y vuelve a comprobar el ATR conectado antes de consultar archivos.
Mantiene una transacción durante las dos consultas y la libera al terminar.

Usa las funciones de OpenSC `sc_parse_ef_atr()` para `3F00/2F01` y
`sc_enum_apps()` para `3F00/2F00`. Estas seleccionan y leen los archivos públicos;
no se autentica, firma, configura MSE ni se lee un archivo de CAN. Un SELECT
fallido no procede a leer ese archivo. Si EF.ATR no se encuentra, consulta
EF.DIR; otros errores de EF.ATR detienen. Un EF.DIR ausente queda registrado
como no encontrado en este contexto, sin afirmar ausencia de aplicaciones.
No hay reintentos ni cambios de driver automáticos.

El informe `local/jcop4-files-*/report.txt` registra resultados de cada operación,
AID raíz y capacidades publicadas por EF.ATR, y AID/etiquetas de EF.DIR.
Depuración 0, stdin vacío, configuración propia y permisos privados. Un valor
0 de tamaño máximo APDU significa que no se obtuvo ese dato; no prueba que
no admita APDU extendidas. Una enumeración de aplicaciones con salida de
proceso 0 tampoco demuestra que los certificados o la firma funcionen.

## Resultado EF.ATR/EF.DIR: 13 de octubre, 07:39:47 UTC

Informe `local/jcop4-files-DORk47gE/report.txt`, OpenSC `3dfef6704`.
Helper compilado, conexión/ATR/reserva correctos; ambos parsers devuelven 0,
una aplicación enumerada y transacción liberada. No hubo autenticación.

| Metadato público | Resultado |
| --- | --- |
| AID raíz en EF.ATR | `A0000000003F00` |
| Pre-issuing en EF.ATR | `0400ECC2`, sin interpretación adicional |
| Máximos APDU extraídos | 0/0: no se obtuvo ese dato |
| AID en EF.DIR | `E828BD080FA00000036364796E6964` |
| Etiqueta en EF.DIR | `ChipDoc Generic` |

Ambos AID difieren de los probados antes. El AID de aplicación tampoco
aparece en la tabla `apps[]` de `dir.c`, ni en las condiciones de las
adaptaciones JCOP3. La coincidencia de etiqueta ChipDoc no demuestra un
protocolo idéntico ni autoriza a ampliar esas excepciones.
Esto explica que los cuatro candidatos previos no se seleccionaran;
ya no corresponde repetirlos ni activar eOI solamente por el ATR.

## Consulta PKCS#15 nativa sin PIN

Con la misma JCOP4 insertada y firmador cerrado:

```bash
make inspect/jcop4-pkcs15 READER=0
```

Compila solamente el helper contra OpenSC existente. Mantiene la comprobación
del ATR y la consulta de EF.ATR/EF.DIR en una transacción. El helper recibe
el AID descubierto y exige que siga anunciado antes de hacer el binding.
Usa `sc_pkcs15_bind_internal()`, sin emulación ni driver específico.
Desactiva cachés de archivos/PIN explícitamente en el objeto PKCS#15.

Consulta tres directorios de metadatos: certificados X.509, llaves privadas
y objetos PIN. Lee DER únicamente de certificados públicos; omite objetos
marcados privados. De las llaves obtiene tipo, ID, referencia, uso, ruta y
tamaño RSA declarado, sin leer componentes de la llave privada. De los PIN
obtiene referencia, flags, ruta y límites declarados en AODF; no consulta
su estado físico ni su valor. No hay VERIFY, login, MSE, CAN/PACE o firma.
Los errores detienen el flujo y liberan la transacción, sin reintentos.

El informe privado `local/jcop4-pkcs15-*/report.txt` contiene certificados
identificables en hexadecimal: mantenerlo fuera de Git y no pegarlo completo
en el chat. Los DER se contrastaron con OpenSSL sin tarjeta para corroborar
RSA 2048 y vincular los certificados a las referencias PKCS#15. Los tamaños
y referencias de esta consulta no se fijan a 2048 ni a la llave 3 de JCOP3.
Una lectura exitosa no demuestra que la firma o el driver específico funcionen.

## Resultado PKCS#15: 13 de octubre, 07:50:37 UTC

Informe `local/jcop4-pkcs15-msMLoEtW/report.txt`, OpenSC `3dfef6704`, principal
`8edee31` al revisar. Binding nativo 0, dos certificados públicos leídos,
dos metadatos de llaves privadas y un objeto PIN enumerados, transacción
liberada y proceso 0. No se utilizó el PIN.

OpenSSL `x509 -inform DER -pubkey` y `pkey -pubin -text -noout` corroboraron
`rsaEncryption`, **2048 bits** y exponente 65537 en ambos certificados.
La comparación de ID con PrKDF produjo estas asociaciones:

| Objeto | Referencia privada | Tamaño certificado / PrKDF | Uso PrKDF |
| --- | --- | --- | --- |
| LlaveDeFirma | `0x91` (145) | 2048 / 2048 | `0x0C` |
| LlaveDeAutenticacion | `0x90` (144) | 2048 / 2048 | `0x2E` |

Las dos llaves se declaran nativas y con ruta vacía. Las referencias son
metadatos, no demostración del comando RSA que debe usarse. AODF declara
Card PIN referencia 1, flags `0x11` (sin flag LOCAL, por tanto global),
mínimo 6 y máximo 14. Estos son límites públicos declarados, no lectura
del PIN ni de su estado físico. Un solo objeto PIN enumerado no demuestra
que otras operaciones carezcan de requisitos CAN/PACE.

Se conservaron los DER, llaves públicas PEM y `certificate-summary.json`
privados junto al informe. La comprobación establece algoritmo/tamaño y
asociación de ID; no valida cadena de confianza, revocación ni firma.
El perfil difiere de JCOP3 en AID, raíz y referencias privadas. No trasladar
la excepción de PIN ni la referencia 3 de aquel driver.

## Selección temporal RSA/SHA-256 sin PIN

Encontramos un precedente de protocolo en la
[guía CPS V4 de ANS, versión 1.03](https://industriels.esante.gouv.fr/sites/default/files/media/document/ANS_CPSV4_0011_Guide_De_R%C3%A9f%C3%A9rence_v1_03.pdf),
secciones 6.6.1 y 6.7.1: otra personalización ChipDoc documenta MSE SET
`00 22 81 B6`, plantilla `91 02`, algoritmo `28` para SHA-256/RSA PKCS#1 v1.5,
y PSO `00 2A 9E 9A`. Allí el bit alto de la referencia indica hash externo.
Esto es una **hipótesis de protocolo**, no evidencia de compatibilidad con
Costa Rica. Sus referencias y políticas pueden diferir.

Con firmador y otros clientes cerrados y la misma JCOP4 insertada:

```bash
make inspect/jcop4-sign-setup READER=0
```

Compila solo el helper, usa driver genérico y mantiene una transacción.
Comprueba el ATR conectado, el AID observado y su anuncio en EF.DIR antes
de binding nativo. Registra las referencias/mecanismos/operaciones de
algoritmos publicados en TokenInfo y referencias de algoritmos en PrKDF,
sin derivar mecanismos compatibles de su ausencia. Omite lectura de DER
en este modo; conserva la lectura de metadatos de certificados/llaves/PIN.

Exige un único ID LlaveDeFirma, RSA nativa, uso SIGN, referencia `0x91` y
2048 bits, como en la captura. Si cambian, termina antes de MSE. Son límites
del diagnóstico actual, no condiciones del futuro driver ni rechazo de
RSA 3072 a nivel de arquitectura. Reselecciona el AID de aplicación para
salir del contexto del último archivo leído; ante rechazo no envía MSE.

Envía **una sola** selección temporal:

```text
00 22 81 B6 04 91 02 28 91
```

El último byte se obtiene de los metadatos comprobados y se conserva íntegro;
no se elimina ni reasigna su bit alto. El informe privado
`local/jcop4-sign-setup-*/report.txt` registra transporte y SW. Solo `9000`
con transporte correcto se considera selección aceptada. Otros SW, incluso
si la herramienta no falla en transporte, detienen sin reintentos ni variantes.
No hay VERIFY, login, consulta física de PIN, PSO, CAN/PACE, lectura de
componentes privados ni operaciones administrativas. MSE cambia únicamente
el entorno temporal de la sesión. `9000` no demuestra que la firma funcione
ni qué autenticación requiere. Esa prueba sigue pendiente.

## Resultado MSE: 13 de octubre, 08:03:22 UTC

Informe `local/jcop4-sign-setup-qob4FMSh/report.txt`, principal `4611409`,
OpenSC `3dfef6704`. Binding nativo y selección correctos, metadatos sin
cambios: LlaveDeFirma RSA 2048/0x91 y PIN global 1. La selección
`00 22 81 B6 04 91 02 28 91` obtuvo transporte 0 y **SW 9000**.
Transacción liberada y proceso 0; sin PIN ni PSO. No repetir automáticamente.

TokenInfo no anunció algoritmos (`algorithm_metadata_count=0`) y no aparecieron
referencias de algoritmos en PrKDF. Esta ausencia no prueba falta de soporte;
la tarjeta sí aceptó configurar el comando propuesto. Sigue pendiente
comprobar autenticación, operación RSA y verificación criptográfica del
resultado. Aceptar MSE no confirma aún padding, formato de entrada ni
requisitos de canal seguro de la operación de firma.

## Consulta vacía del estado del PIN

Con firmador y otros clientes cerrados y la misma JCOP4 insertada, una vez:

```bash
make inspect/jcop4-pin-status READER=0
```

Compila solo el helper. Conserva el driver genérico, comprobación del ATR
conectado/AID anunciado, binding nativo y transacción. Lee metadatos públicos
sin DER; agrega al informe tipo de autenticación, método, formato y carácter
de padding declarados en AODF. Estos describen el protocolo, no el valor
ni la longitud de ningún PIN introducido. Vincula el objeto PIN a la llave
de firma mediante el auth ID de la llave y el ID de autenticación del PIN;
no utiliza la credencial administrativa que protege el objeto PIN.

Exige la llave observada y un único PIN vinculado: referencia 1, flags 0x11,
límites 6–14 y ruta vacía. Un cambio de metadatos detiene antes de la consulta;
no intenta otras referencias. Reselecciona la aplicación y envía:

```text
00 20 00 01
```

Es VERIFY caso 1, sin datos ni Le, como el generado para GET_INFO por
`iso7816_build_pin_apdu()` en OpenSC. Consulta el estado, sin probar un PIN.
Las banderas impiden reintentos por 6C y GET RESPONSE automático de esta
APDU. No envía MSE, PSO, CAN/PACE ni comandos administrativos. Libera la
transacción incluso ante fallo. Stdin vacío, depuración 0 y cachés apagadas.

| Respuesta | Interpretación del diagnóstico |
| --- | --- |
| `63Cx`, x > 0 | PIN no verificado, x intentos disponibles |
| `9000` | PIN ya verificado en ese contexto; el helper no lo verificó |
| `63C0` o `6983` | Detener; posible bloqueo, sin intento de desbloqueo |
| Otro SW / error de transporte | Detener; contexto o protocolo aún sin confirmar |

El informe privado queda en `local/jcop4-pin-status-*/report.txt`.
El éxito del diagnóstico confirma que se consultó un estado reconocido,
no que haya login o firma. Se revisará antes de solicitar el PIN por primera
vez. La autenticación real se preparará con formato/contexto comprobados,
sin capturas APDU ni reintentos de PIN automáticos.

## Resultado de estado PIN: 13 de octubre, 08:10:14 UTC

Informe `local/jcop4-pin-status-BYJtNyTV/report.txt`, principal `765ef89`,
OpenSC `3dfef6704`. Binding/selección correctos, transporte 0, **SW 63C7**:
PIN no verificado, siete intentos disponibles. Transacción liberada y proceso 0.
No se introdujo PIN ni hay evidencia de consumo de un intento de autenticación.
No repetir por inercia.

Los metadatos declaran autenticación PIN/CHV, referencia global 1, formato
UTF-8 (`2`), flags `0x11` sin padding, carácter de pad `00`, límites 6–14 y
ruta vacía. El objeto se vincula a la llave de firma por su auth ID Card PIN;
la credencial administrativa que protege el objeto no intervino. Estos
parámetros permiten delimitar una primera autenticación experimental.

## Primera firma experimental por APDU

Protocolo preparado y ya ejecutado; **no repetir ahora**. El resultado
de VERIFY y la siguiente consulta sin PIN están documentados abajo.
El comando utilizado, con firmador y otros clientes cerrados, fue:

```bash
nix develop
make sign/jcop4-apdu READER=0
```

Se compila solo `tools/card-sign-test.c`, usando `util_getpass()` de OpenSC
para la entrada sin eco. El resultado queda en `local/jcop4-signature-*/`:
texto técnico `message.txt`, SHA-256 `digest.bin`, certificado público
`certificate.der`, llave pública PEM, firma e informe. Directorio 0700,
archivos 0600, sin datos privados en Git. El texto se muestra antes del PIN.
No se envía ningún PDF a la tarjeta.

Es una prueba de protocolo sobre libopensc y el driver genérico, previa a
integrar JCOP4 en PKCS#11. La firma por este helper no demostraría aún que
firmador pueda cargar la biblioteca y usar esta tarjeta.

El helper exige terminal tanto en stdin como stdout y comprueba ATR conectado,
AID anunciado y binding nativo. Valida un único ID LlaveDeFirma RSA nativa
2048/ref 0x91/uso SIGN/ruta vacía y un único PIN vinculado con los parámetros
observados, incluido UTF-8 y ausencia de padding. Lee únicamente el certificado
público del mismo ID y corrobora tipo RSA y módulo de 256 bytes antes del PIN.
No se traslada la llave 3 ni la excepción de PIN de JCOP3. Los límites actuales
son controles de esta prueba, no diseño definitivo para variantes futuras.

En una sola transacción:

1. Selecciona el AID de aplicación tras leer los archivos públicos.
2. Consulta VERIFY vacío; bloqueo, estado inesperado o error detienen antes
   de pedir el PIN. No se toma un estado ya verificado como permiso para omitirlo.
3. Configura MSE `91 02 28 91`, ya aceptado en hardware.
4. Recoge el PIN sin eco, valida límites públicos y envía **un VERIFY** a
   referencia 1, sin padding ni transformaciones. Borra el buffer inmediatamente.
5. Ante VERIFY 9000, envía **un PSO `00 2A 9E 9A`**, con el SHA-256 de 32 bytes
   como entrada y Le 256. No selecciona archivos ni relee metadatos después
   de la autenticación. La entrada digest es una hipótesis por verificar.
6. Tras cualquier intento autenticado, reinicia la sesión con `sc_reset(card,0)`
   y libera la transacción; no altera PIN, llaves ni objetos persistentes.
7. Si PSO devuelve 9000 y 256 bytes y la limpieza termina bien, OpenSSL
   verifica RSA/SHA-256 sobre el texto usando el certificado leído de la tarjeta.

VERIFY, MSE y PSO no se reenvían automáticamente por 6C. GET RESPONSE se
permite para recuperar la respuesta normal de PSO, sin repetir la firma.
No CAN/PACE, cambio de PIN, desbloqueo, lectura de componentes privados ni
operaciones administrativas. Un error detiene sin otro PIN ni variante de
comando. Revisar el informe antes de proponer otro intento.

No capturar APDU durante esta prueba. El helper autenticado conserva la
terminal, sin tee/redirección; debug 0, trazas API y experimentos JCOP3
apagados. Solo registra etapas, resultados/SW y tamaño público de firma,
**nunca el PIN, su longitud real ni hash**. No usa caché ni replay de PIN.
La única tubería tee corresponde a verificación offline posterior al helper.

El helper real compiló contra OpenSC offline. Pasaron siete comprobaciones
C de perfil/operación con credenciales sintéticas, sin lector, y cuatro del
lanzador bajo PTY con helper/OpenSSL simulados. Cubren flujo único,
rechazos antes del PIN, formatos distintos, PIN sintético rechazado,
VERIFY/PSO negativos, respuesta corta, transporte y reset fallidos,
limpieza de memoria y ausencia de credencial en el informe. Log C:
`build/opensc-offline/jcop4-sign-operation-tests.log`. Las pruebas con PTY
comprueban terminal/configuración/permisos y detención sin reintentos; no
producen una firma criptográfica real. Ambos conjuntos se agregaron a
`make test/iasecc`; util-linux en el flake proporciona `script` para la PTY.
No se repitió la suite JCOP3 ni cambió el driver durante esta preparación.
La firma en hardware, login exitoso y verificación real siguen pendientes.

## VERIFY rechazado: 13 de octubre, 13:44:00 UTC

Informe válido `local/jcop4-signature-5Dpadr1N/report.txt`, principal
`b9c5696`, OpenSC `3dfef6704`. El usuario pidió excluir
`jcop4-signature-4FGWOtpI` por error humano; se preserva el original sin
utilizarlo para interpretar el protocolo, comparar intentos o inferir contadores.

| Etapa | Resultado |
| --- | --- |
| Perfil/certificado público | Validados; RSA 2048 |
| VERIFY vacío | Transporte 0, 63C7 |
| MSE RSA/SHA-256 | Transporte 0, 9000 |
| VERIFY con PIN, una transmisión | Transporte 0, **6982** |
| PSO/firma | No ejecutados |
| Reset/unlock | Ambos 0 |
| Helper/proceso | Ambos 1, detención ante el rechazo |

6982 indica condición de seguridad no satisfecha. El fallo ocurre antes
de la firma, durante la autenticación. No equivale a la respuesta típica
de PIN incorrecto 63Cx, pero tampoco acredita el PIN ni prueba que no se
consumió un intento: el informe contiene el contador solo **antes** de VERIFY.
No solicitar otro intento de PIN hasta investigar la condición que falta.

La revisión de `eoi_pin_cmd()` en `card-eoi.c` muestra que abre el canal
PACE mediante `eoi_sm_open()` antes de VERIFY si aún no está abierto, sin
condicionar ese paso a contactless. `card-chipdocit.c` también precede VERIFY
con PACE/CAN. Son precedentes de perfiles diferentes, **no evidencia de que
la tarjeta costarricense exija PACE, CAN o el mismo protocolo**. La
[documentación eOI de OpenSC](https://github.com/OpenSC/OpenSC/wiki/Slovenian-EID)
describe CAN para NFC; no permite deducir el comportamiento de contacto de CR.
También sigue abierta la influencia del orden MSE antes de VERIFY.

La consulta PKCS#15 anterior solo enumeró AUTH_PIN: encontrar un PIN no
excluye otros objetos de autenticación, como AuthKey. Por eso la siguiente
consulta enumera la clase AUTH completa y obtiene información pública
acotada, antes de considerar cambios al protocolo autenticado.

## Consulta pública de seguridad

Con la misma JCOP4 insertada, firmador y otros clientes cerrados:

```bash
nix develop
make inspect/jcop4-security-info READER=0
```

Compila únicamente el helper público contra la biblioteca existente. Usa
driver default, ATR/AID observados, binding nativo sin cachés y una transacción.
No introduce PIN, abre PACE ni lee CAN, secretos o componentes privados.
Se detiene ante un rechazo de seguridad/transporte y libera la transacción.
El informe privado está en `local/jcop4-security-info-*/report.txt`.

Operaciones después de leer EF.ATR/EF.DIR y metadatos PKCS#15:

1. Enumera metadatos de toda la clase AUTH: identificadores, tipo, método,
   ruta y referencia a llave cuando aplica. Nunca lee el archivo de credencial
   al que pudiera apuntar esa ruta.
2. Selecciona el AID anunciado y consulta seis bytes con `00 CA 01 16 06`,
   el GET DATA utilizado por `eoi_init()`. Registra longitud y bytes sin
   asignarles significado o versión comercial en esta tarjeta. Acepta
   respuestas de 0–6 bytes con 9000; la longitud exacta de eOI no es un
   requisito demostrado del perfil CR. Una longitud mayor al buffer detiene.
3. Selecciona MF `3F00` y lee **un solo bloque**, hasta 256 bytes, mediante
   `00 B0 9C 00 00`. SFI `1C` es el usado por `get_ef_card_access()` en
   `vendor/opensc/src/sm/sm-eac.c`. No hace lecturas posteriores, GET RESPONSE
   ni reenvío por 6C, ni interpreta un bloque parcial como archivo completo.

Los estados de función/objeto no disponible se registran como
`unavailable_in_this_query`; un rechazo 6982 es un error y detiene el flujo.
La ausencia en esta consulta no prueba que no haya PACE u otro canal seguro.
Encontrar algoritmos PACE en CardAccess tampoco demostraría por sí solo
que sean obligatorios para verificar el PIN por contacto.

El helper real compiló contra `build/opensc-offline`. Pasaron **30
comprobaciones C** (24 previas y seis grupos nuevos) y **19 del lanzador**
(17 previas y dos nuevas). Verifican clase AUTH con AuthKey sintético sin
lectura de credencial, bytes y orden de las consultas, una sola lectura,
estados opcionales, fallos de transporte/seguridad/longitud y limpieza;
el lanzador verifica configuración aislada, stdin vacío y propagación de error.
Log C: `build/opensc-offline/card-public-security-info-tests.log`.
Sin pruebas nuevas en tarjeta por el agente ni cambios en el driver OpenSC.

## Consulta de seguridad: 13 de octubre, 14:00:01 UTC

Informe `local/jcop4-security-info-9FQ37Lk3/report.txt`, principal
`b41f16b`, OpenSC `3dfef6704`. Binding y metadatos leídos correctamente.
La enumeración de toda la clase AUTH devuelve **dos objetos**: Card PIN
y Card Admin Key (AuthKey derivada, método 8, sin ruta ni ID de llave
simétrica declarado). No se enumeró Card CAN; esto no descarta PACE ni
otros mecanismos y no autoriza operaciones con la llave administrativa.

SELECT de aplicación y transporte GET DATA terminaron bien, **SW 9000**.
El helper falló por exigir exactamente seis bytes, una suposición tomada
del perfil eOI que no está demostrada aquí. El informe no registraba la
longitud recibida: se sabe que difiere de seis, pero no cuál fue. **No llegó
a SELECT MF ni READ de CardAccess**; unlock 0 y proceso 1. No hubo VERIFY,
PIN ni autenticación en esta consulta, y no se obtuvo 6982.

Se corrigió el helper para registrar longitud/bytes, aceptar 0–6 bytes
sin interpretar su formato y continuar la consulta pública de CardAccess
tras 9000. Se mantiene el límite del buffer y la detención ante errores
de seguridad/transporte. No se cambian APDU, reintentos ni el protocolo de
firma. La regresión nueva cubre respuestas de 0, 1 y 5 bytes y comprueba
que se llega a la única lectura pública, sin VERIFY/MSE ni credenciales;
7 bytes sigue siendo un error. Pasaron las 30 comprobaciones del helper
y su compilación real offline. El lanzador no cambió ni se repitió su suite.

La intervención posterior fue ejecutar una vez la misma consulta pública
`make inspect/jcop4-security-info READER=0`, con esta corrección y los otros
clientes cerrados. La repetición respondió al fallo del helper; no solicitó
un nuevo intento de PIN. Su resultado se documenta a continuación.

## Resultado de seguridad: 13 de octubre, 14:03:00 UTC

Informe `local/jcop4-security-info-pveu5cDK/report.txt`, principal
`83b2eda`, OpenSC `3dfef6704`. La corrección de longitud funcionó: GET DATA
de versión obtuvo transporte 0, 9000 y **cuatro bytes `03 02 00 52`**.
La sección 2.2.4 (página 8) del
[informe ANSSI-CC-2025/03v2](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03v2-rapport.pdf)
identifica exactamente esos bytes como **ChipDoc 3.2.0.52**. Esto respalda
la identificación de la aplicación, sin probar la personalización, certificación
de la tarjeta CR ni su variante de plataforma. No trasladar las reglas de
otros productos certificados al perfil costarricense.

SELECT MF terminó bien; READ BINARY `00 B0 9C 00 00` devolvió transporte 0
y **6A82**. CardAccess no estuvo disponible por SFI 1C desde MF en esta
consulta: no se leyó el archivo ni se pueden interpretar algoritmos PACE.
No demuestra ausencia de PACE en otros contextos. Clase AUTH sigue con
Card PIN y Card Admin Key; no Card CAN enumerado. Unlock y proceso 0.
No hubo PIN ni autenticación. No repetir el diagnóstico de versión/CardAccess
como primer paso. La causa de VERIFY 6982 continúa pendiente.

## Consulta de política del PIN

El siguiente dato relevante es la política pública del objeto CHV 1. Se
preparó una comparación acotada entre MF y el AID anunciado, usando la
plantilla GET DATA DOCP de `iasecc_sdo_get_tagged_data()` de OpenSC. El
formato funcionó en JCOP3; su disponibilidad y semántica en JCOP4 todavía
deben comprobarse. No se asume igualdad de políticas por compartir ChipDoc.

Con la misma tarjeta y otros clientes cerrados, desde nix develop:

```bash
make inspect/jcop4-pin-policy READER=0
```

Después del binding nativo/validación del perfil y PIN global 1 vinculado,
hace SELECT MF y GET DATA, después SELECT aplicación y el mismo GET DATA:
`00 CB 3F FF 0A 4D 08 70 06 BF 81 01 02 A0 80 00`. Solo pide **DOCP A0**,
metadatos de control; nunca la plantilla de valor CHV `7F41`. Registra
contexto, transporte, SW, longitud y bytes. Máximo dos consultas, buffer/Le
256, sin GET RESPONSE, reenvío por 6C ni variantes de referencias/plantillas.
No pide PIN, autentica, firma, abre canales seguros ni usa la llave administrativa.
No modifica llaves, PIN ni objetos persistentes. Informe privado
`local/jcop4-pin-policy-*/report.txt`.

Un rechazo de seguridad, transporte o longitud fuera del buffer detiene
antes del siguiente contexto y libera la transacción. Objeto/función no
disponible se registra como tal; un proceso 0 solo confirma consultas
terminadas, no que se obtuviera o interpretara una política válida.

Compiló el helper real offline y pasaron **35 comprobaciones C** y **21
del lanzador**. Las nuevas cubren APDU exactas de DOCP, orden MF/aplicación,
exclusión de VERIFY/MSE/otras plantillas, metadatos incompatibles, ausencia
opcional, errores de seguridad/transporte/SELECT, tamaño excesivo y limpieza.
Log: `build/opensc-offline/card-public-pin-policy-tests.log`.
Ninguna prueba del agente usó hardware; el submódulo OpenSC no cambió.

## DOCP no disponible: 13 de octubre, 14:10:23 UTC

Informe `local/jcop4-pin-policy-pMGzFYNL/report.txt`, principal `292c6b4`,
OpenSC `3dfef6704`. Binding, metadatos y SELECT de ambos contextos correctos.
GET DATA de CHV 1 / DOCP A0 devolvió transporte 0 y **6D00**, respuesta
vacía, tanto en MF como en aplicación. El comando/formato IAS/ECC utilizado
no está soportado en los contextos consultados: no hubo política que analizar.
Esto impide reutilizar directamente esta consulta JCOP3; no prueba que toda
la aplicación o sus mecanismos de firma sean incompatibles con IAS/ECC.
No ampliar el barrido de referencias/plantillas sin evidencia de otro comando.

Unlock y proceso 0: el helper terminó las consultas y liberó la tarjeta.
No significa que obtuviera una política válida; se registró disponibilidad
negativa. No hubo PIN, autenticación, firma ni operaciones administrativas.
VERIFY 6982 anterior sigue pendiente; no diagnosticar PIN incorrecto a partir
de estos datos ni solicitar otro intento por inercia.

La revisión del módulo Idopte instalado confirmó dependencias PC/SC y
nombres de funciones relacionadas con canales seguros y restricciones de
PIN. Los símbolos no muestran qué protocolo utiliza para esta JCOP4, y
no se copió implementación privativa. La
[cible pública NXP ChipDoc 3.2 SSCD](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03-cible.pdf),
sección 1.3.2.1.3, contempla PACE con MRZ/CAN/PIN y comunicaciones protegidas.
Es un marco de investigación, no prueba de que este perfil requiera PACE
ni explicación suficiente de 6982. La ausencia de Card CAN y el 6A82 de
CardAccess tampoco descartan todas las formas de canal seguro.

## Información pública de Idopte

Antes de nuevas APDU hipotéticas, revisar qué perfil y estado anuncia el
módulo instalado. Se pidió al usuario confirmar si esta JCOP4 había firmado
correctamente con Idopte; no se le solicitó repetir una firma ni proporcionar
su PIN. Esa confirmación es independiente de la consulta siguiente.

Desde nix develop, tarjeta insertada y firmador/otros clientes cerrados:

```bash
make inspect/jcop4-idopte READER=0
```

El lanzador hace el inventario PC/SC y confirma el ATR observado. Después
ejecuta exclusivamente `pkcs11-tool --module
/run/current-system/sw/lib/SCMiddleware/libidop11.so --show-info --list-slots`
con la herramienta compilada localmente. Según el flujo de pkcs11-tool estas
opciones consultan información de biblioteca, slots y tokens sin pedir
C_OpenSession ni C_Login. No leen objetos, autentican ni firman. Idopte
puede realizar su descubrimiento interno de tarjeta; no se captura APDU.

No recompila ni usa el helper de autenticación. Stdin vacío, debug OpenSC 0,
trazas desactivadas y configuración aislada. Errores o faltas de módulo/
herramienta detienen sin reintentos ni sustitución por otro módulo. Resultado
privado en `local/jcop4-idopte-*/report.txt`: etiquetas y seriales pueden
identificar al titular; no publicarlos sin anonimizar.

Se revisará presencia/perfil y los flags anunciados de PIN (inicializado,
bloqueado, último intento, cambio pendiente si corresponde). Esos flags
no prueban autenticación o firma, y pueden depender de la lectura/caché del
módulo. READER comprueba el lector PC/SC; la consulta Idopte enumera todos
sus slots, con índices potencialmente distintos y varias vistas por tarjeta.

Pasaron **25 comprobaciones del lanzador**, cuatro nuevas para Idopte:
comando acotado y datos privados, stdin/configuración, propagación de errores,
ATR distinto y herramientas/módulo ausentes. Las herramientas se simulan;
no se cargó Idopte ni utilizó lector en estas pruebas. No se repitieron las
regresiones C/JCOP3 ni se modificó el submódulo.

## Idopte reconoce ChipDoc v3.2: 13 de octubre, 14:17:31 UTC

Informe `local/jcop4-idopte-6aaJ5sIO/report.txt`, principal `697eb0e`, OpenSC
`3dfef6704`. Módulo Idopte 6.23/Cryptoki 2.40, slot 0 presente, fabricante
NXP, modelo **ChipDoc v3.2**, límites públicos PIN 6/14. Flags anunciados:
login required, rng, token initialized, PIN initialized. No flags de bloqueo,
último intento o cambio pendiente. Proceso 0; no sesión/login/objetos/firma
solicitados. Etiqueta, serial y URI se conservan solo en el informe privado.

Coincide con identificación de versión/PKCS#15, pero no prueba el PIN, su
contador exacto o una firma con Idopte. La confirmación de uso previo de esta
JCOP4 con Idopte solicitada al usuario continúa pendiente. No repetir la
consulta normal como primer paso ni atribuir 6982 a un cambio de PIN sin evidencia.

## Descubrimiento público observado, pendiente

Preparado `tools/pcsc-public-observer.c`, precarga PC/SC para **este único
diagnóstico sin login**, sin modificar OpenSC ni Idopte. Objetivo: identificar
el primer paso de descubrimiento que requiere un protocolo aún no estudiado.
No usar la precarga con firmador ni con ninguna operación autenticada.

Desde nix develop, misma tarjeta y otros clientes cerrados:

```bash
make inspect/jcop4-idopte-discovery READER=0
```

Compila solo la biblioteca de diagnóstico con headers PC/SC del flake. Tras
inventario/ATR se activa exclusivamente para el mismo `--show-info --list-slots`
de Idopte. El observador permite SELECT ISO de MF/AIDs observados y archivos
bootstrap públicos (sin DFs intermedios desconocidos), GET DATA de versión CA 01 16,
READ BINARY/RECORD de archivos bootstrap públicos 2F00/2F01/5031/5032 tras
selección explícita y GET RESPONSE tras 61xx de una operación permitida.
**Bloquea antes de transmitir** otros archivos/GET DATA/CLA, todos los VERIFY,
MSE, PSO, autenticación, escritura y controles del lector (incluido PIN pad).
Solo se transmite al primer handle, máximo 128 comandos; después del primer
bloqueo/error de transporte o 69xx, no transmite más comandos de fallback.

Guarda cabecera de cuatro bytes y transporte/SW, **nunca datos de comando/
respuesta, AID/FID del payload, tamaños, Lc/Le, PIN ni hash**. Requiere crear
log nuevo con O_EXCL/NOFOLLOW y modo 0600; si falla la creación, no transmite.
La precarga externa se elimina del inventario, y esta se habilita solo en el
proceso de diagnóstico. No autentica ni firma y no reenvía comandos bloqueados.

Informe privado en `local/jcop4-idopte-discovery-*/report.txt`, cabeceras en
`public-headers.txt`. El informe del módulo puede identificar al titular;
no publicar su etiqueta/serial/URI. **Idopte puede devolver error por el bloqueo**;
el lanzador conserva cabeceras y estado real del módulo. Examinar el primer
`blocked_header`/control antes de concluir fallo de tarjeta. Un comando bloqueado
no es evidencia de cómo respondería la tarjeta: no llegó al transporte.
Ausencia de eventos PC/SC se reporta como fallo de observación, no éxito.

Pasaron **29 comprobaciones del lanzador** y **7 del observador**. Incluyen
VERIFY/PIN sintético bloqueado, lectura de otros archivos/componentes y CLA
protegida bloqueadas, respuesta privada sintética omitida, ausencia de longitudes,
limpieza/fallback sin transmisión, controles del lector, límites/errores y
precarga ELF real contra transporte falso. Biblioteca real compilada, sin
Idopte ni PC/SC real en pruebas. Repetibles con `make test/pcsc-public-observer`
e incluidas en `test/iasecc`; no se repitió la suite completa JCOP3.

## Diseño requerido para 2048, 3072 y futuras generaciones

Separar tres dimensiones: plataforma JCOP, protocolo/aplicación y tamaño
de la llave. El ATR es una pista para descubrir el perfil; los AID y metadatos
de la aplicación deben delimitar sus adaptaciones. El diseño debe permitir
varios perfiles sin copiar un driver por generación de hardware.

La rama actual tiene excepciones medidas para JCOP3 que aún se limitan a
Amos + ChipDoc + llave de firma 3 + RSA 2048. No equivale a una implementación
modular terminada. Al integrar JCOP4:

- Centralizar la identificación y las diferencias de perfiles, conservando
  las restricciones de seguridad que se hayan confirmado en cada tarjeta.
- Separar las reglas de PIN de los tamaños RSA y obtener referencias/tamaños
  de PKCS#15/DOCP cuando esos metadatos sean fiables.
- Separar hash/DigestInfo, operación RSA y transporte. RSA 2048 produce una
  firma de 256 bytes; RSA 3072 produce 384. Revisar buffers, Le, GET RESPONSE
  y APDU extendidas, además de los mecanismos anunciados. El camino AT actual
  usa un buffer de 261 bytes y Le de 256; no basta con agregar 3072 a la lista.
- Mantener regresiones por perfil, probar respuestas de 384 bytes mediante
  transporte simulado y validar RSA 3072 con hardware cuando esté disponible.
  Las pruebas de la tarjeta actual corroborarán RSA 2048, no RSA 3072.

La consulta pública nueva es independiente del modelo y del tamaño RSA;
el lanzador JCOP4 conserva la restricción al ATR observado como control de
la prueba. No usar ese control diagnóstico como detección final del driver.
Se decidirá cómo reutilizar iasecc/eOI cuando se conozca el perfil real.

## Después del ATR

1. Comparar el ATR y la detección prevista por OpenSC con la evidencia JCOP3.
2. Preparar consultas públicas de aplicación, directorio PKCS#15 y certificados
   según ese resultado, registradas aparte. No reutilizar a ciegas las APDU
   ni las excepciones de PIN/referencia privada medidas en JCOP3.
3. Comparar AIDs, política pública de PIN, referencias privadas, mecanismos
   y tamaños RSA antes de modificar la detección o habilitar una firma.
4. Si corresponde al mismo protocolo, ampliar un perfil delimitado dentro de
   `iasecc` y agregar regresiones con las respuestas nuevas. No crear un
   driver por generación de plataforma sin diferencias de protocolo.
5. Obtener una firma de prueba verificada con el certificado de esta tarjeta
   y después probar firmador. RSA 3072 requiere revisar el transporte de
   respuestas de 384 bytes; no basta con anunciar ese tamaño.

Se mantienen las restricciones de privacidad y de operaciones sobre tarjetas
emitidas de [AGENTS.md](../AGENTS.md). NFC, CAN/PACE y algoritmos poscuánticos
siguen fuera de estas pruebas. Pasaron **17 comprobaciones del lanzador** en
`tests/jcop4-probe-launcher.sh` y **24 del helper** en `tests/card-public-info.c`.
Esta última se compila contra OpenSC pero simula las API de conexión/lectura
/selección/transporte, sin PC/SC ni lector. La herramienta real también compiló
contra `build/opensc-offline`. Log C:
`build/opensc-offline/card-public-pin-status-tests.log`. Cubren MSE único,
bytes exactos, rechazo de otros metadatos/perfiles, SW negativos, fallos de
transporte/SELECT, VERIFY sin datos/Le, estados 63Cx/9000/bloqueo y
limpieza sin autenticación ni reintentos. Las simulaciones
de consulta pública incluyen metadatos RSA 3072 con otra referencia; no
validan RSA ni transporte de una firma de 384 bytes. No se repitió toda la
suite JCOP3 ni se modificó su driver durante esta investigación.
