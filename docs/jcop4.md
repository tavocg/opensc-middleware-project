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

## Primera versión del observador público (v1, sustituida)

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

## Sin eventos observados: 13 de octubre, 14:32:27 UTC

Informe `local/jcop4-idopte-discovery-y7dskS3z/report.txt`, principal
`fbf3434`, OpenSC `3dfef6704`. Inventario y ATR correctos, compilación del
observador 0. Idopte mostró slot vacío y terminó 1. El archivo de cabeceras
solo contiene el marcador de formato v1: **no registró transmisión, control
ni bloqueo**. No hay respuesta APDU para interpretar. No demuestra que la
tarjeta rechazara un comando ni que el módulo no transmitiera por otra ruta.

La investigación local reprodujo una limitación de la implementación v1:
una biblioteca precargada puede interceptar la llamada de un plugin
RTLD_LOCAL sin encontrar su transporte mediante RTLD_NEXT. Esa reproducción
registra una cabecera, pero no llega al transporte falso; por tanto, **no
explica por sí sola la ausencia total de eventos del informe real**.

OpenSC carga módulos PKCS#11 con RTLD_LOCAL en src/common/libscdl.c;
OPENSC_DEEPBIND puede modificar esa búsqueda. El lanzador v2 elimina también
esta variable externa para mantener el diagnóstico reproducible.

La versión v2 usa la [delegación oficial de pcsc-lite](https://pcsclite.apdu.fr/api/libredirect_8c_source.html):
`LIBPCSCLITE_DELEGATE` apunta al observador únicamente en el proceso público
de Idopte. El observador depende explícitamente de `libpcsclite_real.so.1`
del libdir de pkg-config, que proporciona las demás funciones PC/SC y el
transporte siguiente. El compilador conserva esa dependencia y su ruta;
si no existe la biblioteca real, se detiene antes de ejecutar Idopte.
No hay sustitución de bibliotecas del sistema ni LD_PRELOAD en el diagnóstico.

Conserva la misma lista limitada de APDU y bloqueos de autenticación/firma,
añadiendo bloqueo de SCardSetAttrib. Registra únicamente nombre/resultado de
SCardEstablishContext, SCardConnect y SCardReconnect, con límite de 128 eventos;
no nombres de lector, handles, ATR, datos, longitudes ni respuestas completas.
Conexión/reconexión eliminan permisos de lectura/GET RESPONSE anteriores.
Si solo se observan estas funciones, el informe dice pcsc_lifecycle_only y
termina 1; sin eventos, no_pcsc_event_observed. Ambos conservan el log y el
estado real de Idopte sin anunciar observación APDU ni reintentar.

Pasaron **30 comprobaciones del lanzador y 10 del observador**. La nueva
regresión carga un plugin local a través del redirector PC/SC real, pero
delega exclusivamente a una biblioteca ficticia, sin daemon/tarjeta/Idopte.
Comprueba scopes local/global, conexión fallida, dependencia resoluble,
omisión de datos privados y bloqueo de VERIFY, controles y atributos antes
del transporte. También conserva la reproducción del defecto anterior.
Se compiló el observador real; no se modificó ni recompiló OpenSC.

Siguiente intervención, misma JCOP4 y otros clientes cerrados:

```bash
make inspect/jcop4-idopte-discovery READER=0
```

No solicita PIN. Revisar los resultados de conexión y el primer bloqueo
antes de ampliar consultas. Una captura estándar con pcsc-spy durante firma
podría registrar el PIN; este observador todavía no admite sesiones autenticadas.

## Primer bloqueo identificado: 13 de octubre, 14:53:29 UTC

Informe `local/jcop4-idopte-discovery-aPCZL12P/report.txt`, principal
`7f31abc`, OpenSC `3dfef6704`. Compilación e inventario correctos. La
delegación sí funcionó: SCardEstablishContext -> 0, SCardConnect -> 0,
**blocked_header=00CA0100**, segunda conexión -> 0. Idopte mostró el slot
vacío y terminó 1; observer_result=events_recorded. El primer comando
**no llegó a la tarjeta**, y el observador bloquea las transmisiones siguientes.
Esto distingue el fallo de observación anterior de un descubrimiento ahora
interrumpido por la lista permitida. No hay SW de tarjeta para este comando.
Sin PIN, VERIFY ni firma.

El informe adicional `jcop4-idopte-discovery-zA0uhMiU`, 14:40:29 UTC,
todavía usa formato v1 y solo contiene el marcador inicial; no añade una
secuencia APDU ni sustituye el nuevo resultado.

La búsqueda del comando encontró una fuente primaria:
[ANSSI-CC-2018/07](https://www.commoncriteriaportal.org/nfs/ccpfiles/files/epfiles/anssi-cc-2018-07fr.pdf),
sección 1.2.4, páginas 6–7, documenta **00 CA 01 00 00** como GET DATA
del nombre de la aplicación ChipDoc Lite. Es una aplicación anterior,
no la personalización costarricense actual; su interpretación es un
precedente para una consulta de identificación sin autenticación, **no una
predicción del contenido exacto ni prueba de compatibilidad completa**.
El informe público de ChipDoc 3.2 también enumera nombre/version/capabilities
como identificación por GET_DATA, pero no proporciona aquí los P1/P2.

Se amplía únicamente la condición del observador para permitir CA 01 00
con **CLA 00 y cinco bytes (caso-2 corto sin payload)**. Conserva la omisión
de todas las respuestas y los límites/bloqueos existentes. Tags vecinos,
comandos con datos de entrada, otros canales y secure messaging quedan
rechazados. No se agregan consultas de plataforma, INSTALL ni otros comandos
que aparecen en el informe de certificación.

Pasaron **30 comprobaciones del lanzador y 11 del observador**. La regresión
nueva permite la consulta básica y rechaza variantes con payload, otros
tags/canales, SM y caso truncado, manteniendo datos sintéticos fuera del log.
La integración con redirector PC/SC real y plugin local confirma dos GET
DATA permitidos (nombre/versión) antes de bloquear VERIFY, con transporte
exclusivamente ficticio. Compilado solo el observador; OpenSC sin cambios.

Siguiente intervención, misma JCOP4 y otros clientes cerrados, sin PIN:

```bash
make inspect/jcop4-idopte-discovery READER=0
```

Revisar el SW de esta consulta y el siguiente bloqueo, si existe. No ampliar
la lista por inercia ni volver al intento autenticado mientras VERIFY 6982
siga sin explicación.

## Identificación aceptada, SELECT EF bloqueado: 13 de octubre, 14:57:38 UTC

Informe `local/jcop4-idopte-discovery-fZcrQuOX/report.txt`, principal
`7c046dc`, OpenSC `3dfef6704`. Contexto y conexión PC/SC 0. Secuencia:

| Cabecera | Transporte | SW / resultado |
| --- | --- | --- |
| 00 CA 01 00 | 0 | 9000 |
| 00 CA 01 16 | 0 | 9000 |
| 00 A4 00 00 | 0 | 9000 |
| 00 A4 02 00 | No transmitido | Bloqueado por el observador |

Segunda conexión 0, pero el guard sigue detenido. Módulo 1, slot vacío,
observer_result=events_recorded. No hubo PIN, VERIFY ni firma. **Los 9000
confirman aceptación de esas operaciones, no el contenido de las respuestas**:
el observador las omite. Tampoco identifican cuál fue el archivo de la
selección inicial, aunque fue uno de los permitidos por el guard.

La nueva cabecera corresponde a selección de EF; no contiene el identificador
del archivo. Buscar el comando en el código público y documentación disponible
no permitió identificar su destino. No se habilita SELECT/READ arbitrario:
podría tratarse de un fichero diferente de los bootstrap públicos.

El formato **public_discovery_headers_v3** añade una única excepción a la
omisión de campos de comando: `blocked_select_ef=XXXX`, identificador de dos
bytes de **un SELECT ya bloqueado**. Solo se reconoce la forma exacta CLA 00,
INS A4, P1 02, P2 00, Lc 2 y siete u ocho bytes totales. Forma distinta:
`blocked_select_form=unsupported`, sin payload ni tamaño. No se registra
contenido de archivo, datos de autenticación ni respuestas. No se transmite
ese SELECT ni ningún fallback, y no se amplía la lista de permisos.

Pasaron **12 comprobaciones del observador y 30 del lanzador**. La regresión
nueva comprueba el FID sintético, bloqueo anterior al transporte y rechazo
del READ posterior. Un SELECT con payload sintético de credencial se marca
unsupported sin registrarlo. La integración con redirector PC/SC real también
confirma SELECT/fallback bloqueados, contador del transporte ficticio cero y
únicamente FID en el log privado. OpenSC sin cambios ni recompilación.

Siguiente intervención, otros clientes cerrados y la misma JCOP4 insertada:

```bash
make inspect/jcop4-idopte-discovery READER=0
```

No solicita PIN. Revisar el FID/forma del SELECT bloqueado antes de permitir
otra consulta; conservar el fallo como evidencia de los límites del observador.

## EF D003 identificado: 13 de octubre, 15:07:25 UTC

Informe `local/jcop4-idopte-discovery-FIyDDxFN/report.txt`, principal
`3423abc`, OpenSC `3dfef6704`. Inventario/compilación correctos; contexto y
conexión 0. GET DATA 00 CA 01 00, 00 CA 01 16 y SELECT 00 A4 00 00 ->
transporte 0/SW 9000. El SELECT 00 A4 02 00 quedó bloqueado con
**blocked_select_ef=D003**. No llegó a la tarjeta, no hay SW ni contenido
de ese archivo. Segunda conexión 0, pero guard detenido; Idopte 1, slot vacío,
events_recorded. Sin PIN ni autenticación.

La investigación encontró precedentes primarios:

- [ANS CPS4, sección 3.1.2, página 14](https://industriels.esante.gouv.fr/sites/default/files/media/document/ANS_CPSV4_0011_Guide_De_R%C3%A9f%C3%A9rence_v1_03.pdf)
  describe `3F00/D003` como EF SN transparente de identificación/serie,
  con lectura en contacto sin autenticación.
- [Especificación CIE 3.0, anexo A, página 24](https://www.cartaidentita.interno.gov.it/downloads/2021/03/cie_3.0_-_specifiche_chip.pdf)
  también sitúa EF SN ICC en D003 con lectura pública.

Son personalizaciones distintas. Respaldan investigar una lectura acotada,
**no prueban el contenido ni las ACL de D003 en esta JCOP4 de Costa Rica**.
El código local `pkcs15-esteid2018.c` lee ese FID como número de documento;
su significado debe resolverse por perfil, no por FID o generación JCOP.
No se usó documentación de procedencia incierta ni código privativo.

El observador conserva formato v3 y permite solo el SELECT EF observado:
CLA 00, INS A4, P1 02, P2 00, dos bytes D003, en una transacción que haya
confirmado MF. Ese contexto procede de un SELECT básico válido de 3F00/
AID raíz conocido, o una selección absoluta de bootstrap desde 3F00;
solo se acredita tras 9000 o 61xx. Selecciones fallidas descartan el contexto
anterior; SELECT de aplicación/otro canal y reconexión también lo retiran.
No se extiende el permiso a otras formas de SELECT de D003.

Tras seleccionar D003 correctamente se permite solo READ BINARY corto en
canal básico, sin SFI ni datos de entrada; las lecturas mantienen los límites
existentes de 256 bytes por APDU y 128 comandos por proceso. READ RECORD y
lecturas de otros canales no se habilitan para ese archivo. GET RESPONSE
solo tras 61xx de operación permitida, también básico para D003. El contenido,
serial y respuesta FCP no se registran. Una respuesta 69xx detiene el guard.
No se solicitan autenticación, PIN, componentes privados, escrituras ni
comandos administrativos.

Pasaron **14 comprobaciones del observador**; el lanzador conserva sus **30
comprobaciones previas** y no cambió. Las nuevas regresiones confirman MF
requerido, SELECT/READ permitidos, serial sintético omitido, formas/canales
no admitidos y retiro de permisos por fallos, cambio de aplicación y
reconexión. La integración con el redirector PC/SC real y transporte ficticio
ejecuta MF/D003/READ y bloquea VERIFY antes de llegar al transporte.
Se compiló solo el observador; driver/submódulo OpenSC intactos.

Siguiente intervención, misma JCOP4, otros clientes cerrados, dentro de nix develop:

```bash
make inspect/jcop4-idopte-discovery READER=0
```

Sin PIN. Puede volver a terminar con error del guard: revisar los SW de
selección/lectura y el siguiente comando bloqueado antes de otra consulta.
El fallo autenticado VERIFY 6982 sigue sin resolverse.

## Compilación detenida: 13 de octubre, 15:28:07 UTC

Informe `local/jcop4-idopte-discovery-4riCxCot/report.txt`, principal `e332630`,
OpenSC `3dfef6704`; ambos árboles limpios al comenzar. Las tres consultas de
inventario terminaron con 0 y el ATR coincide. La compilación del observador
terminó con **observer_build_status=1** porque pkg-config no encontró
`libpcsclite.pc`. No se cargó el módulo Idopte y no se creó public-headers.txt.
No hay una nueva respuesta APDU, lectura de D003 ni intento de autenticación.

El flake ya declara pcsclite y pkg-config. El informe no permite determinar
por qué esa dependencia estaba ausente del entorno de la terminal usada.
El constructor ahora comprueba la dependencia antes de compilar y da un
comando que entra explícitamente al entorno del flake. Además, separa la
consulta de flags del `read`: un error de pkg-config detiene el script en vez
de quedar oculto por el éxito de read. No se añaden rutas fijas ni fallback.

Pasaron **16 comprobaciones del observador/constructor** con transporte
ficticio. Dos nuevas simulan dependencia ausente y fallo al consultar flags;
ambas comprueban que el compilador no se invoca, y la primera verifica el
diagnóstico con nix develop. Lanzador sin cambios, sus 30 comprobaciones
previas siguen vigentes. No se modificó ni recompiló el driver.
`nix develop --offline -c pkg-config --exists libpcsclite` terminó con 0
en el entorno del agente, sin hardware ni carga de Idopte.

Siguiente intervención, con la misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-idopte-discovery READER=0
```

Sin PIN. Sigue pendiente comprobar SELECT/READ D003 y revisar el siguiente
bloqueo; el fallo autenticado VERIFY 6982 permanece sin resolver.

## Descubrimiento aceptado, control del lector bloqueado: 13 de octubre, 15:31:33 UTC

Informe `local/jcop4-idopte-discovery-qKJ16Ogh/report.txt`, principal `e240c20`,
OpenSC `3dfef6704`, ambos árboles limpios al comenzar. Inventario/ATR y
compilación del observador correctos. Contexto/conexión PC/SC devuelven 0.
Las ocho APDU transmitidas dieron transporte 0/SW 9000, en este orden:

```text
00 CA 01 00
00 CA 01 16
00 A4 00 00
00 A4 02 00
00 B0 00 00
00 A4 02 00
00 B0 00 00
00 CA 01 16
blocked=reader_control
```

El formato v3 no registra los FIDs de SELECT permitidos ni contenidos de
respuesta. Por ello no atribuir ambas lecturas a D003 ni interpretar contenido
o ACL por la cabecera sola. El primer rechazo es del observador a SCardControl,
**antes de llamar al transporte del lector**. No existe resultado del lector
ni código de ese control en esta captura. Una segunda conexión da 0 pero
no rehabilita el transporte. Idopte muestra slot vacío y sale 1; compatible
con la interrupción, no prueba de rechazo de tarjeta. No hubo PIN, VERIFY
ni operaciones de firma.

SCardControl envía operaciones al driver del lector, incluidas capacidades,
PIN-pad y otras extensiones. La [API de PC/SC](https://pcsclite.apdu.fr/api/group__API.html)
documenta dwControlCode como identificador de operación y los buffers de entrada/
salida por separado. [reader.h](https://pcsclite.apdu.fr/api/reader_8h.html)
define CM_IOCTL_GET_FEATURE_REQUEST como SCARD_CTL_CODE(3400), equivalente
a 42000D48 en pcsc-lite. Es un candidato de identificación, **no evidencia
de que Idopte pidiera ese control**.

El observador v4 conserva todos los permisos de v3 y sigue bloqueando
**todos** los controles. Añade solo `blocked_control_code` hexadecimal y
`blocked_control_kind`: feature_request_empty si el código estándar usa la
forma sin entrada, feature_request_unsupported si lleva entrada, unknown
para otros códigos. No lee ni registra el buffer, su tamaño real o la respuesta.
Después del bloqueo no registra más controles ni permite APDU de fallback.
No se activa una consulta de capacidades por hipótesis.

Pasaron **17 comprobaciones del observador/constructor** con transporte
ficticio, mediante `nix develop --offline -c make test/pcsc-public-observer`.
La nueva regresión cubre formas vacías/con entrada, códigos desconocidos,
respuesta de bloqueo sin bytes, ausencia de contenido privado y detención
persistente. La integración con redirector real y plugin local prueba que
el control estándar vacío y con payload no alcanzan el backend ficticio,
y que sus buffers sintéticos no aparecen en el registro. Lanzador sin cambios,
sus 30 comprobaciones previas siguen vigentes. Driver/submódulo sin cambios.

Siguiente intervención, misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-idopte-discovery READER=0
```

Sin PIN. Se espera que el control siga bloqueado; la nueva captura lo
identificará antes de decidir si admite una consulta pública acotada.
VERIFY 6982 continúa pendiente y no se solicita otro intento autenticado.

## Control de capacidades identificado: 13 de octubre, 15:35:59 UTC

Informe `local/jcop4-idopte-discovery-eIa1G5Gt/report.txt`, principal `526338a`,
OpenSC `3dfef6704`, ambos árboles limpios al comenzar. Inventario/ATR y
compilación correctos. Contexto/conexión 0 y las mismas ocho APDU permitidas
con transporte 0/SW 9000. Primer bloqueo:

```text
blocked=reader_control
blocked_control_code=42000D48
blocked_control_kind=feature_request_empty
```

Ahora sí identifica CM_IOCTL_GET_FEATURE_REQUEST de PC/SC Part 10, sin entrada.
El observador v4 lo detuvo antes de llegar al lector: **no hay respuesta de
capacidades ni resultado de transporte de ese control**. Segunda conexión 0
sin rehabilitar transporte. Módulo 1, slot vacío, events_recorded; compatible
con la interrupción del guard. No hubo PIN, VERIFY ni firma.

La [documentación oficial CCID](https://github.com/LudovicRousseau/CCID/blob/master/SCARDCONTROL.md)
identifica SCARD_CTL_CODE(3400) como descubrimiento de capacidades Part 10.
La [implementación pública IFDHControl](https://github.com/LudovicRousseau/CCID/blob/master/src/ifdhandler.c)
construye una lista TLV de funciones disponibles del lector, distinta de los
controles que ejecutan esas funciones. Consultar esa lista no autentica un
PIN. El hecho de que anuncie PIN-pad/PACE no demuestra que Idopte los use.

El observador v5 añade exclusivamente ese control **sin entrada** a la lista
permitida. Siempre envía NULL/0 al backend, incluso si el cliente proporciona
un puntero con longitud cero. Requiere buffer/contador de salida válidos,
log privado y primer handle; comparte con APDU el presupuesto de 128 eventos.
Usa buffer interno de hasta 256 bytes, limitado además por el buffer del
cliente. Copia al cliente solo una respuesta exitosa que quepa en el buffer;
fallo, transporte ausente o tamaño inválido detienen sin copiar ni reintentar.
No cambia el contexto de archivos tras una consulta exitosa.

Registra únicamente public_control_code y control_transport, sin TLV,
capacidades, longitud ni buffers. **Las funciones anunciadas no crean permisos**:
todos los demás controles, incluidos PIN-pad, escape y PACE, permanecen
bloqueados incluso con entrada vacía; SCardSetAttrib, VERIFY, MSE y firma
también siguen bloqueados. El lanzador reconoce la consulta como evento de
control, sin afirmar que sea una APDU.

Pasaron **20 comprobaciones del observador/constructor y 31 del lanzador**,
sin hardware. La suite del observador se ejecutó desde nix develop --offline
y compiló la biblioteca real. Cubre límite de respuesta, buffer inválido,
fallo/símbolo ausente, privacidad, contexto, límite de eventos/handle, estado
detenido y ausencia de log. Con redirector real/plugin local y transporte
ficticio se permite la consulta, se anuncia un IOCTL de PIN sintético y se
comprueba que ese IOCTL con/sin datos y VERIFY no alcanzan el backend.
La nueva regresión del lanzador reconoce una captura de solo control público
sin confundirla con conexiones solas o una APDU. Driver/submódulo intactos.

Siguiente intervención, misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-idopte-discovery READER=0
```

Sin PIN. Revisar control_transport y la primera operación bloqueada antes de
permitir cualquier otra consulta. VERIFY 6982 sigue sin resolverse.

## Capacidades consultadas, EF 4141 bloqueado: 13 de octubre, 15:45:03 UTC

Informe `local/jcop4-idopte-discovery-OUj06FEQ/report.txt`, principal `86e8ba9`,
OpenSC `3dfef6704`, ambos árboles limpios al comenzar. Inventario/ATR,
compilación y conexión correctos. La consulta estándar 42000D48 obtuvo
**control_transport=0**; su respuesta no se registra, por lo que no permite
afirmar qué capacidades anunció el lector.

Las trece APDU transmitidas dieron transporte 0/SW 9000. Después del control
hubo SELECT EF/READ BINARY, SELECT AID, SELECT EF/READ BINARY y el primer bloqueo:

```text
blocked_header=00A40200
blocked_select_ef=4141
```

El SELECT de 4141 no llegó a la tarjeta; no hay SW, contenido ni ACL para
interpretar. Los FIDs/AID permitidos se omiten, así que no atribuir lecturas
a un archivo ni ese SELECT a una aplicación concreta solo por las cabeceras.
Segunda conexión 0 sin rehabilitar transporte; módulo 1, slot vacío,
events_recorded, compatible con la interrupción del guard. Sin PIN, VERIFY
ni firma.

La búsqueda en el código local y las fuentes públicas consultadas no permitió
identificar 4141. No es evidencia de que sea privado ni de que sea público,
AODF o CAN. Se conservan los permisos del observador v5, sin añadir ese archivo.
El siguiente paso cambia a consultar **el directorio público conocido 5031**,
para determinar si lo referencia y con qué tipo de directorio.

`make inspect/jcop4-odf-map` compila únicamente el helper card-public-info
contra la biblioteca OpenSC existente. Tras inventario, exige ATR/AID medidos
y que EF.DIR anuncie ese AID. Bajo una transacción, selecciona esa aplicación
y 5031 con SELECT EF básico/P2 0C sin solicitar FCP. Un único READ BINARY corto
de hasta 256 bytes desde offset cero, sin SFI, GET RESPONSE ni reintentos.
Rechazos de SELECT detienen antes de READ; solo acepta lectura 9000/6282 y
longitud de respuesta entre 1 y 256 bytes.

Usa el decodificador ASN.1 libre de OpenSC para rutas de ODF; admite hasta
32 registros de las nueve clases de directorio PKCS#15. Solo imprime la clase
y ruta después de validar el bloque, no DER bruto. Rechaza truncamientos,
alternativas inline/extendidas, rutas vacías/impares y relleno no uniforme.
El resultado se limita a las referencias del primer bloque, no prueba
completitud general del archivo ni autoriza leer un destino.

**No hace binding PKCS#15 ni enumera objetos/certificados**, y no selecciona
ninguna referencia obtenida, incluido 4141. No Idopte, VERIFY, PIN, MSE,
firma, componentes privados ni comandos administrativos. Mantiene la
configuración aislada sin cachés/debug/trazas externas y stdin vacío;
resultados privados en local/jcop4-odf-map-*/report.txt.

Pasaron **40 comprobaciones del helper y 33 del lanzador**, sin hardware.
Cinco grupos nuevos del helper cubren ODF tipado y sin lecturas referenciadas,
selección/aplicación fallida, READ fallido o incompleto sin reintento,
ATR/AID conocidos y ASN.1/padding/límite de registros. Dos nuevas del lanzador
cubren entorno aislado/entrada vacía/informe privado y fallo sin anuncio de
éxito. Helper real compilado contra OpenSC offline; no se modificó ni recompiló
el driver ni el observador. Sus 20 comprobaciones anteriores siguen vigentes.

Siguiente intervención, misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-odf-map READER=0
```

Sin PIN. Revisar si 4141 aparece como referencia tipada antes de decidir
si corresponde alguna selección/lectura posterior. No repetir el descubrimiento
Idopte ni intentar autenticar por inercia. VERIFY 6982 continúa pendiente.

## Referencias del ODF: 13 de octubre, 15:58:49 UTC

Informe privado `local/jcop4-odf-map-eAjoi575/report.txt`, principal `458573f`
y OpenSC `3dfef6704`; ambos árboles limpios al comenzar. Inventario/ATR,
compilación, conexión y selección de la aplicación correctos. SELECT 5031 y
READ BINARY dieron transporte 0/SW 9000. Se decodificaron cuatro referencias:

| Tipo PKCS#15 | Ruta |
| --- | --- |
| Objetos de autenticación | 7001 |
| Metadatos de llaves privadas | 7002 |
| Llaves públicas | 7004 |
| Certificados | 7005 |

No se siguieron rutas; unlock y proceso 0. No binding, PIN ni autenticación.
7002 designa un directorio de metadatos, no bytes de llave privada. **4141
no figura entre las referencias del bloque leído**. No demuestra ausencia
general, completitud del ODF ni propósito del archivo. La búsqueda pública
previa no lo identificó; no asumir AODF, CAN ni permisos de lectura.

### Consulta limitada a SELECT 4141

Se prepara `make inspect/jcop4-file4141` para comprobar existencia/metadatos
de control del único EF observado bloqueado, bajo la aplicación conocida.
El [SELECT de OpenSC ISO 7816](https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/iso7816.c)
procesa FCP/FCI como información de control del archivo; obtenerla no requiere
READ del contenido. No se permite el archivo a Idopte ni se busca una serie
de FIDs desconocidos. Este diagnóstico independiente:

1. Mantiene default driver, comprobación del ATR conectado, inventario público
   EF.ATR/EF.DIR y exige el AID conocido anunciado, sin binding PKCS#15.
2. Selecciona esa aplicación y envía **00 A4 02 00 02 41 41 00**, SELECT EF con
   respuesta de control, máximo 256 bytes. No READ BINARY/RECORD ni seguimiento
   de rutas, no consultas de credenciales ni operaciones administrativas.
3. Registra transporte/SW. 9000 sin metadatos es diagnóstico válido; 6A82
   significa no encontrado **bajo esa aplicación/contexto**, sin probar ausencia
   global ni qué contexto empleó Idopte. Otros SW, incluido 61xx/6Cxx, transporte
   o respuesta inválida detienen sin GET RESPONSE/reintentos.
4. Si recibe FCP/FCI válido, resume únicamente tags directos 82 (estructura)
   y 80 (tamaño); verifica 83=4141 cuando esté presente. Omite nombres, ACL,
   campos propietarios y respuesta bruta. No interpreta plantillas anidadas
   como esos tags directos ni acredita acceso de lectura por selección exitosa.

No Idopte, VERIFY, PIN, MSE ni firma. Debug 0, stdin vacío, configuración local
sin cachés, informes privados en local/jcop4-file4141-*/report.txt. Código y
permisos del observador v5 intactos; seguirá bloqueando el SELECT de 4141.
No cambios ni recompilación del driver/submódulo.

Pasaron **45 comprobaciones del helper y 35 del lanzador** con transporte
ficticio, incluyendo bytes exactos/único SELECT, ausencia de READ/binding,
rechazo de otros ATR/AID/contextos, SW negativos, truncamientos, contradicciones,
omisión de campos identificables y propagación de fallos sin éxito anunciado.
Helper real compilado contra OpenSC offline. Log C:
`build/opensc-offline/card-public-info-tests.log`. No hardware por el agente;
las 20 comprobaciones anteriores del observador siguen vigentes.

Siguiente intervención, misma JCOP4 y firmador/otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-file4141 READER=0
```

Revisar el informe antes de decidir cualquier lectura o ampliación del
observador. No repetir intentos de PIN; VERIFY 6982 permanece sin resolver.

## SELECT 4141 confirmado: 13 de octubre, 16:12:31 UTC

Informe privado `local/jcop4-file4141-Kzi6aI8S/report.txt`, principal `e4c5d1e`
y OpenSC `3dfef6704`; ambos árboles limpios al comenzar. Inventario, compilación,
contexto/conexión y selección de la aplicación correctos. Resultados:

```text
file4141_select_transport=0
file4141_select_sw=9000
file4141_structure=transparent
file4141_size=400
file4141_result=selected_metadata_only_no_read
unlock_result=0
exit_status=0
```

El archivo existe bajo el AID ChipDoc conocido. No se leyó contenido, ni se
hizo binding/autenticación. **Su propósito, contenido y política de acceso
siguen pendientes**; tamaño/estructura y selección exitosa no justifican
leerlo. ODF no lo referencia en el bloque previamente leído. Nuevas búsquedas
públicas tampoco identificaron el FID; no interpretarlo como AODF/CAN.

### Observador v6: selección de metadatos sin permiso de lectura

El SELECT ISO del archivo concreto ya se interpretó y confirmó en hardware.
Se permite ahora ese único SELECT a Idopte para observar su siguiente operación,
sin habilitar el contenido del EF ni una selección general de archivos:

- Solo CLA 00/INS A4/P1 02/P2 00, Lc 2, FID 4141 y total de 7 u 8 bytes,
  bajo AID ChipDoc conocido previamente seleccionado en canal básico con 9000.
- Se conserva la aplicación al seleccionar un EF bootstrap relativo conocido
  con éxito. SELECT MF/raíz/ruta absoluta, canal diferente, SELECT fallido o
  pendiente y conexión/reconexión descartan su elegibilidad. Un 61xx en SELECT
  de aplicación no acredita todavía ese contexto para la excepción.
- SELECT 4141 revoca el permiso READ del archivo anterior y **no habilita
  READ BINARY/RECORD**. GET RESPONSE sigue limitado a 61xx de comandos permitidos
  y no habilita lectura ni autenticación. Respuestas completas siempre omitidas.
- Se agregan etiquetas constantes `public_select=known_chipdoc_application`,
  `known_root_application` y `known_4141_metadata_only`. Se obtienen solo de
  coincidencias con constantes públicas ya permitidas; no guardan AID/FID
  arbitrarios ni buffers. Preceden al transporte, por lo que describen un
  intento permitido: consultar su SW antes de afirmar éxito.
- Se mantienen los demás permisos, límites y bloqueo definitivo de v5. No
  VERIFY, PIN, MSE, PSO, SM, escritura ni controles del lector salvo la consulta
  vacía de capacidades. No usar este observador con firmador ni login/firma.

Pasaron **25 comprobaciones del observador/constructor y 35 del lanzador**,
con transporte ficticio, incluida delegación PC/SC real y plugin RTLD_LOCAL.
La integración permite aplicación/SELECT 4141 pero aborta si READ o PIN alcanzan
el backend falso; otros contextos/formas y pérdida de contexto también se
comprueban. La biblioteca de diagnóstico real compiló. No se cargó Idopte ni
se usó hardware por el agente. Helper y driver/submódulo sin cambios; las 45
comprobaciones previas del helper siguen vigentes.

Siguiente intervención, misma JCOP4 y firmador/otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-idopte-discovery READER=0
```

Sin PIN. Un error de Idopte si intenta READ de 4141 es esperable. Revisar
etiquetas, transporte/SW y primer bloqueo; no conceder lectura ni reintentar
PIN por inercia. VERIFY 6982 sigue sin resolver.

## Idopte intenta leer 4141: 13 de octubre, 16:30:07 UTC

Informe privado `local/jcop4-idopte-discovery-mEpNvPwv/report.txt`, principal
`e30c61e`, OpenSC `3dfef6704`; ambos árboles limpios al comenzar. Inventario,
compilación y contexto/conexión correctos. El observador v6 confirmó:

```text
public_header=00A4040C
public_select=known_chipdoc_application
transport=0
sw=9000
# SELECT/READ bootstrap permitidos: transporte 0/SW 9000
public_header=00A40200
public_select=known_4141_metadata_only
transport=0
sw=9000
blocked_header=00B00000
```

En total, catorce APDU permitidas dieron 0/9000 y la consulta de capacidades
42000D48 dio transporte 0. Respuestas y FIDs bootstrap omitidos; no atribuir
la lectura bootstrap a un archivo específico. El **READ BINARY de 4141 no
llegó a la tarjeta**; no hay SW ni contenido de esa operación. La segunda
conexión obtuvo 0 pero el transporte siguió detenido. Idopte salió 1 y se
conservaron events_recorded. Es compatible con el bloqueo deliberado; no es
el rechazo VERIFY 6982 ni una prueba de cómo autentica Idopte. No PIN,
VERIFY, MSE ni PSO. No repetir esta captura ni conceder READ por inercia.

### Resumen de seguridad de los metadatos SELECT

Se amplía el helper de `make inspect/jcop4-file4141`, sin cambiar sus comandos:
mismo inventario EF.ATR/EF.DIR, aplicación conocida y SELECT 4141, sin lectura
de contenido, binding, Idopte ni autenticación. El informe previo omitió estos
atributos; repetir SELECT obtiene ahora un resumen nuevo, no una variante del
protocolo ni un reintento tras rechazo. Solo emite:

- `file4141_security_format`: forma compacta directa, referenciada, expandida,
  dependiente del perfil, control anidado, combinación o ninguna reconocida
  entre los tags directos. No afirmar ausencia general por este último caso.
- `file4141_standard_compact_read`: para una única 8C directa y descriptor de
  EF transparente ordinario, reconoce `always` únicamente si los grupos ISO
  estándar anuncian SC 00 para READ. Otros valores se resumen como
  `nonzero_uninterpreted`; ausencia de READ, alternativas diferentes y AM
  propietario permanecen explícitos y sin resolver. Múltiples atributos/formas,
  descriptor distinto o anidamiento dejan la condición sin interpretar.

El precedente de [ETSI TS 102 221, secciones 9.2.4–9.2.5](https://www.etsi.org/deliver/etsi_ts/102200_102299/102221/16.05.00_60/ts_102221v160500p.pdf)
describe los tags 8C/8B/AB y grupos compactos de AM/SC según ISO 7816-4;
READ ocupa el bit inferior del AM para un EF. Es referencia de formato,
**no demuestra que la personalización CR adopte el perfil UICC**. No trasladar
interpretaciones de PIN, CAN, SE ni A1/9C de JCOP3. Los grupos compactos
estándar vacíos/truncados detienen sin imprimir una condición. No se guardan
ACL, SCB, referencias, nombres, valores propietarios ni respuestas brutas.

El resultado es un **resumen de un atributo, no la política global del archivo**.
Incluso `always` no identifica el contenido como público ni concede acceso de
lectura; el observador conserva el bloqueo de READ 4141. No aplica políticas a
OpenSC ni modifica el driver.

Pasaron **49 comprobaciones del helper** con transporte ficticio, incluida
la ausencia de READ/binding/autenticación, múltiples grupos/alternativas,
truncamientos, descriptores diferentes y omisión de datos. Helper real compilado
contra OpenSC offline; log `build/opensc-offline/card-public-info-tests.log`.
Lanzador y observador sin cambios; sus 35/25 comprobaciones previas siguen
vigentes. No hardware por el agente, driver/submódulo intactos.

Siguiente intervención, misma JCOP4 y firmador/otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-file4141 READER=0
```

Sin PIN ni READ del archivo. Revisar el resumen antes de decidir nuevos pasos.
Propósito de 4141 y requisito pendiente de VERIFY 6982 siguen sin resolverse.

## Seguridad de 4141 sin interpretar: 13 de octubre, 16:38:38 UTC

Informe privado `local/jcop4-file4141-o5gvxntR/report.txt`, principal `3f894c6`
y OpenSC `3dfef6704`; ambos árboles limpios al comenzar. Inventario, compilación,
contexto/conexión y aplicación correctos. Resultado:

```text
file4141_select_transport=0
file4141_select_sw=9000
file4141_structure=transparent
file4141_size=400
file4141_security_format=profile_uninterpreted
file4141_standard_compact_read=uninterpreted
file4141_result=selected_metadata_only_no_read
unlock_result=0
exit_status=0
```

La consulta terminó correctamente; no es un nuevo rechazo de tarjeta. No hubo
READ, binding, PIN ni autenticación. **No se obtuvo una condición READ
interpretable**. El helper agrupa los tags directos 86/A1/9C bajo la misma
etiqueta: acredita presencia de al menos uno, sin distinguir cuál, su número,
estructura ni valores. No afirmar que se observó A1 específicamente. Algunas
estructuras podrían contener codificaciones estándar anidadas que este resumen
no interpreta; no se concluye que todos sus atributos sean propietarios.

El resultado no demuestra protección del archivo ni acceso libre, y tampoco
acredita PACE/CAN obligatorio o la causa del VERIFY 6982. No aplicar la política
JCOP3. Las búsquedas nuevas en documentación pública y nombres del módulo
instalado no identificaron el archivo; no es prueba de ausencia de documentación
ni de soporte. **READ 4141 sigue bloqueado en el observador v6**. El error
Idopte mEpNvPwv corresponde a ese bloqueo antes de transporte; el rechazo
VERIFY autenticado de 5Dpadr1N es evidencia distinta y anterior.

En esa revisión se pidió confirmar si **esta misma JCOP4 ya produjo una
firma válida con el Idopte instalado**, recordando una prueba anterior. No se
solicita otra firma, PIN ni captura autenticada. La consulta pública Idopte
6aaJ5sIO confirmó reconocimiento de ChipDoc v3.2/PIN inicializado, pero no
firma. Esa confirmación permite delimitar el diagnóstico de funcionamiento
antes de modificar autenticación o atribuir un fallo al driver libre.

Si ya funcionó, la investigación debe identificar el archivo/perfil o preparar
una observación que proteja credenciales; la confirmación no autoriza por sí
sola READ de archivos desconocidos ni nuevos intentos de PIN. Si no hay prueba
previa o falló, revisar esa situación y su evidencia antes de decidir el
protocolo siguiente. No contactar terceros ni capturar APDU autenticadas con
pcsc-spy estándar sin un alcance autorizado y protección del PIN.

Esta revisión modifica solo documentación; no helper, observador, lanzador,
driver ni submódulo. Las 49/25/35 comprobaciones anteriores siguen vigentes y
no se repitieron por estos cambios. No hardware ni commits por el agente.
No repetir SELECT/descubrimiento como primer paso. JCOP4 aún no firma con
OpenSC; RSA 3072 y la arquitectura final siguen pendientes.

## Firma previa con Idopte y resumen estructural ampliado

El usuario confirmó que **esta misma JCOP4 ya firmó satisfactoriamente con
Idopte instalado**. Esa prueba previa establece una referencia de funcionamiento
según su testimonio. No se ejecutó ni verificó otra firma/PDF por el agente.
No demuestra qué protocolo usa Idopte, qué condición faltó en VERIFY 6982 ni
que el intento OpenSC hubiera usado el PIN correcto. No solicitar otra firma
para confirmar este dato ni leer 4141 por esa sola confirmación.

El resumen anterior agrupa 86/A1/9C y no permite distinguir la estructura.
El código iasecc existente busca 8C dentro de A1 antes de buscar una 8C directa
([implementación local](../vendor/opensc/src/libopensc/card-iasecc.c)). Eso es
un precedente de organización de TLV, **no prueba de que JCOP4 CR use ese
perfil ni sus reglas**. Se amplía solo el diagnóstico --file4141, manteniendo
exactamente su inventario y SELECT; no READ, binding ni autenticación:

- `file4141_security_direct_tags`: presencia de tags con nombres fijos
  8C/8B/AB/86/A1/9C/62/6F, sin valores ni tags arbitrarios.
- `file4141_a1_structure`: un único A1 con TLV inmediatos válidos,
  opaque_uninterpreted o multiple_uninterpreted. Si es opaco/malformado se
  descarta toda evidencia parcial de sus hijos; no se exploran niveles siguientes.
- `file4141_a1_security_tags`: los mismos nombres fijos presentes como hijos
  inmediatos; `file4141_a1_other_tags` informa solo presencia de otros tags,
  sin sus identificadores, referencias, valores ni tamaños.
- `file4141_a1_8c_read_encoding`: resumen de la **codificación** de una sola
  8C inmediata para descriptor transparente ordinario. Usa los grupos compactos
  estándar ya documentados arriba; no resuelve condiciones no nulas. Duplicados,
  datos inválidos o descriptor distinto quedan uninterpreted. No identifica
  una ACL de contacto ni una política de lectura del perfil.

Se conservan security_format y standard_compact_read anteriores. Una 8C
anidada **no** se convierte en atributo directo ni cambia esos resultados.
Incluso read_encoding=always describe solo bytes de ese atributo: sin conocer
su papel en el perfil y el propósito de 4141 no concede READ. No se exponen
buffers, ACL, SCB, PIN, longitudes reales de credenciales o campos propietarios.
Ningún comando nuevo, bucle APDU, permiso del observador o driver se agrega.

Pasaron **54 comprobaciones del helper** con transporte ficticio y compilación
real contra build/opensc-offline. Las nuevas regresiones cubren distinción de
tags, codificación anidada sin política global, privacidad de valores, A1
malformado/duplicado, compactos inválidos/duplicados, ausencia de descriptor y
contenedores más profundos sin interpretar. Log privado de pruebas:
`build/opensc-offline/card-public-info-tests.log`. Observador v6 y lanzador
sin cambios; sus 25/35 comprobaciones anteriores siguen vigentes, sin repetirlas.
OpenSC/submódulo intactos; no hardware, Idopte, PIN ni commits por el agente.
Principal c244218 al comenzar; OpenSC 3dfef6704.

Se requiere una nueva selección para recoger la estructura que el informe
anterior no conservó, con la misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-file4141 READER=0
```

Sin PIN. Compila únicamente el helper. Revisar los campos nuevos antes de
decidir el siguiente paso. **READ 4141 permanece bloqueado**; no repetir
captura Idopte ni intentar autenticación todavía. VERIFY 6982, propósito del
archivo, integración JCOP4 y transporte RSA 3072 siguen pendientes.

## Tag directo 86 confirmado: 13 de octubre, 16:53:35 UTC

Informe privado `local/jcop4-file4141-9gvwdH1n/report.txt`, principal `e4a227e`
y OpenSC `3dfef6704`; ambos Git limpios al comenzar. Inventario/ATR, compilación,
conexión y selección de aplicación correctos. Resultados:

```text
file4141_select_transport=0
file4141_select_sw=9000
file4141_structure=transparent
file4141_size=400
file4141_security_format=profile_uninterpreted
file4141_security_direct_tags=86
file4141_standard_compact_read=uninterpreted
file4141_result=selected_metadata_only_no_read
unlock_result=0
exit_status=0
```

La consulta funcionó. **86 es la única forma de seguridad directa reconocida**
por el conjunto fijo de tags del helper; no aparecen A1/9C/8C/8B/AB/62/6F.
Eso no acredita un único atributo físico, ausencia de otros tags omitidos ni
una política concreta. No hay un A1 cuyo contenido anidado se pueda resumir
en este informe. No se registraron valores de seguridad ni se leyó contenido del EF; no se
ejecutó binding, PIN, autenticación o firma.

[El parser ISO de OpenSC](../vendor/opensc/src/libopensc/iso7816.c) conserva
86 como atributo de seguridad mediante sc_file_set_sec_attr, sin interpretar
su política. La presencia de ese tag no identifica una aplicación particular.
No trasladar el formato de ACL de otro driver; tampoco identifica 4141 como
archivo público, CAN, llave o parámetros PACE. El VERIFY autenticado 6982 de
5Dpadr1N permanece sin causa resuelta. El error de Idopte mEpNvPwv fue un READ
bloqueado por el observador antes de llegar a la tarjeta, evidencia distinta.

Las búsquedas adicionales no identificaron 4141 ni la codificación de 86 para
esta personalización. No prueba que esa documentación no exista. Se identifican
los manuales técnicos concretos que podrían resolverlo en la bibliografía
9.2 (página 67) de la
[cible de seguridad NXP/ANSSI-CC-2025/03](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03-cible.pdf):

| Documento | Referencia | Revisión |
| --- | --- | --- |
| ChipDoc 3.2 User Guide Manual | 678510 | 1.0, 22 de abril de 2021 |
| ChipDoc 3.2 Crypto Guide | CDv3.2_1_03210_ChipDoc3.2_Crypto_Guide | 1.0, 29 de abril de 2021 |
| ChipDoc 3.2 SSCD Personalization Guide | 678712 | 1.2, 2 de diciembre de 2024 |

**Solo se obtuvo esa referencia bibliográfica, no el contenido de los manuales.**
Tampoco acredita que la tarjeta CR use la configuración certificada descrita.
Se pidió al usuario indicar si tiene acceso a esos documentos o a la
especificación del perfil BCCR. La información necesaria es la función de 4141,
la codificación de sus atributos 86, el canal/contexto requerido antes de VERIFY
y la ubicación documentada de sus parámetros públicos. No se solicita otra
firma ni PIN; ya confirmó firma previa satisfactoria con Idopte.

No repetir SELECT/descubrimiento, explorar otro archivo o reintentar PIN por
inercia. **READ 4141 permanece bloqueado**. Sin documentación, habrá que decidir
otro alcance concreto de observación que proteja credenciales; este resultado
no lo autoriza por sí solo. No contactar terceros ni capturar una sesión
Idopte autenticada con pcsc-spy estándar.

Cambios solo en documentación. Helper/observador/lanzador/driver/submódulo
intactos; sus 54/25/35 comprobaciones anteriores siguen vigentes, sin repetir
suites por estos cambios. No hardware, carga de Idopte, PIN ni Git por el agente.
JCOP4, RSA 3072 y arquitectura final siguen pendientes.

## Captura sin PIN para pcsc-spy: preparación

El usuario propuso observar pruebas sin PIN y aportó la
[cible NXP/ANSSI-CC-2025/03](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03-cible.pdf).
Su sección 1.3.2.1.3 contempla PACE con PIN/CAN/MRZ; ese precedente no demuestra
la configuración CR ni identifica 4141 o sus atributos 86. Los manuales de
la bibliografía siguen sin haberse obtenido. No hace falta esperar su obtención
para examinar una respuesta de control pública ya observada.

El [pcsc-spy estándar](https://pcsclite.apdu.fr/pcsc-spy.1.html) muestra llamadas
y buffers. Su
[implementación](https://pcsclite.apdu.fr/api/libpcscspy_8c_source.html) registra
entrada a SCardTransmit antes de delegar; ubicarlo antes de un guard también
registraría el payload de un VERIFY bloqueado. Se conserva el bloqueo previo
al registro. El nuevo **make inspect/jcop4-idopte-spy** usa el guard existente
para emitir exclusivamente una traza compatible de **SELECT 4141 permitido**
y su respuesta de control; el programa pcsc-spy real decodifica ese archivo.
No se carga libpcscspy ni usa un FIFO, LD_PRELOAD o modificación de HOME.
No es una captura completa del proceso: handles/PCI/prefijo de hilo son
placeholders normalizados, y tiempos/APDU/respuesta/resultado son observados.

Este modo permite registrar el FCP completo, incluidos los atributos 86 que
el resumen anterior omitía, para interpretarlos. No lee el contenido de 4141
ni cambia los permisos del observador. No registra comandos rechazados, otras
respuestas de SELECT/GET DATA, lecturas bootstrap, serie D003 ni capacidades.
La coincidencia de captura es solo 00 A4 02 00 / Lc 2 / FID 4141 / total 7 u 8,
después de que el guard haya confirmado contexto y permitido transmisión.
Respuesta máxima 258 bytes incluyendo SW; error de transporte, respuesta
inválida para el buffer o superior al límite omite los datos. No añade comandos
ni GET RESPONSE para completar una respuesta. Un 61xx puede dar control parcial;
no afirmar completitud o interpretar política global por esa captura.

La herramienta pública sigue --show-info --list-slots, sin solicitar sesión,
objetos, login, PIN o firma. Stdin vacío, debug 0 y configuración aislada.
VERIFY incluso vacío, MSE/PSO/SM/autenticación/escrituras/lecturas desconocidas
continúan bloqueados antes del transporte; no pueden filtrarse al archivo.
Tras primer bloqueo, reconectar no habilita otras APDU. READ 4141 sigue prohibido.

Capturas privadas (directorio 0700, archivos 0600):

- report.txt y public-headers.txt: resultados/cabeceras v6, sin respuestas.
- select4141-spy.raw: solo el intercambio de control en formato compatible.
- select4141-spy.txt: representación decodificada por pcsc-spy, sin colores.

El FCP puede contener nombres/campos identificables. No volcar la captura
completa al chat ni Git; al revisar extraer solo estructura, tamaño, FID,
atributo 86 y SW. No afirmar ausencia de datos sensibles por no haber PIN.
El modo solicita raw nuevo O_EXCL/NOFOLLOW/0600; si falta/collide no transmite.
Fallo de escritura detiene consultas posteriores. El nuevo env se elimina
antes del inventario y en los demás modos; se activa solo dentro de Idopte.
Falta de pcsc-spy/Python 3, captura vacía o fallo de decodificación detiene
sin fallback. También se detectan errores de hilo con salida de proceso 0.

Se agregó Python 3 al flake, sin cambiar lock: el script de pcsclite.dev
trae shebang /usr/bin/python3, que no existe en este NixOS. Se ejecuta con
el intérprete del entorno, sin editar el paquete. **Estado Idopte y estado del
decodificador se conservan por separado**. El READ posterior seguirá bloqueado,
así que Idopte puede salir 1 aun si la captura/decodificación funcionan.
Revisar spy_result/spy_decode_status y el SELECT antes de repetir por ese error.

Pasaron **30 comprobaciones del observador/constructor/decoder** y **40 del
lanzador**, sin hardware. Incluyen transporte falso, exclusión READ/PIN, log
privado/fallos/symlink, pcsc-spy real y redirector real con plugin RTLD_LOCAL
hacia transporte falso. Biblioteca de diagnóstico real compilada. Helper,
OpenSC/driver/submódulo sin cambios; las 54 del helper previas siguen vigentes.
No Idopte, tarjeta, PIN o Git por el agente. Principal 69a4310/OpenSC 3dfef6704
al comenzar. Soporte JCOP4 y RSA 3072 siguen pendientes.

Siguiente intervención, misma JCOP4 y otros clientes cerrados:

```bash
nix develop -c make inspect/jcop4-idopte-spy READER=0
```

Compila solo diagnóstico. No introducir PIN, abrir firmador ni repetir firmas;
examinar primero la respuesta de control de SELECT. No autoriza nuevos comandos,
READ de archivos desconocidos ni captura de autenticación por esa propuesta.

## Captura revisada: 13 de octubre, 17:33:49 UTC

Informe privado `local/jcop4-idopte-spy-wYs4iWG8/report.txt`, principal
`4058fdf`, OpenSC `3dfef6704`; ambos árboles limpios al comenzar la revisión.
Compilación/inventario correctos: lector 0 **Bit4id miniLector-s 00 00**,
con el ATR JCOP4 conocido. Este lector difiere del OMNIKEY de las consultas
históricas; no inferir capacidades de autenticación por su nombre.
Las catorce APDU permitidas devolvieron
transporte 0/SW 9000 y la consulta de capacidades transporte 0. Resultados:

```text
module_exit_status=1
spy_decode_status=0
spy_result=select4141_control_data_only
observer_result=events_recorded
```

**La captura funcionó**. Idopte intentó READ BINARY 00 B0 00 00 después de
SELECT 4141; el observador lo bloqueó antes del transporte. No existe SW ni
contenido de ese READ. La segunda conexión tuvo resultado 0, sin rehabilitar
transporte. Es el límite previsto del diagnóstico, no un nuevo fallo de PIN
ni un rechazo de tarjeta a la lectura. No repetir para cambiar el estado del
módulo; el intercambio buscado ya se recuperó.

Se extrajo solo el SELECT y los campos de control previstos de la captura,
sin volcarla completa ni copiar identificadores del inventario:

| Campo observado | Valor | Interpretación delimitada |
| --- | --- | --- |
| Comando SELECT | `00 A4 02 00 02 41 41 00` | Archivo 4141 bajo la aplicación conocida |
| SW | `9000` | SELECT aceptado |
| Plantilla exterior | `6F` | FCI en esta respuesta; no plantilla exterior 62 |
| Tamaño, tag 80 | `0190` | 400 bytes anunciados |
| Descriptor, tag 82 | `010000` | EF transparente |
| Identificador, tag 83 | `4141` | Coincide con el archivo solicitado |
| Seguridad, tag 86 | `00FFFF` | Tres bytes cuya codificación ChipDoc no está resuelta |

Solo se capturó un intercambio SELECT. El control no es el contenido del EF:
no hubo READ 4141, autenticación, PIN o firma. Raw y texto decodificado tienen
permisos 0600; preservar privados junto al informe. El agente no utilizó tarjeta
ni Idopte. No hubo cambios de driver o permisos del observador.

La **tabla 12 de ISO/IEC 7816-4:2005** distingue 86 (atributo de seguridad en
formato propietario) de 8C (formato compacto). La norma, en la
[copia del documento ISO alojada por USPTO](https://ptacts.uspto.gov/ptacts/public-informations/petitions/1528566/download-documents?artifactId=PopQy1plFOQHSsHyHiAXK-0BBsF13u2SVbhITWCr4KbEKUelaC40rHM),
no define para 86 una regla universal que asigne cada byte a READ/UPDATE/DELETE.
El cero inicial no demuestra READ público; tampoco el patrón identifica PACE,
CAN, una clave pública o la causa del VERIFY 6982. No procesarlo con el parser
8C ni trasladar políticas JCOP3.

La revisión local de `iso7816.c` confirma que guarda 86 sin interpretar. MyEID
usa tres bytes con reglas de su propio perfil; su coincidencia de longitud no
acredita las reglas ChipDoc. Las búsquedas por ChipDoc/secuencia/atributos y la
[especificación CIE 3.0](https://www.cartaidentita.interno.gov.it/downloads/2021/03/cie_3.0_-_specifiche_chip.pdf)
no obtuvieron una definición aplicable de 4141/86. Eso no demuestra que tal
documentación no exista. No se copió implementación privativa.

**Siguiente paso:** conseguir la definición del archivo y sus atributos para
ChipDoc 3.2, además del canal/contexto previo a VERIFY. El
[informe ANSSI-CC-2025/03, anexo A](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03-rapport.pdf)
identifica User Guide Manual ref. 678510, versión 1.0, 22 de abril de 2021,
y los otros manuales ya referidos arriba. Su contenido sigue sin obtenerse;
se preguntó al usuario si dispone de ese manual o documentación del perfil BCCR.
Durante esta revisión apareció en raíz el archivo del usuario
`anssi-cible-cc-2021_20v2-m01.pdf`, todavía sin seguimiento Git. Su texto es
**Security Target Lite rev. 1.2, 13 de julio de 2022**. Se revisó localmente:
cita PACE y el User Guide ref. 678510, pero no aporta la definición de 4141/86.
Es una cible anterior, no el manual citado. Se preserva sin agregarlo al commit
ni modificarlo; los hechos observados en tarjeta no cambian por ese documento.

No contactar terceros ni pedir nuevas pruebas autenticadas. READ 4141 permanece
bloqueado; el valor 00FFFF no justifica ampliar la lista por sí solo.

Revisión documental, sin nuevas pruebas: siguen vigentes las 30 comprobaciones
del observador/decoder, 40 del lanzador y 54 del helper anteriores. No recompilar
OpenSC ni repetir hardware por este cambio. Dar comandos de commit al usuario;
soporte JCOP4 y transporte RSA 3072 pendientes.

## Pistas de eOI y revisión del ZIP driverfd

Revisión estática posterior a wYs4iWG8, principal `4f5040f`, OpenSC `3dfef6704`.
El usuario pidió comparar eOI y aportó `local/driverfd.zip`. Solo se leyeron
fuentes/documentación y analizaron literales con AST; no se extrajo ni ejecutó
el paquete, sus proxies, binarios o herramientas de captura. Archivo original
preservado en local/, fuera de Git. No acceso de hardware o autenticación.

### Aplicación, plataforma y personalización

La [cible NXP](https://messervices.cyber.gouv.fr/visas/ANSSI-CC-2025-03-cible.pdf),
secciones 1.3.1.3.1–1.3.1.3.2, distingue JCOP (JCVM/JCRE/JCAPI/GlobalPlatform)
de ChipDoc, aplicación Java Card. Su inicialización permite varias configuraciones
y sistemas de archivos. Esa separación explica por qué un ATR/plataforma común
no asegura comandos, archivos, políticas o algoritmos iguales en dos tarjetas.

| Elemento | Alcance de su significado |
| --- | --- |
| JCOP y hardware | Plataforma que ejecuta el applet; capacidades criptográficas y transporte |
| SELECT/READ/VERIFY y tags ISO | Estructura/interfaz de comandos y respuestas normalizada |
| ChipDoc y su versión | Implementación del protocolo y formatos propios de la aplicación |
| Perfil/personalización | AID, archivos, referencias de claves, certificados y políticas configuradas |

4141 es un identificador de archivo bajo el AID seleccionado, no un código
universal de JCOP4. El tag 86 es ISO, pero su valor usa formato propietario;
00FFFF requiere interpretar el perfil de la aplicación.

### Qué aporta realmente eOI

En el [código eOI](https://raw.githubusercontent.com/OpenSC/OpenSC/master/src/libopensc/card-eoi.c)
revisado, `ChipDocLite` es **una constante del driver**, eoi_model local línea 48,
devuelta por SC_CARDCTL_GET_MODEL. No es resultado de GET DATA del nombre.
eoi_match_card solo compara ATR; eoi_init usa CA 01 16 para versión. La etiqueta
no demuestra qué aplicación respondió en otra tarjeta con el mismo ATR.

La pista de protocolo es eoi_pin_cmd (local línea 400): cuando se solicita
VERIFY y no hay SM, abre eoi_sm_open **sin comprobar interfaz contactless**.
Este último usa PACE con CAN y luego el VERIFY ISO viaja bajo SM. CAN y PIN de
firma tienen papeles distintos: el primero abre ese canal y el segundo autentica
al usuario. La condición de PKCS#15 para abrir SM antes de leer objetos sí se
limita a contactless; no confundirla con la condición anterior a VERIFY.

En [pkcs15-eoi.c](https://raw.githubusercontent.com/OpenSC/OpenSC/master/src/libopensc/pkcs15-eoi.c)
se obtiene el blob CAN por la ruta declarada en el objeto Card CAN y se exige
24 bytes. No hay FID 4141 fijo. eOI hereda process_fci ISO y no implementa una
interpretación de 86=00FFFF. Su capacidad de firma es EC P-384; referencias y
perfil difieren de la JCOP4 RSA 2048 de esta investigación. No activar eOI ni
copiar su canal por ATR/modelo. La ausencia de Card CAN en nuestra enumeración
no descarta otros protocolos de canal.

### Pista nueva: Chip Authentication en el prototipo

El ZIP contiene 53 archivos, fuentes de prototipos y proxies, pero **no las
capturas originales ni salidas de ejecución** citadas por sus comentarios. Su
texto atribuye los intentos al 10 de agosto de 2026/BCCR ChipDoc 3.2. La constante
AID coincide con la aplicación conocida de nuestra JCOP4. **El usuario confirmó
que se trataba de otra JCOP4 y que se logró firmar satisfactoriamente.** Es un
antecedente de firma exitoso reportado para otro ejemplar; no una firma de la
tarjeta actual ni una verificación criptográfica independiente por el agente.
No extrapolar ATR/AID/versión/algoritmo o personalización del ejemplar anterior
solo por compartir generación. Se preservan las discrepancias estáticas del
prototipo: confirmar una firma no identifica por sí solo qué código la produjo.

El ZIP contiene varias rutas de firma. El usuario aclaró posteriormente que
el éxito fue **solo APDU, sin Idopte**, y pidió identificar el comando mediante
revisión estática. La ruta PKCS#11 queda descartada como explicación de ese dato:

| Ruta encontrada | Módulo/protocolo que usa |
| --- | --- |
| src/signer/cli_sign.py | PKCS#11 mediante libidop11.so |
| sign_llavefirma.py | APDU directos; VERIFY sin SM en esa función |
| driverfd/signer.py | Prototipo propio de preparación ECDH y firma bajo SM |

No se ejecutó ninguna ruta. La revisión de las CLI identifica un candidato
concreto, descrito abajo; no demuestra el comando histórico exacto ni valida
por sí sola el CA/SM reproducido.

El prototipo propone SELECT de la aplicación, SELECT/READ D004, MSE SET AT,
GENERAL AUTHENTICATE con ECDH P-521 y posteriormente VERIFY bajo SM. El nombre
EF.CardAccess de D004 y su supuesto contenido no están corroborados en los
informes de esta investigación. No se encontraron referencias 4141 en las
fuentes revisadas; no identifica su función o política.

La revisión del MSE literal en chipdoc/constants.py:25, repetido en otros dos
archivos, aporta una diferencia comprobable respecto a sus nombres:

- Son 18 bytes: cabecera/Lc, **12 bytes de datos**, y un byte restante **02**.
  Tiene una interpretación válida como APDU corto caso-4 con Le=02; no afirmar
  malformación por ese byte restante ni cambiarlo automáticamente.
- El tag 80 declara y contiene 10 bytes de OID dentro de esos 12 datos.
  Decodifica **0.4.0.127.0.7.2.2.3.2.2**, correspondiente a
  **id-CA-ECDH-AES-CBC-CMAC-128** en las
  [definiciones primarias de OpenPACE](https://frankmorgner.github.io/openpace/_static/doxygen/objects_8h_source.html).
  Pertenece a Chip Authentication, distinto de PACE. El nombre de función/
  comentario lo llama 3DES, mientras sus helpers también usan 3DES.
- La constante OID separada contiene 11 bytes y un arco .2 adicional; no
  coincide con el OID delimitado por el TLV del comando. No asumir cuál refleja
  la captura original ni reparar el prototipo y ejecutarlo por esa discrepancia.

Otra limitación: probe_sm_with_pubkey devuelve True si SW1=90 y falta el formato
esperado, antes de validar MAC. Un mensaje SM establecido en esa rama no acredita
acuerdo de claves ni firma. La CLI también cita una clave de una sesión anterior
como fallback y una extracción dinámica externa; esos archivos no están en el
ZIP. No usar claves de otra sesión, extraer secretos o ejecutar estos scripts
como próximo paso. Los comentarios de cinco pasos 9000 son antecedentes sin
captura que permita comprobarlos; no invalidan ni sustituyen los informes actuales.

La hipótesis ahora es más específica: **un canal CA/ECDH antes de VERIFY** puede
ser una alternativa al PACE/CAN de eOI. Aún no se demuestra que sea la ruta de
esta tarjeta. OpenSC ya tiene perform_chip_authentication_ex en sm-eac.c y
[OpenPACE implementa CA versión 2](https://frankmorgner.github.io/openpace/_static/doxygen/ca_8h.html).
No es integración automática: deben coincidir formato de clave, respuesta,
KDF, tokens/MAC y variante CA. El prototipo comenta 7C00 vacío, mientras el helper
existente deriva las claves con nonce/token; compatibilidad pendiente.

La consulta pública actual no observó esos comandos porque se detiene antes
en READ 4141. **No habilitar READ 4141/D004 o GA/SM por el ZIP**. La instrucción
posterior del usuario permite investigar además una variante aislada del orden
VERIFY/MSE; el observador público conserva todos sus bloqueos.

### Comando APDU candidato y diferencias verificables

Revisión adicional al comenzar en principal `1f4d9ae`, OpenSC `3dfef6704`.
El candidato es, desde el directorio del script:

```sh
python3 sign_llavefirma.py --sign
```

Es la CLI APDU encontrada que despacha una secuencia completa de firma.
`driverfd/signer.py` declara `--sign`, pero su bloque CLI solo despacha probe,
detect o ayuda; sus métodos de firma podrían haberse invocado desde otro código
no incluido. `sign_chipdoc_lite.py` solo despacha probes. El ZIP no contiene
salidas originales ni un registro que pruebe el comando histórico exacto.

`sign_llavefirma.py` selecciona un AID, verifica PIN, configura MSE SET AT
con OID de SHA-256/RSA y manda DigestInfo SHA-256 a PSO. Da éxito por SW1=90,
sin verificación con certificado. Su helper limita a ocho bytes el PIN,
rellena FF y prueba referencias 01/81 tras ciertos errores. Además, `--reader`
no se pasa a la conexión de `do_sign`. No recomendar ejecutar ese script tal
cual ni trasladar sus reintentos, truncamiento o supuestos a la tarjeta actual.

La diferencia más cercana al punto donde falla nuestra tarjeta es el orden:
el candidato autentica **antes de MSE**, mientras 5Dpadr1N hizo MSE antes y
obtuvo 6982 en el único VERIFY real. Esto es una hipótesis comprobable, no una
explicación demostrada. También difieren MSE y la entrada PSO; separarlas permite
saber si el cambio de orden resuelve primero la autenticación.

### Rutas evaluadas para obtener una firma válida

| Ruta | Evidencia y decisión actual |
| --- | --- |
| VERIFY antes de MSE | Antecedente estático de la CLI APDU candidata; preparar una variante que cambie solo el orden |
| Otro MSE o DigestInfo en PSO | Diferencias del ZIP relevantes para firmar después de un VERIFY aceptado; no arreglan por sí solas un rechazo anterior ni justifican barridos |
| Chip Authentication ECDH + SM | Hipótesis del prototipo; requiere corroborar clave pública, formato, KDF, variante y MAC antes de integrar OpenPACE |
| PACE/CAN de eOI | Precedente de otro perfil, sin objeto CAN observado aquí; no copiar por ATR ni concluir que sea obligatorio en CR |
| Omitir campos ajenos a la firma | 4141/86, etiquetas, serial y PuKDF no son requisitos de la ruta mínima actual; su omisión ya permite llegar a VERIFY |

No se puede omitir un rechazo VERIFY, inventar autenticación, alterar silenciosamente
el PIN o declarar éxito solo por recibir bytes. Referencia de clave, asociación
con el certificado, hash/padding y verificación criptográfica determinan si la
firma es válida. Los metadatos conocidos de PIN también evitan enviar una
credencial transformada. La ruta mínima conserva esos datos; no requiere
interpretar 86=00FFFF ni leer 4141. El inventario incompleto de algoritmos y los
objetos administrativos tampoco impiden ensayar el mecanismo RSA ya delimitado.

### Variante preparada: VERIFY antes de MSE

Comando histórico ejecutado con la misma JCOP4 y GUI/otros clientes cerrados;
**ya falló, no repetir como próximo paso**:

```sh
nix develop -c make sign/jcop4-verify-first READER=0
```

El modo solo modifica el orden de la operación experimental existente:

1. Validar ATR/AID conocido, metadatos nativos PKCS#15, clave 91 RSA2048,
   referencia PIN 1/UTF-8/sin relleno y certificado público asociado.
2. Seleccionar aplicación y consultar disponibilidad con VERIFY vacío,
   conservando el chequeo anterior; solo 9000 o 63Cx con intentos disponibles
   permite preguntar PIN.
3. Hacer un único VERIFY real sin eco; borrar la credencial inmediatamente.
   Cualquier resultado distinto de 9000 detiene antes de MSE y PSO.
4. Hacer el mismo MSE 00 22 81 B6, datos 91 02 28 91, y solo si acepta 9000
   el mismo PSO 00 2A 9E 9A con digest SHA-256 de 32 bytes y Le 256.
5. Resetear tras el intento autenticado, también ante error, y liberar la
   transacción. Solo una respuesta de 256 bytes y limpieza correcta permite
   guardar firma y verificarla localmente con OpenSSL y el certificado.

No es reproducción completa del ZIP. No cambia MSE, hash/padding, formato
de PIN, referencias o aplicación; no selecciona archivos después de PIN,
introduce canal SM, lee 4141/D004 ni hace reintentos autenticados. Conserva
GET RESPONSE ordinario del PSO previo, pero no recupera errores probando otra
longitud. Firma texto técnico local, no PDF ni PKCS#11 aún.

`sign/jcop4-apdu` conserva su orden anterior. Ambos lanzamientos limpian trazas,
LD_PRELOAD, delegados PC/SC y variables del observador público para que no
interfieran en la operación autenticada. Informes/artefactos privados bajo
local/jcop4-signature-*/; el nuevo modo registra authentication_order=
verify_before_mse y mode=experimental_apdu_sha256_rsa_verify_first, nunca PIN,
longitud real o hash de la credencial. No abrir capturas autenticadas externas.

Pasaron **12 comprobaciones de operación y 6 del lanzador**, con transporte
ficticio: orden, borrado antes de MSE, VERIFY 6982/63Cx/transporte sin MSE/PSO,
fallo MSE posterior sin PSO, firma/reset fallidos sin éxito, modo explícito,
aislamiento de entorno y ausencia de reintentos. Helper real compilado offline;
sin hardware, Idopte o PIN real por el agente. Driver/submódulo y guard v6
intactos; no repetir sus suites sin cambios. No hay nueva firma medida: JCOP4,
integración PKCS#11/PDF y RSA3072 siguen pendientes. Si reaparece VERIFY 6982,
se detiene esta ruta y se revisan los requisitos de canal/personalización;
no se ignora el estado ni se pide otro intento por inercia.

### Resultado VERIFY antes de MSE: 13 de octubre, 19:00:58 UTC

Informe **local/jcop4-signature-6m61lek4/report.txt**, principal `3acd6b9`,
OpenSC `3dfef6704`. Principal/submódulo limpios al comenzar la revisión salvo
el PDF del usuario sin seguimiento, que se preserva. Lector índice 0; el
índice no identifica el modelo del lector. Metadatos validados y certificado
RSA2048; modo experimental_apdu_sha256_rsa_verify_first, orden explícito
verify_before_mse.

| Etapa | Resultado observado |
| --- | --- |
| Consulta de estado sin credencial | Transporte 0, SW 63C7 |
| Único VERIFY real | Transporte 0, SW **6982** |
| MSE / PSO | **No enviados** |
| Reset / unlock | 0 / 0 |
| Helper / proceso | 1 / 1 |

Cambiar el orden no resolvió la autenticación. MSE y formato de entrada PSO
no participaron después de VERIFY en este intento; no ajustar hash/padding
para explicar este rechazo. El resultado tampoco demuestra si el PIN era
correcto o incorrecto, ni cuál de los canales posibles requiere esta
personalización. 63C7 corresponde al estado **anterior** al intento: no hay
consulta posterior del contador en este informe. No repetir el diagnóstico
ni probar relleno, otras referencias, PIN administrativo o ignorar 6982.

### Auditoría adicional del canal propuesto en el ZIP

El usuario confirmó que **solo conserva el ZIP**, sin registros originales
de la firma exitosa en la otra JCOP4. Se respeta ese antecedente, pero no es
posible identificar su preparación exacta por una salida histórica conservada.
Revisión estática adicional sin extracción, importación, ejecución o hardware:

| Hallazgo en las fuentes | Consecuencia para la investigación |
| --- | --- |
| chipdoc/sm.py, build_sm_apdu calcula `mac` pero no lo añade al comando | La implementación transmite el indicador 01 y ciphertext, sin envolver DO87 ni enviar DO8E; no implementa el formato ISO SM que se pretende reproducir |
| parse_sm_response devuelve 9000 fijo y no decodifica DO99 | Su valor de éxito no acredita el estado protegido real de la respuesta |
| driverfd/signer.py, establish_secure_channel solo inicializa claves locales | El mensaje de canal establecido no prueba confirmación criptográfica con la tarjeta |
| SELECT bajo SM es constante 0C A4 00 00 00 00 21 | No incluye MAC ni usa las claves calculadas; además son siete bytes de caso-2 extendido, no un envío ordinario con Lc cero |
| GENERAL AUTHENTICATE usa CLA10 sin fragmento final en estas funciones | En el encadenamiento ISO indica un comando intermedio; un posible 9000 no equivale por sí solo a autenticar y cerrar la secuencia |
| kdf_bsi concatena contadores de un byte, produce claves 3DES de 24 bytes y deriva SSC | No coincide con la derivación CA/AES indicada por su OID; la variante counter_be corrige el ancho del contador, sin resolver algoritmo, contexto o confirmación |

El contraste se hizo contra fuentes primarias. En
[OpenSC sm-iso.c](https://github.com/OpenSC/OpenSC/blob/master/src/sm/sm-iso.c)
el formato contiene objetos de datos protegidos y estado de respuesta; el
MAC calculado se incorpora al APDU. El
[encadenamiento APDU de OpenSC](https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/apdu.c)
añade CLA10 a fragmentos intermedios; el
[CA de sm-eac.c](https://github.com/OpenSC/OpenSC/blob/master/src/sm/sm-eac.c)
envía GENERAL AUTHENTICATE con CLA00 y procesa nonce/token. No cambiar bytes
del prototipo y probarlos por analogía: debe determinarse la variante del perfil.

[OpenPACE eac_kdf.c](https://raw.githubusercontent.com/frankmorgner/openpace/master/src/eac_kdf.c)
deriva con secreto, nonce opcional y contador de cuatro bytes en orden de red;
el tamaño depende del cifrador seleccionado. Su
[CA versión 2](https://raw.githubusercontent.com/frankmorgner/openpace/master/src/eac_ca.c)
deriva y verifica el token antes de aceptar claves. En
[eac.c](https://raw.githubusercontent.com/frankmorgner/openpace/master/src/eac.c),
la autenticación incorpora SSC y la comprobación compara el MAC; inicializar
el contexto por sí solo no demuestra acuerdo con una tarjeta. Estas fuentes
definen implementaciones de referencia, no el perfil BCCR.

También se inspeccionó estáticamente el módulo instalado libidop11.so, **sin
cargarlo ni ejecutarlo**. Contiene nombres CA/ECDH con 3DES y AES128/192/256,
PACE y PK-ECDH. La búsqueda limitada no encontró literales exactos de los OID
CA/AES128 y CA/3DES, MSE del ZIP o SELECT D004/4141. No es prueba de ausencia:
pueden construirse en ejecución. Sus nombres y mensajes genéricos no indican
qué rama usa esta JCOP4 ni autorizan reproducirla. No se copió implementación
propietaria. La búsqueda pública adicional recuperó referencias a manuales,
sin obtener una especificación aplicable del canal.

Los metadatos disponibles tampoco resuelven esa elección: los certificados
observados son RSA de firma/autenticación; la enumeración AUTH no anunció CAN
y el ODF solo identificó un directorio de llaves públicas 7004, sin enumerar
su contenido. Eso no identifica una clave CA/ECDH ni acredita ausencia de ella.
No sustituir la clave pública del canal por la del certificado RSA o por una
clave de sesión anterior; no seguir rutas desconocidas por esa coincidencia.

El dato faltante sigue siendo la **preparación real del canal en este perfil**:
algoritmo/variante, origen y formato de la clave pública o secreto de acceso,
MSE/GA y confirmación/SM. Sin registro original, la vía documental es el User
Guide 678510/Crypto Guide y la personalización BCCR; una cible que enumera
protocolos no determina esos parámetros. El estado 6982 y el ZIP justifican
investigar esa preparación, sin demostrar que sea CA, PACE u otra condición.
No nuevo READ 4141/D004, GA/SM o PIN por los mensajes del prototipo.

Esta revisión solo cambió documentación; guard, helper y driver intactos.
Las 12+6 comprobaciones de firma anteriores siguen vigentes, sin repetir
suites. No hardware ni autenticación por el agente, ni nueva firma válida
JCOP4; integración PKCS#11/PDF y RSA3072 pendientes.

## Comparación empírica de APDU: siguiente observación sin PIN

El usuario propone comparar el tráfico real de libidop11.so con el prototipo
y experimentar con pasos/valores, aunque no haya definición completa de cada
byte. Es una vía válida de ingeniería inversa. La falta de manual no impide
observar comandos públicos o ensayar hipótesis concretas; sí requiere distinguir
hechos, dependencias del middleware y requisitos criptográficos de la tarjeta.

Comparación disponible al comenzar en principal `3e24829`, OpenSC `3dfef6704`:

| Etapa | Idopte observado en esta JCOP4 | ZIP revisado | Helper experimental actual |
| --- | --- | --- | --- |
| Nombre/versión | CA0100 y CA0116, transporte 0/SW9000 | No son parte de do_sign; comentarios suponen ChipDoc | Versión consultada por diagnóstico anterior; firma valida ATR/AID/metadatos |
| SELECT de aplicación | 00 A4 04 0C al AID conocido, SW9000 | AID candidato coincide en constantes; do_sign prueba varios | Selecciona solo el AID conocido anunciado en EF.DIR |
| Archivo previo al canal | SELECT 4141: 6F/400 bytes/86=00FFFF; READ bloqueado antes de transporte | Prototipo SM selecciona D004 y pretende leer 137 bytes; no evidencia actual de ese archivo | No lee 4141 ni D004 |
| MSE para CA/ECDH | No observado después del bloqueo READ | 00 22 41 A4 0C 80 0A 04 00 7F 00 07 02 02 03 02 02 02; OID CA/AES128 y Le02 | No establece canal CA/SM |
| GENERAL AUTHENTICATE | No observado en esta consulta | Prototipo construye 10 86 00 00 / 7C / 80 / punto P521, sin cierre mostrado | No envía GA |
| VERIFY real | Sin captura autenticada de Idopte | CLI directa antes de MSE; SM incompleto en otra ruta | 6982 tanto antes como después de MSE de firma |
| Firma RSA | Usuario confirma uso exitoso de Idopte instalado, sin traza autenticada aquí | CLI directa usa MSE SET AT/OID y DigestInfo, sin verificación de certificado | MSE SET DST/91022891 aceptado; PSO con digest32 preparado pero no alcanzado |

Solo SELECT 4141 tiene aquí comando/respuesta completos capturados por el modo
spy. Los demás APDU permitidos se registraron por cabecera y SW; no inventar
los FID/Lc/Le omitidos. La consulta Idopte es inventario de slots, no una firma:
que no muestre MSE/GA todavía no demuestra que no se usen al autenticar.

### Modo optativo de lectura para comparación

Se prepara **inspect/jcop4-idopte-compare**, distinto de discovery y spy, por la
solicitud de comparación posterior. La ampliación permite observar el progreso
tras la operación concreta que nuestro propio guard interrumpió. **No está
basada en interpretar 86=00FFFF como acceso público ni cambia la política de
los modos anteriores**. Idopte recibe el contenido de 4141 en su proceso; el
observador no lo registra ni interpreta su propósito. No autenticación ni PIN.

Con misma JCOP4 y firmador/otros clientes cerrados, ejecutar una vez:

```sh
nix develop -c make inspect/jcop4-idopte-compare READER=0
```

El lanzador conserva inventario de lector/ATR, módulo instalado fijo,
--show-info --list-slots, stdin vacío, entorno aislado e informes privados.
Opt-in JCOP4_DIAGNOSTIC_READ4141=1 solo para ese proceso; se elimina antes del
inventario y en los demás modos, incluida firma. No ejecutar con firmador.

El guard exige aplicación conocida seleccionada/9000 y el mismo SELECT 4141
ya observado. En SCardConnect/Reconnect comprueba el **ATR de la conexión real**
mediante SCardGetAttrib(ATR_STRING), sin guardar sus bytes. ATR ausente/distinto
detiene antes de APDU. Esto evita atribuir la conexión al índice del inventario:
Idopte --list-slots enumera todos los lectores; READER solo selecciona la consulta
previa. La comprobación del ATR no envía APDU. Permite exclusivamente READ BINARY básico de ese archivo, sin
SFI, payload ni SM; acepta caso-2 corto o extendido con Le explícito no nulo.
El Le00 corto significa 256 y sí se permite. Offset inicial cero y el siguiente
debe continuar los bytes realmente devueltos. **Máximo dos lecturas y 400 bytes
solicitados en todo el proceso**: SELECT no repone presupuesto/offset ni permite
releer; una reconexión después de READ detiene definitivamente. Un fallo de
escritura del registro también detiene antes de transmitir la lectura.
No archivo nuevo, READ RECORD, otra referencia, escritura o control de PIN.

Se transmite con buffer temporal de Le+2, nunca superior a 402 bytes, y se
exige capacidad suficiente del cliente. Solo transporte correcto, respuesta
entre 2 y Le+2 y SW9000/6282 permiten devolver los datos a Idopte. Otro SW,
61xx/GET RESPONSE, transporte o tamaño detiene; sin fallback. read4141_sw y
read4141_transport distinguen un SW rechazado de un bloqueo propio del guard.
La respuesta nunca se copia a las trazas. El modo READ y la captura SELECT
spy son incompatibles; configuración opt-in inválida detiene antes de APDU.

Se mantienen bloqueados **VERIFY incluso vacío, MSE, GA, SM, PSO** y operaciones
administrativas. Si Idopte intenta exactamente el MSE CA conocido, se registra
solo blocked_mse_profile=ca_ecdh_aes128/ca_ecdh_3des y blocked_mse_case=
short_case3/short_case4. Son etiquetas de constantes públicas ya identificadas,
no buffers arbitrarios ni permiso de transporte. Otros MSE solo muestran
cabecera. No longitudes/credenciales o respuestas de archivo en el log.

Archivos local/jcop4-idopte-compare-*/report.txt y public-headers.txt, permisos
privados. El inventario Idopte puede imprimir identificadores derivados del
token: no publicar los informes completos. No raw/txt de READ ni pcsc-spy
global. Module exit distinto de cero por el siguiente bloqueo es esperable;
revisar SW de lectura y primer comando bloqueado antes de repetir o ampliar.

### Cómo elegir variantes que respondan una duda

1. Obtener secuencia base del proveedor hasta el próximo bloqueo y comparar
   la **misma tarjeta/aplicación**, sin trasladar éxito de otra personalización.
2. Identificar qué dato se necesita para reproducir el paso. Si el comando
   siguiente es MSE CA, contrastar su identificador/caso con el literal del ZIP;
   si es otro SELECT, estudiar ese archivo antes de autorizar una lectura.
3. Cambiar una sola variable comprobable en un diagnóstico acotado y separado,
   con reset/limpieza y resultado de transporte/SW. No barridos ni credenciales.
   La prueba de orden VERIFY/MSE ya respondió negativamente esa hipótesis.
4. Distinguir dependencia del proceso Idopte de condición de la tarjeta: que
   Idopte avance tras READ puede demostrar que necesitaba esos datos para
   preparar el siguiente comando, sin demostrar que leerlos cambie autenticación.
5. En protocolos criptográficos, identificar nonce, clave efímera, contador,
   MAC y OID. Bytes aleatorios distintos entre sesiones no implican variantes
   arbitrarias. Cambiar datos autenticados exige regenerar su MAC/contexto;
   omitir comprobaciones de MAC o aceptar 6982 no produce una firma válida.

Pasaron **41 comprobaciones del observador/decoder, 43 del lanzador y 6 del
lanzador de firma**, sin hardware. Incluyen lectura corta/extendida acotada,
presupuesto no renovable, continuidad, restricciones de forma/buffer, rechazo
69xx/61xx/respuesta excesiva, VERIFY antes del backend, clasificación MSE sin
transporte, incompatibilidad spy/READ y aislamiento del entorno. Delegación
PC/SC real con plugin/transporte ficticios y biblioteca de diagnóstico real
compilada. Primer fallo de integración fue fixture limitado a D003, corregido;
no error de tarjeta. Helper de firma/público y driver intactos. No ejecutar
Idopte o hardware por el agente durante esa preparación. La autorización
posterior del usuario permite ejecutar comandos sin PIN ni GUI, incluidos estos
diagnósticos y Git; resultado de la primera ejecución a continuación.
JCOP4, firma PKCS#11/PDF y RSA3072 siguen pendientes.

### READ 4141 aceptado; siguiente SELECT bloqueado: 19:55:02 UTC

El agente ejecutó `nix develop --offline -c make inspect/jcop4-idopte-compare
READER=0` tras la autorización del usuario para comandos sin PIN ni GUI.
Informe privado `local/jcop4-idopte-compare-ReO2RheL/report.txt`, principal
`3e24829` con cambios pendientes, OpenSC `3dfef6704` intacto. Inventario y
compilación correctos; `comparison_atr=known_jcop4` verifica la conexión real.

| Paso observado | Transporte / SW | Alcance de la evidencia |
| --- | --- | --- |
| SELECT 4141 en aplicación conocida | 0 / 9000 | Selección aceptada |
| READ BINARY, cabecera 00 B0 00 00 | 0 / 9000 | Lectura sin autenticación aceptada; contenido omitido |
| READ BINARY, cabecera 00 B0 00 DF | 0 / 9000 | Segunda lectura aceptada; offset DF, contenido omitido |
| SELECT EF 4109 | Bloqueado antes de transporte | Intento del proveedor; no respuesta de tarjeta |

El registro no conserva Le ni longitudes devueltas: no afirmar lectura completa
de los 400 bytes. Hay dos lecturas diagnósticas; sus contenidos solo se devolvieron
al proceso Idopte. Directorio 0700, informe/cabeceras 0600. Ningún PIN, VERIFY,
MSE, GA, PSO o escritura llegó a la tarjeta. `module_exit_status=1` y el error de
Make son consecuencia compatible con el bloqueo de SELECT 4109; el observador
sí registró los eventos. No es el VERIFY 6982 anterior ni un rechazo de READ.

Esto identifica otro paso de inventario que el helper/ZIP no reproduce; no
demuestra que leer 4141 establezca autenticación, ni que 4109 contenga una clave
CA, CAN, CardAccess o datos públicos. No se encontró referencia a 4109 en el
código local/ZIP revisado; las búsquedas públicas no dieron una definición
aplicable. El siguiente diagnóstico propuesto es SELECT-only de 4109 bajo la
aplicación observada, para revisar metadatos antes de decidir una lectura.
No repetir esta consulta ni ampliar READ por el solo orden de los archivos.

El commit intentado por el agente falló al crear `.git/index.lock`:
`Read-only file system`. La autorización existe; el montaje de `.git` en este
entorno impide escribirlo. No se creó un commit ni se alteró OpenSC.

## SELECT 4109 aceptado: 13 de octubre, 20:00:55 UTC

El usuario registró el diagnóstico anterior en `32616a2`. El agente preparó y
ejecutó `nix develop --offline -c make inspect/jcop4-file4109 READER=0`.
Informe privado `local/jcop4-file4109-LKNy75nY/report.txt`, principal `32616a2`
con este diagnóstico pendiente de commit, OpenSC `3dfef6704` sin cambios.

```text
file4109_select_transport=0
file4109_select_sw=9000
file4109_structure=transparent
file4109_size=17
file4109_security_format=profile_uninterpreted
file4109_security_direct_tags=86
file4109_standard_compact_read=uninterpreted
file4109_result=selected_metadata_only_no_read
```

Inventario, compilación, conexión, lock, EF.ATR/EF.DIR, unlock y proceso correctos.
El helper exige ATR conectado conocido y aplicación conocida anunciada; selecciona
esa aplicación y únicamente envía `00 A4 02 00 02 41 09 00`. No reproduce las
lecturas previas de 4141: confirma selección y control de 4109 bajo la aplicación,
no equivalencia de todo el estado interno de Idopte. Sin READ de 4109, binding
PKCS#15, PIN, VERIFY, MSE, GA o PSO. No se ejecutó nuevamente el inventario Idopte.

El analizador de control se comparte entre los dos FID fijos, con validación de
83 cuando aparece; no se añadió interfaz para sondear archivos arbitrarios.
Conserva los resúmenes de seguridad existentes sin imprimir valores propietarios,
nombres ni respuestas brutas. 86 identifica una codificación propietaria; su
presencia y el tamaño de 17 bytes no identifican contenido, permiso público,
CAN, clave, CardAccess ni condición faltante de VERIFY 6982. La existencia de
4109 queda confirmada en este contexto, su propósito sigue pendiente.

Pasaron **58 comprobaciones del helper y 45 del lanzador**, sin hardware;
compilación real correcta. Las nuevas regresiones comprueban FID exacto,
respuesta con identificador distinto, contexto/AID/ATR, errores y continuidad
bloqueada tras 61xx/6Cxx, sin READ o autenticación. Observador y operación de
firma intactos; no
repetir sus suites por este cambio. Directorio 0700 e informe privado 0600.

El modo Idopte conserva SELECT 4109 bloqueado. Para avanzar habrá que decidir
una observación concreta del proveedor después de ese SELECT y, si intenta
READ, acotarla separadamente al archivo de 17 bytes sin registrar su contenido;
no habilitar selección/lecturas generales ni repetir PIN. La consulta de
metadatos actual no establece la ruta de autenticación. JCOP4/RSA3072 y firma
PKCS#11/PDF con el módulo libre siguen pendientes.

## Idopte selecciona 4109 e intenta leerlo: 13 de octubre, 20:04:49 UTC

El usuario registró el SELECT-only anterior en `40decb2`, principal limpio al
comenzar; OpenSC `3dfef6704` intacto. Se amplió exclusivamente el modo optativo
`inspect/jcop4-idopte-compare` para transmitir el SELECT 4109 observado:
CLA00/A4/P1=02/P2=00/Lc=2/FID4109, total 7 u 8 bytes. Requiere ATR de conexión
validado, aplicación conocida seleccionada con éxito, archivo 4141 seleccionado
y al menos una lectura acotada previa de ese archivo. No concede READ de 4109.

La selección elimina el permiso READ anterior y conserva el contexto de ADF
únicamente con SW9000. Falla, continuación, cambio de aplicación/canal o forma
distinta no habilitan lecturas. Los modos discovery/spy siguen bloqueando
SELECT 4109; presupuesto de READ 4141, bloqueo de autenticación y privacidad
intactos. Se añade únicamente la etiqueta fija `known_4109_metadata_only`.
El nuevo alcance queda registrado como
`read4141_max400_select4109_only_no_recording_no_authentication`.

El agente ejecutó una vez el modo actualizado, sin PIN ni GUI. Informe privado
`local/jcop4-idopte-compare-BCIdSIvC/report.txt`, 20:04:49 UTC:

```text
comparison_atr=known_jcop4
public_header=00A40200
public_select=known_4109_metadata_only
transport=0
sw=9000
blocked_header=00B00000
module_exit_status=1
observer_result=events_recorded
```

Inventario y compilación correctos; las 17 APDU transmitidas terminaron con
transporte0/SW9000, incluidas las dos lecturas acotadas de 4141. El intento READ
BINARY de 4109 **no llegó a la tarjeta**: no existe respuesta de tarjeta a esa
lectura ni contenido de 4109 en la traza. El error final del módulo/Make sigue
siendo compatible con el bloqueo del guard. No se observó MSE, GA o VERIFY
antes de ese bloqueo; tampoco demuestra que no se usen en autenticación.

Esta consulta confirma el siguiente acceso que necesita la enumeración de
Idopte, no el propósito de los 17 bytes ni su efecto sobre autenticación. El
ZIP revisado no contiene referencia a 4109. Para avanzar empíricamente, la
hipótesis concreta es que Idopte necesita ese pequeño archivo para completar
inventario o preparar otro paso: una futura excepción de lectura tendría que
ser independiente, ajustada a su forma realmente solicitada y al tamaño de
17 bytes, sin guardar contenido ni desbloquear PIN/canal/firma. No deducir
su permiso de lectura por SELECT9000 o por el atributo 86. No repetir esta
consulta idéntica ni pedir otra prueba de PIN mientras ese paso esté bloqueado.

Pasaron **44 comprobaciones del observador/decoder y 45 del lanzador**, transporte
ficticio y redirector real; biblioteca compilada. Nuevas regresiones verifican
secuencia/contexto/ATR y formas exactas, revocación de READ anterior, bloqueo
de READ BINARY/RECORD y PIN, y manejo de SELECT pendiente/fallido. Helper público,
operación de firma y driver intactos; sus pruebas previas siguen vigentes.

## READ 4109 aceptado; siguiente directorio 7001: 13 de octubre, 20:12:16 UTC

El usuario registró la observación SELECT-only en `e37cff1`, principal limpio al
comenzar; OpenSC `3dfef6704` intacto. El agente preparó el modo independiente
`inspect/jcop4-idopte-read4109` y lo ejecutó una vez, sin PIN ni GUI. Informe
privado `local/jcop4-idopte-read4109-4qFZ03Qz/report.txt`.

El nuevo opt-in `JCOP4_DIAGNOSTIC_READ4109=1` requiere el opt-in READ4141 y log
privado válido; solo el lanzador de este modo lo introduce dentro del proceso
Idopte --show-info --list-slots, stdin vacío. Inventario y modos anteriores lo
eliminan; también el lanzador de firma. No se alteran los permisos de compare,
discovery o spy. Configuración inválida, sin modo padre o mezclada con raw spy
impide todo transporte.

Se permite una sola solicitud READ BINARY caso-2 básico, corta o extendida,
con **Le exactamente 17, offset cero**, únicamente tras SELECT4109 exitoso
bajo el contexto/ATR/secuencia previamente comprobados. No se reduce un Le
mayor ni se cambia el APDU del proveedor. Buffer temporal Le+2=19; solo
transporte correcto, tamaño entre 2 y 19 y SW9000/6282 devuelven datos a Idopte.
Sin contenido en la traza, sin GET RESPONSE, reintento, repetición, SFI, otros
canales o READ RECORD. El presupuesto global de una llamada no se reinicia por
SELECT. Los controles de autenticación, SM, MSE/GA/PSO, escrituras y PIN siguen
bloqueados; los límites de READ4141 no cambian.

```text
comparison_scope=read4141_max400_read4109_once17_no_recording_no_authentication
comparison_atr=known_jcop4
public_select=known_4109_bounded_read_comparison
transport=0
sw=9000
diagnostic_read=known_4109_contents_omitted
public_header=00B00000
transport=0
sw=9000
blocked_header=00A40200
blocked_select_ef=7001
module_exit_status=1
observer_result=events_recorded
```

Inventario y compilación correctos; las 20 APDU transmitidas terminaron con
transporte0/SW9000. READ4109 solicitó 17 bytes y fue aceptado; la traza no
registra longitud devuelta ni contenido. Después aparecen un SELECT EF y READ
bootstrap permitidos, cuyos FID no se registran: no asignarlos por las cabeceras.
Luego **SELECT7001 fue bloqueado antes de transmitir**, sin SW de tarjeta a ese
comando. El error final corresponde al guard, no a rechazo de READ4109 ni al
VERIFY6982 anterior. No se llegó a PIN, MSE, GA o PSO.

El ODF observado en `local/jcop4-odf-map-eAjoi575/report.txt` ya identifica
7001 como `auth_objects`, directorio de objetos de autenticación. Esa asociación
sí tiene evidencia en esta tarjeta, a diferencia del propósito desconocido de
4141/4109. Seleccionar 7001 no significa verificar PIN o leer una credencial;
seguiría siendo necesario distinguir los metadatos de autenticación de una
operación de autenticación. No afirmar que Idopte estableció canal o descartar
CA/PACE por esta consulta de inventario. El ZIP no documenta 4109 ni este paso.

Pasaron **52 comprobaciones observador/constructor/decoder, 47 lanzador y 6
lanzador de firma**, sin tarjeta; delegación real con transporte simulado y
biblioteca real compilada. Cubren opt-ins aislados, petición exacta/buffer,
rechazo de formas/Le/offset, SW/continuación/overrun, log fallido, repetición,
privacidad y bloqueo de VERIFY después de ambas lecturas. Helper público,
operación de firma y OpenSC intactos. Registros privados: directorio 0700,
report.txt/public-headers.txt 0600. No capturas de respuestas ni Git local/.

El siguiente paso con precedente aplicable es estudiar/observar acceso al
AODF7001 del mismo AID, comparando con el inventario PKCS#15 ya obtenido; no
escanear nuevos FID ni extender lectura por el solo orden de archivos. La
consulta actual ya respondió qué intenta Idopte después: no repetir idéntica.
La ruta de canal/VERIFY permanece pendiente, JCOP4 y RSA3072 no están resueltos.

## AODF7001 leído; SELECT7002 bloqueado: 13 de octubre, 20:18:15 UTC

Principal `8dbd7da`, OpenSC `3dfef6704`, ambos limpios al comenzar; usuario hizo
commit anterior. Se contrastó el próximo FID con el ODF de esta tarjeta:
7001=`auth_objects`. El helper nativo ya obtuvo metadatos de autenticación
sin login en consultas previas, por lo que no se traslada una función de otro
perfil ni se interpreta el tag 86 como permiso público.

Primera consulta independiente SELECT-only, realizada por el agente:
`nix develop --offline -c make inspect/jcop4-file7001 READER=0`. Informe privado
`local/jcop4-file7001-3dyC89fA/report.txt`, **20:15:53 UTC**:

```text
file7001_select_transport=0
file7001_select_sw=9000
file7001_structure=transparent
file7001_size=300
file7001_security_format=profile_uninterpreted
file7001_security_direct_tags=86
file7001_standard_compact_read=uninterpreted
```

Compilación, inventario, conexión, lock, EF.ATR/EF.DIR, unlock y proceso correctos;
no READ7001 en esa consulta. Reutiliza el analizador de control y valida 83
contra el FID fijo; CLI no acepta archivos arbitrarios. No binding o PIN.

Con ese tamaño confirmado y el precedente ODF/PKCS#15, se añadió el modo
independiente `inspect/jcop4-idopte-aodf`, opt-in `JCOP4_DIAGNOSTIC_AODF=1` solo
junto a los dos opt-ins de lectura anteriores dentro de Idopte inventario.
Requiere ATR validado/ADF conocido, READ4109 previo y SELECT7001 básico exacto
00/A4/02/00/Lc2/FID7001 de 7 u 8 bytes. Solo SW9000 conserva ADF para lectura.
READ BINARY caso-2 básico, sin SFI/payload/SM; máximo dos solicitudes contiguas
y 300 bytes solicitados por proceso. Cursor avanza según datos devueltos;
SELECT no repone presupuesto. Buffer temporal Le+2, SW9000/6282 únicamente;
otro SW/transporte/tamaño/log fallido detiene, sin GET RESPONSE/reintento.
La reconexión no permite continuar. Contenido devuelto a Idopte, no registrado.

El agente ejecutó una vez
`nix develop --offline -c make inspect/jcop4-idopte-aodf READER=0`, informe
`local/jcop4-idopte-aodf-CUeTL3Na/report.txt`, **20:18:15 UTC**:

```text
comparison_atr=known_jcop4
public_select=known_aodf7001_bounded_read_comparison
transport=0
sw=9000
diagnostic_read=known_7001_contents_omitted
public_header=00B00000
transport=0
sw=9000
diagnostic_read=known_7001_contents_omitted
public_header=00B000DF
transport=0
sw=9000
blocked_header=00A40200
blocked_select_ef=7002
module_exit_status=1
observer_result=events_recorded
```

Las **23 APDU transmitidas** dieron transporte0/SW9000; inventario/compilación
correctos. Dos READ7001 aceptados, segundo offsetDF; la traza no guarda Le ni
longitudes devueltas, así que no afirmar lectura completa por las cabeceras.
**SELECT7002 no se transmitió**: no existe SW de tarjeta a ese comando. Error
de módulo/Make por bloqueo propio, no nuevo rechazo de tarjeta. No PIN, VERIFY,
MSE, GA, SM, PSO o escrituras. Los modos anteriores mantienen SELECT/READ7001
bloqueados; lanzadores de inventario/firma limpian el nuevo opt-in externo.

El ODF previo identifica 7002=`private_keys`; es el directorio de metadatos
PKCS#15 del perfil observado. El helper ya obtuvo referencias RSA nativas de
ese inventario sin exportar llaves. Que Idopte avance por AODF/PrKDF muestra
su enumeración de objetos; no explica por sí solo el canal previo a C_Login
ni resuelve VERIFY6982. No solicitar PIN a partir de esta prueba. El siguiente
paso debe usar ese precedente de metadatos concretos, no sondear archivos
ajenos o asumir reglas de 86. No repetir esta consulta idéntica.

Pasaron **59 pruebas del helper, 57 del observador/constructor/decoder, 51 del
lanzador y 6 del lanzador de firma**, simulación sin tarjeta; biblioteca y helper
reales compilados. Nuevas regresiones cubren identidad del FID, contexto/ATR,
300 bytes no renovables, continuidad, respuesta excesiva/69xx/61xx y opt-ins
independientes; redirector real devuelve AODF sintético solo al cliente y bloquea
VERIFY. Helper de firma/driver intactos, suite12 previa vigente. Archivos privados
0700/0600 bajo local/, no incorporar capturas o inventarios completos a Git.

## PrKDF7002 leído; SELECT7004 bloqueado: 13 de octubre, 20:33:01 UTC

Principal `c61fdd7`, OpenSC `3dfef6704`, limpios al comenzar. El usuario hizo
commit anterior. Se contrastó el FID con el ODF actual: 7002=`private_keys`.
El inventario nativo previo ya obtuvo referencias de llaves RSA con atributo
native=1 sin login; la nueva observación examina acceso al directorio de
metadatos, no una extracción de material privado de esas llaves.

El agente preparó y ejecutó SELECT-only de ese FID:
`nix develop --offline -c make inspect/jcop4-file7002 READER=0`. Informe privado
`local/jcop4-file7002-832gXk0r/report.txt`, **20:30:16 UTC**:

```text
file7002_select_transport=0
file7002_select_sw=9000
file7002_structure=transparent
file7002_size=2048
file7002_security_format=profile_uninterpreted
file7002_security_direct_tags=86
file7002_standard_compact_read=uninterpreted
```

Inventario, compilación, conexión, lock, EF.ATR/EF.DIR, unlock y proceso correctos.
No READ7002 en esa consulta, binding o PIN. **2048 es el tamaño del archivo en
bytes**, no bits de una llave. No interpretar el tag86 como política pública.
La nueva opción fija reutiliza el analizador de control y exige FID83 correcto.

Se añadió después el modo separado `inspect/jcop4-idopte-prkdf`, opt-in
`JCOP4_DIAGNOSTIC_PRKDF=1` solo junto a los tres anteriores dentro del proceso
Idopte de inventario. No cambia los modos existentes. SELECT7002 requiere ATR
validado, ADF conocido/9000, READ7001 previo y forma básica exacta A4/02/00/Lc2,
total7 u8. Solo la selección aceptada permite READ BINARY caso-2 básico, sin
SFI/datos de entrada/SM. Se preserva la solicitud del proveedor: **hasta256 bytes
por petición, hasta2048 bytes solicitados totales, máximo diez solicitudes**,
offset inicial0 y siguientes contiguos a los datos realmente devueltos.
SELECT no repone presupuesto/cursor. Buffer temporal Le+2 (máximo258);
transporte correcto, tamaño2..Le+2 y SW9000/6282 exclusivamente. Fallos detienen,
sin GET RESPONSE/reintento. Todos los contenidos siguen omitidos de las trazas;
la reconexión no reinicia ningún permiso. Lanzadores de inventario y firma
limpian el opt-in externo. PIN/MSE/GA/SM/PSO/escrituras siguen bloqueados.

Ejecución por el agente una vez:
`nix develop --offline -c make inspect/jcop4-idopte-prkdf READER=0`, informe
`local/jcop4-idopte-prkdf-2LKKNfZB/report.txt`, **20:33:01 UTC**. Inventario y
compilación correctos, comparison_atr=known_jcop4. **34 APDU transmitidas**, todas
transporte0/SW9000. SELECT7002 y diez READ aceptados, cabeceras de offset:
0000, 00DF, 01BE, 029D, 037C, 045B, 053A, 0619, 06F8 y 07D7. El registro no
conserva Le ni longitud devuelta final: no afirmar lectura completa por la
secuencia de cabeceras. Contenido entregado exclusivamente al proceso Idopte.

```text
blocked_header=00A40200
blocked_select_ef=7004
module_exit_status=1
observer_result=events_recorded
```

El siguiente SELECT7004 fue bloqueado **antes de transmitir**, sin SW de tarjeta
ni contenido de7004. El error del módulo/Make corresponde al guard, no a un
rechazo de READ7002 ni al VERIFY6982 previo. No hubo PIN, autenticación o firma.
El ODF ya identifica 7004=`public_keys`; el siguiente contraste útil es el
inventario de algoritmos/referencias de ese directorio en la misma aplicación.
Su nombre por sí solo no identifica una clave de Chip Authentication, ni prueba
que la ruta ECDH del ZIP corresponda a esta tarjeta. No descartar CA/PACE por
una enumeración de slots; no pedir otra prueba de PIN por este resultado.

Pasaron **60 comprobaciones del helper, 64 del observador/constructor/decoder,
55 del lanzador y 6 del lanzador de firma**, transporte simulado. Las pruebas
nuevas verifican presupuesto2048 no renovable, límite independiente de diez
llamadas, contexto/ATR/secuencia, continuidad, SW/overrun, aislamiento y
constructor inválido/sin modo padre. Redirector real retorna PrKDF sintético
únicamente al cliente y bloquea VERIFY. Compilación real del helper y biblioteca.
Operación de firma y driver intactos; sus pruebas previas siguen vigentes.
Informes privados0700/0600; no volcar inventarios completos/IDs o respuestas a
Git/chat. Próximo paso concreto: examinar metadatos PuKDF7004 con el precedente
ODF actual. No repetir esta consulta idéntica. JCOP4/RSA3072 siguen pendientes.

## PuKDF7004: dos objetos RSA2048, 13 de octubre, 20:48:52 UTC

Principal `76c99c9`, OpenSC `3dfef6704`, ambos limpios al comenzar; usuario
registró el commit anterior. Dos consultas nuevas del agente, sin PIN/GUI.

Primero SELECT-only7004:
`nix develop --offline -c make inspect/jcop4-file7004 READER=0`, informe privado
`local/jcop4-file7004-JRw8iNNl/report.txt`, **20:48:34 UTC**. SELECT transporte0/
SW9000, estructura transparente, **4096 bytes**, security_direct_tags=86,
security_format=profile_uninterpreted, standard_compact_read=uninterpreted.
Inventario, compilación, conexión, lock, EF.ATR/EF.DIR, unlock y proceso correctos.
Sin READ7004 en esa consulta. No interpretar tamaño/tag86 como política de
acceso o longitud de llave.

Después inventario de metadatos público:
`nix develop --offline -c make inspect/jcop4-public-keys READER=0`, informe
`local/jcop4-public-keys-Qfl75Od0/report.txt`, **20:48:52 UTC**. Native binding,
análisis de PuKDF, conexión/contexto/lock, EF.ATR/EF.DIR y unlock/proceso0.

| Objeto en PuKDF7004 | Algoritmo | Tamaño declarado | Referencia | Native | Usage numérico |
| --- | --- | --- | --- | --- | --- |
| 0 | RSA | 2048 bits | 144 / 0x90 | 1 | 208 |
| 1 | RSA | 2048 bits | 145 / 0x91 | 1 | 192 |

```text
native_pkcs15_bind_result=0
public_keys_parse_result=0
public_key_metadata_count=2
public_keys_result=metadata_only_no_referenced_key_reads
```

Las referencias coinciden con las referencias privadas inventariadas antes.
No se leyeron valores de llaves ni se compararon módulos RSA con certificados:
la coincidencia es de metadatos, no una verificación matemática de identidad.
**No hay objeto EC entre las dos entradas analizadas de este PuKDF**. Por tanto
no respalda encontrar allí el punto P521 supuesto por el prototipo ZIP. No
prueba ausencia global de claves ECC/CA ni que PACE/CA sea obligatorio o esté
desactivado: una clave de canal puede no estar expuesta en este directorio.
El requisito previo al VERIFY6982 sigue sin identificarse.

### Alcance y resultado fiable de la consulta

--public-keys requiere ATR conectado conocido y AID conocido anunciado en EF.DIR.
Usa binding PKCS#15 nativo sin emuladores ni cachés; verifica un único PuKDF
anunciado con ruta7004 (o3F00/7004), y AID compatible si está presente. Rechaza
otro FID, ruta, duplicados, lista de más de32directorios o ausencia. Analiza ese
DF explícitamente con `sc_pkcs15_parse_df` y propaga errores antes de resumir.

Esto evita un límite de `sc_pkcs15_get_objects`: en la versión local,
`__sc_pkcs15_search_objects` omite errores al enumerar DFs y continúa la búsqueda;
un count0 por esa ruta no distingue directorio vacío de análisis fallido.
El diagnóstico no usa esa búsqueda global, ni visita PuKDFTrusted. Filtra los
objetos por pertenencia al DF analizado, valida clase/datos y limita la lista a
menos de32objetos y128nodos antes de emitir un inventario parcial.

Imprime exclusivamente índices, familia RSA/EC/other_uninterpreted, referencia,
atributo native, usage y tamaño declarado de RSA/EC. Un tamaño EC0 significaría
no determinado por estos metadatos, no una curva de cero bits. Nunca sigue las
rutas de objetos, llama read_pubkey, lee certificados u opera con una llave;
no IDs, nombres, rutas, bytes directos/raw/SPKI, parámetros propietarios ni
material de clave en la salida. No PIN/VERIFY/MSE/GA/PSO o escritura. Los datos
iniciales del binding/inventario quedan en el informe privado, no en Git/chat.

Pasaron **65 comprobaciones del helper y59 del lanzador**, sin tarjeta; helper
real compilado. Cubren SELECT7004 exacto/FID devuelto, RSA3072+EC521 sintéticos,
privacidad, error frente a vacío válido, ruta/ADF/ATR/duplicados, listas y objetos
malformados y limpieza. Los datos RSA3072/EC521 de pruebas **no son observaciones
de esta tarjeta**. Observador/permisos, helper de firma y driver intactos;
sus suites64/6/12 previas siguen vigentes, sin repetir por este cambio.

El modo Idopte PrKDF continúa bloqueando SELECT/READ7004: estos resultados
provienen del helper, no de una nueva traza del proveedor. No repetir el modo
anterior por inercia. Próximo contraste empírico, si se sigue su enumeración:
lectura PuKDF7004 acotada al tamaño confirmado y al mismo AID, sin registrar
contenido, para identificar el siguiente comando de Idopte. La enumeración
pública por sí sola no revela los pasos de C_Login. JCOP4/RSA3072 pendientes.

## Idopte lee PuKDF7004; SELECT7005 bloqueado: 13 de octubre, 21:16:07 UTC

El usuario registró el inventario público anterior en `adb68f9`, principal y
OpenSC `3dfef6704` limpios al comenzar. PuKDF7004 ya fue confirmado por ODF,
SELECT-only (transparente/4096 bytes) y análisis nativo (dos RSA2048, referencias
0x90/0x91). Se preparó un modo nuevo para observar el acceso del proveedor a
ese directorio de metadatos, sin seguir los valores/rutas de llaves.

**inspect/jcop4-idopte-pukdf** usa un opt-in independiente
`JCOP4_DIAGNOSTIC_PUKDF=1` junto a los cuatro anteriores dentro del proceso
Idopte --show-info --list-slots, stdin vacío. Modos anteriores no lo introducen,
inventario y lanzador de firma lo eliminan del entorno heredado. Configuración
inválida/sin modo padre/raw-spy impide transporte. Exige ATR validado, ADF
conocido/9000 y READ7002 previo para SELECT7004 básico exacto A4/02/00/Lc2,
total7 u8. Selección pendiente o fallida no conserva ADF para lectura.

READ BINARY caso2 básico, sin SFI/payload/SM, **hasta256 bytes por solicitud,
hasta4096 bytes solicitados, máximo19 llamadas por proceso**. Offset inicial0,
siguientes contiguos a datos devueltos; SELECT no repone presupuesto/cursor.
Buffer temporal Le+2 (máximo258), transporte correcto/tamaño2..Le+2 y SW9000/
6282 únicamente. Fallo de log, respuesta excesiva, otro SW/continuación o
transporte detiene sin GET RESPONSE/reintentos. Reconexión no reinicia permisos.
Contenido solo al cliente Idopte; sin respuestas crudas ni llaves en el registro.
PIN/MSE/GA/SM/PSO/escrituras siguen bloqueados antes del backend.

El agente ejecutó una vez, sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-idopte-pukdf READER=0`. Informe
privado `local/jcop4-idopte-pukdf-rI9D42no/report.txt`, **21:16:07 UTC**.
Inventario/compilación correctos y comparison_atr=known_jcop4. **54 APDU
transmitidas**, todas transporte0/SW9000; SELECT7004 y19 READ aceptados, último
offset0FAE. No se registran Le ni longitud final devuelta: no afirmar lectura
completa solo por las cabeceras. No nueva enumeración de algoritmos en esta
traza; los dos objetos RSA proceden del helper anterior.

```text
blocked_header=00A40200
blocked_select_ef=7005
module_exit_status=1
observer_result=events_recorded
```

**SELECT7005 no llegó a la tarjeta**: no SW de tarjeta ni contenido de7005.
El error final es compatible con el bloqueo del guard, no con un rechazo de
READ7004 ni con el VERIFY6982 anterior. El ODF de esta misma aplicación ya
identifica7005=`certificates`; el helper nativo obtuvo antes certificados
públicos sin login. Esto sigue describiendo inventario de slots, no la ruta de
C_Login; no atribuir CA/PACE a esta secuencia ni solicitar otro PIN.

Pasaron **70 comprobaciones del observador/constructor/decoder,61 del lanzador
y6 del lanzador de firma**, sin hardware. Cubren presupuesto4096 no renovable,
límite independiente19llamadas, contexto/secuencia/ATR, SW/overrun, privacidad
y opt-in inválido/sin padre. Redirector real devuelve PuKDF sintético solo al
cliente y bloquea VERIFY. Biblioteca real compilada. Helper público/operación
de firma/driver intactos; suites65/12 previas vigentes, sin repetir por documentos.
Registros privados0700/0600 bajo local/, no agregar inventarios o respuestas a Git.

Siguiente contraste con precedente aplicable: control y acceso al CDF7005
anunciado en el mismo ODF, comparando con el inventario de certificados ya
obtenido. No habilitar otros archivos por inercia ni repetir esta consulta
idéntica. Hasta ahora ningún comando de canal/autenticación de Idopte se ha
observado antes del bloqueo; el requisito de VERIFY sigue pendiente.

## CDF7005 y estado PIN sin datos; SELECT DF0F pendiente: 13 de octubre

Principal `387d233`, OpenSC `3dfef6704`, ambos limpios al comenzar; usuario hizo
commit anterior. El agente ejecutó las siguientes consultas sin PIN/GUI; no se
solicitó login, firma o escritura. Las lecturas entregan datos solo a Idopte,
no los registran. Directorio/informes privados0700/0600.

### Control y lectura CDF

SELECT-only7005, informe `local/jcop4-file7005-eK0x57Vx/report.txt`,
**21:26:20 UTC**: SELECT0/9000, transparente/**2368bytes**, tag86/
profile_uninterpreted/read_uninterpreted; conexión/inventario/compilación/
EF.ATR/EF.DIR/unlock/proceso correctos, sin READ en esa consulta. El ODF ya
identifica7005=`certificates` y el inventario nativo obtuvo certificados públicos.

Nuevo modo **inspect/jcop4-idopte-cdf**, opt-in `JCOP4_DIAGNOSTIC_CDF=1` junto a
los anteriores exclusivamente en Idopte inventario/slots/empty stdin. Requiere
ATR/ADF conocidos y READ7004 previo para SELECT7005 exacto básico/9000.
READ BINARY caso2 básico corto/extendido sin SFI/datos/SM, hasta256por petición,
presupuesto2368 y11llamadas por proceso, continuidad desde0 según datos
devueltos. SELECT/reconexión no reponen presupuesto/cursor. BufferLe+2 máximo258,
SW9000/6282 únicamente; errores/tamaño/continuación/log fallido detienen sin
GET RESPONSE/reintento. Modos anteriores siguen bloqueando7005; inventario/firma
limpian el nuevo opt-in heredado. No se siguen otras rutas de certificados.

Primera ejecución, `local/jcop4-idopte-cdf-x3Np9eLh/report.txt`,
**21:29:08 UTC**: compilación/inventario/ATR correctos, **67 APDU0/9000**,
11READ7005 aceptados (últimooffset08B6); luego SELECT00A40000/9000 y
VERIFY00200001 bloqueado antes de transmitir. Proceso/módulo0, events_recorded.
**El código0 no acreditaba inventario Cryptoki satisfactorio**: el resultado
impreso por C_GetTokenInfo fue CKR_TOKEN_NOT_PRESENT. No PIN ni respuesta de
tarjeta al VERIFY bloqueado.

### Distinguir VERIFY vacío de una credencial

Se añadieron dos etiquetas, sin cambiar permisos: `known_master_file` para
SELECT básico al3F00 conocido, y `blocked_verify_form=case1_no_data` para
00/20/00/01 de exactamente4bytes. Variante header+00 se clasifica separadamente;
otros casos como other_contents_omitted, nunca contenido/longitud/otro PINref.
La revisión anterior solo tenía cabeceras; esta consulta se repitió una vez
para recuperar esa distinción antes omitida, no para reintentar autenticación.

Informe `local/jcop4-idopte-cdf-WJMGxt0U/report.txt`, **21:31:50 UTC**: mismas
67APDU0/9000 y module_exit_status0, **SELECT MF3F00/9000 seguido de VERIFY1
case1_no_data bloqueado**. C_GetTokenInfo volvió a CKR_TOKEN_NOT_PRESENT; ningún
PIN se proporcionó o llegó a la tarjeta. Solo se confirmó la forma/contexto de
una consulta de estado. No identifica el contexto/formato usado por C_Login.

### Excepción independiente para estado, sin autenticación

Nuevo **inspect/jcop4-idopte-pin-status**, opt-in
`JCOP4_DIAGNOSTIC_PIN_STATUS=1` solo junto al modo CDF. Permite exclusivamente
00 20 00 01 (caso1 sin datos), una vez por proceso después de READ7005 y bajo
MF confirmado/ATR conocido/sin continuación pendiente. Otro canal/ref/caso,
credencial, repetición o falta de contexto se bloquean antes del backend.
Usa buffer temporal de exactamente2bytes; solo una respuesta SW1/SW2 se devuelve.
Error/tamaño distinto/log fallido detienen. No crea una sesión de login ni
transmite un PIN; la consulta no es un intento de verificación con credencial.
No cambia la prohibición de VERIFY en modos anteriores. PIN/MSE/GA/SM/PSO/
escrituras siguen bloqueados. Inventario y firma limpian el opt-in externo.

El agente ejecutó una vez:
`nix develop --offline -c make inspect/jcop4-idopte-pin-status READER=0`.
Informe `local/jcop4-idopte-pin-status-1qsDoPci/report.txt`, **21:38:03 UTC**:

```text
public_select=known_master_file
transport=0
sw=9000
diagnostic_pin_status=mf_pin1_case1_no_data
public_header=00200001
transport=0
sw=63C7
public_select=known_chipdoc_application
transport=0
sw=9000
blocked_header=00A40200
blocked_select_ef=DF0F
token_info_result=CKR_TOKEN_NOT_PRESENT
module_exit_status=0
observer_result=events_recorded
diagnostic_result=token_info_unavailable
```

69APDU transmitidas:68SW9000 y una63C7. Consulta de estado aceptada, sin PIN/
autenticación; 63C7 informa estado en ese momento, no verifica un PIN ni mide
un contador antes/después de un intento. Idopte seleccionó otra vez el AID
ChipDoc conocido y luego intentó SELECT DF0F; **DF0F no se transmitió**, no tiene
SW de tarjeta, FCP o contenido capturado. Este bloqueo nuevo impide obtener
información del token; no es el VERIFY6982 anterior ni un error del estado vacío.
No ODF leído identificaDF0F; su función todavía no se conoce. No etiquetarlo
como canal/PACE/CA/licencia ni permitir READ por su solo identificador.

El lanzador ahora analiza solo mensajes fijos de pkcs11-tool para registrar
`token_info_result` (returned/returned_uninitialized/CKR_/not_observed), sin
IDs o etiquetas. Si C_GetTokenInfo falla aunque el proceso salga0, preserva
module_exit_status0 y termina el diagnóstico con1/token_info_unavailable.
Así un proceso terminado no se confunde con un token correctamente enumerado.

Pasaron **66 helper,83 observador/constructor/decoder,69 lanzador y6 lanzador
firma**, sin hardware; biblioteca/helper reales compilados. Nuevas regresiones
cubren presupuesto CDF, clasificadores MF/VERIFY, SW-only de2bytes, contexto/
referencia/caso/repetición, opt-ins aislados, PIN con datos bloqueado y fallo de
C_GetTokenInfo con proceso0. El test previo que esperaba ausencia de cualquier
etiqueta SELECT bajo MF se ajustó a la etiqueta MF nueva; la protección de
SELECT4141 fuera de ADF se mantiene. Driver/operación firma intactos, suite12
previa vigente sin repetir. JCOP4/RSA3072 siguen pendientes.

### Investigación documental de DF0F

No referencia aplicable encontrada en código local/ZIP revisado. La fuente
primaria [License Management de Idopte](https://www.idopte.fr/licensesys/)
confirma que su middleware maneja licencias almacenadas en un archivo de la
tarjeta y que C_GetTokenInfo participa en ese sistema. **Esa página no identifica
DF0F**, por lo que no demuestra que este FID sea el archivo de licencia en CR
ni la causa de VERIFY6982. No se ejecutaron herramientas de licencias, cambios
de atributos, generación, créditos o carga. No se modificó la tarjeta.

Próximo paso concreto: SELECT-only DF0F en el AID observado para conocer su
control, o una definición primaria del perfil que identifique el archivo.
No repetir la consulta de estado ni pedir PIN por inercia. La prueba del estado
bajo MF ya respondió63C7; el siguiente bloqueo está identificado.

## SELECT-only DF0F aceptado: 13 de octubre, 21:43:59 UTC

El usuario registró el diagnóstico anterior en `d07b7c2`, principal limpio al
comenzar; OpenSC `3dfef6704` intacto. Se agregó el target fijo
`inspect/jcop4-filedf0f` y la opción --filedf0f al helper/lanzador existentes.
El agente ejecutó `nix develop --offline -c make inspect/jcop4-filedf0f READER=0`
sin PIN/GUI. Informe privado `local/jcop4-filedf0f-6FBH76f8/report.txt`:

```text
fileDF0F_select_transport=0
fileDF0F_select_sw=9000
fileDF0F_structure=transparent
fileDF0F_size=150
fileDF0F_security_format=profile_uninterpreted
fileDF0F_security_direct_tags=86
fileDF0F_standard_compact_read=uninterpreted
fileDF0F_result=selected_metadata_only_no_read
```

Inventario, compilación, conexión/lock, EF.ATR/EF.DIR, unlock y proceso0.
Exige ATR conectado conocido, AID conocido anunciado y selección exitosa de
esa aplicación. Luego envía únicamente `00 A4 02 00 02 DF 0F 00`, con máximo
256bytes de control, sin GET RESPONSE/reintentos. No READ de DF0F, binding,
VERIFY/PIN/MSE/GA/PSO o escritura. No repite la inicialización/estado de Idopte:
confirma selección bajo el AID observado, no equivalencia de todo su estado.

El analizador compartido valida83=DF0F cuando aparece; omite valores86,
identificadores/nombres, ACL y respuesta bruta. La existencia/tamaño/control
no identifican propósito ni autorizan lectura. La página oficial de licencias
citada antes no identifica ese FID; **150bytes no demuestran que sea licencia,
CAN/PACE/CA o causa de VERIFY6982**. No se deduce política de lectura de86.

Pasaron **68 comprobaciones del helper y71 del lanzador**, sin tarjeta;
helper real compilado. Nuevas regresiones verifican el comando/FID exacto,
identificador distinto en control, ATR, ausencia contextual6A82, transporte,
69xx/61xx y control malformado, sin leer/autenticar/reintentar. Observador y sus
permisos intactos: sigue bloqueando SELECT/READ DF0F. Driver/operación de firma
intactos, suites83/6/12 previas vigentes, sin repetir por este cambio.
Informes privados0700/0600, no incorporar local/ a Git.

Próximo contraste posible: permitir exclusivamente el SELECT DF0F ya observado
al proveedor, conservando READ bloqueado, para distinguir una solicitud de
lectura de otro paso. Es una observación nueva y acotada, no una autorización
para leer archivos arbitrarios. No repetir SELECT-only ni estado vacío idénticos
por inercia, ni pedir PIN mientras se investiga esta fase de inventario.
JCOP4/RSA3072 y la ruta de autenticación/firma siguen pendientes.

## Idopte SELECT DF0F aceptado, READ bloqueado: 13 de octubre, 22:03:13 UTC

Principal `3fbd854`, OpenSC `3dfef6704`, ambos limpios al comenzar antes de
preparar este cambio. La preparación fue interrumpida tras escribir cuatro
archivos y retomada sin duplicar trabajo; no había ejecución hardware de esta
versión antes de las pruebas. El usuario autorizó continuar y ejecutar los
comandos sin PIN/GUI. Se mantiene la exclusión de PIN/GUI del agente.

Nuevo modo **inspect/jcop4-idopte-df0f**, opt-in
`JCOP4_DIAGNOSTIC_SELECT_DF0F=1` solo junto al modo de estado vacío y los
anteriores dentro del proceso Idopte inventario/slots/empty stdin. Los demás
modos y el lanzador de firma eliminan la variable heredada; configuración
inválida/sin padre/raw spy bloquea transporte.

Permite **un solo SELECT básico** 00/A4/02/00/Lc2/FIDDF0F, total7 u8bytes,
ATR conectado validado, ADF conocido seleccionado/9000 y respuesta previa
válida al estado vacío en MF (9000 o63Cx). El indicador del estado se establece
solo tras transporte0 y respuesta SW-only de2bytes, y se descarta al cambiar
conexión/contexto; no equivale a autenticación. Ese estado bajo MF obliga a
volver a seleccionar ADF antes de permitir DF0F. Se requiere escribir el
marcador privado antes del transporte. SELECT revoca READ del archivo anterior
y **nunca habilita READ BINARY/RECORD de DF0F**. Presupuesto un SELECT por
proceso, sin reposición; cualquier SW distinto de9000 detiene todo transporte
posterior, incluido GET RESPONSE. Los permisos anteriores de lecturas/estado
vacío no se amplían; VERIFY con datos, MSE/GA/SM/PSO/escrituras siguen bloqueados.

Ejecución única por el agente:
`nix develop --offline -c make inspect/jcop4-idopte-df0f READER=0`, informe
privado `local/jcop4-idopte-df0f-OCE6tqH3/report.txt`, **22:03:13 UTC**:

```text
comparison_atr=known_jcop4
public_select=known_master_file
transport=0
sw=9000
diagnostic_pin_status=mf_pin1_case1_no_data
public_header=00200001
transport=0
sw=63C7
public_select=known_chipdoc_application
transport=0
sw=9000
public_select=known_df0f_metadata_only
public_header=00A40200
transport=0
sw=9000
blocked_header=00B00000
token_info_result=CKR_TOKEN_NOT_PRESENT
module_exit_status=0
observer_result=events_recorded
diagnostic_result=token_info_unavailable
```

Inventario/compilación correctos. **70 APDU transmitidas**, 69SW9000 y una63C7.
El SELECT DF0F recibió0/9000. El siguiente READ BINARY offset0 **no se
transmitió**, por lo que no hay contenido ni SW de tarjeta para esa lectura.
C_GetTokenInfo sigue indisponible; el módulo sale0 y el lanzador correctamente
termina1. Es una interrupción del guard al acceso solicitado por Idopte,
no rechazo de READ por la tarjeta ni el VERIFY6982 anterior. No se proporcionó
PIN, no hubo login, firma o escritura.

La captura del proveedor acredita selección y petición de lectura durante
C_GetTokenInfo; no identifica por sí sola el propósito del archivo de150bytes,
la seguridad86, una licencia, PACE/CA o causa de VERIFY6982. La consulta SELECT-only
anterior aporta control/tamaño, pero no una política de lectura. Se mantiene
READ DF0F bloqueado; no ejecutar consultas idénticas ni repetir PIN por inercia.
Una futura excepción de lectura necesitaría modo separado y forma/tamaño
acotados al archivo observado, sin registrar el contenido o cambiar la solicitud
del proveedor. Si se busca una definición de propósito, usar fuente primaria
aplicable al perfil, no atribuirla por una coincidencia de tamaño/FID.

Pasaron **90 comprobaciones del observador/constructor/decoder,73 del lanzador
y6 del lanzador de firma**, sin tarjeta. Incluyen secuencia/estado/ATR/ADF,
selección corta con/sinLe, READ BINARY/RECORD y PIN bloqueados, reselección,
SW61xx/6A82/69xx sin follow-up y log fallido antes de transporte. Delegación
PC/SC real con transporte simulado, selección DF0F aceptada y READ bloqueado;
constructor inválido/sin padre bloquea todo. Biblioteca real compilada. Helper
público68/operación de firma12 previos intactos, no repetir suites sin cambios.
Driver/submódulo intactos. Informes privados0700/0600 bajo local/, no publicar
inventarios completos ni agregar capturas a Git. JCOP4/RSA3072 siguen pendientes.

## Idopte READ DF0F aceptado; SELECT B001 bloqueado: 22:21:37 UTC

El usuario registró `442c70e`; principal y OpenSC `3dfef6704` limpios antes de
esta revisión. El nuevo modo `make inspect/jcop4-idopte-read-df0f READER=0`
usa el inventario habitual de Idopte, con stdin vacío y una excepción separada:
`JCOP4_DIAGNOSTIC_READ_DF0F=1`, dependiente del permiso SELECT_DF0F y todos sus
padres. Los modos previos conservan su bloqueo de READ DF0F.

La excepción exige ATR conectado conocido, ADF seleccionado, estado vacío MF
aceptado y SELECT DF0F exacto con SW9000. Permite una sola READ BINARY caso 2,
CLA00, sin SFI, offset0 y Le **exactamente 150**, corto o extendido. Ese tamaño
proviene del control observado por separado; no interpreta tag86 como política
de lectura. No modifica la APDU ni reduce un Le mayor. Reserva 152 bytes para
respuesta y SW, acepta únicamente 9000/6282 y devuelve datos solo a Idopte.
No guarda contenido en traza, informe o spy. Errores de buffer, tamaño, SW,
transporte o escritura del marker detienen el proceso sin GET RESPONSE,
reintentos ni rehabilitación por SELECT/reconexión. El lanzador elimina la
variable externa en inventario previo, todos los otros modos y firma.

Tras las pruebas, el agente ejecutó una vez:

```sh
nix develop --offline -c make inspect/jcop4-idopte-read-df0f READER=0
```

Informe privado `local/jcop4-idopte-read-df0f-BeXtNUJ3/report.txt`,
**2026-10-13T22:21:37Z**. Compilación e inventario correctos, ATR de la conexión
con Idopte conocido. Se transmitieron **73 APDU**, con transporte correcto:
72 SW9000 y un estado vacío63C7. Últimos pasos observados:

```text
public_select=known_df0f_bounded_read_comparison
public_header=00A40200
transport=0
sw=9000
diagnostic_read=known_DF0F_contents_omitted
public_header=00B00000
transport=0
sw=9000
public_header=00A40000
public_select=known_master_file
transport=0
sw=9000
public_header=00A4040C
public_select=known_chipdoc_application
transport=0
sw=9000
blocked_header=00A40200
blocked_select_ef=B001
```

El guard bloqueó SELECT B001 **antes de transmitir**: no hay SW, control ni
contenido de ese archivo. `module_exit_status=0`, pero
`token_info_result=CKR_TOKEN_NOT_PRESENT` y
`diagnostic_result=token_info_unavailable` hacen terminar el diagnóstico con 1;
`observer_result=events_recorded`. No es un rechazo de READ DF0F por la tarjeta
ni un nuevo VERIFY6982. No se introdujo PIN ni se solicitó login, MSE, GA,
secure messaging, PSO, escritura o firma. Hubo la única consulta de estado
VERIFY sin datos ya permitida; no equivale a autenticación con credencial.

Esta observación demuestra que Idopte pudo leer DF0F en esta secuencia, sin
solicitar autenticación en el proceso. No identifica el contenido, su política
general ni su necesidad para el driver libre; tampoco demuestra licencia,
PACE o chip authentication. Mantener los datos privados, sin copiarlos a Git.

El código local `pkcs15-cedulauy.c` llama B001 al certificado de firma de la
cédula uruguaya, con AID, PIN y llave distintos. Esa coincidencia de FID es una
pista **de otro perfil**, no evidencia del contenido de B001 en esta tarjeta.
La revisión estática de textos/fuentes del ZIP no encontró B001. El siguiente
contraste es obtener un resumen de las rutas de certificados de CDF7005 bajo
el AID conocido y comprobar si referencia B001; omitir IDs, nombres y datos.
No ampliar SELECT/READ B001 todavía ni repetir inventario o PIN por inercia.

Pasaron **99 comprobaciones del observador/constructor/decoder**, **75 del
lanzador público** y **6 del lanzador de firma**: 180. Incluyen respuesta solo
al cliente, SELECT-only anterior, límite de una READ, formas inválidas,
credenciales bloqueadas, SW/tamaños inválidos, log fallido, pérdida de contexto,
constructor sin padre/inválido y delegación PC/SC real con backend ficticio.
Biblioteca real compilada. Helper 68 y operación de firma 12 previos intactos;
no se repitieron sus suites. Driver/submódulo intactos. JCOP4/RSA3072 pendientes.

## CDF7005 referencia B001 y B002: 13 de octubre, 22:27:46 UTC

El usuario registró `2a30496`. Principal y OpenSC `3dfef6704` limpios antes de
la revisión. Para comprobar si el SELECT B001 bloqueado de Idopte corresponde
a una referencia de certificado, se agregó el modo
`make inspect/jcop4-certificate-paths READER=0`.

El helper exige ATR/AID conocidos y AID anunciado en EF.DIR. Usa default driver
con binding PKCS#15 nativo, cachés desactivadas y certificados privados sin
declasificar. Localiza exclusivamente el CDF7005 anunciado en ODF, comprueba
ruta de directorio única, tipo y AID, y llama directamente a
`sc_pkcs15_parse_df`. No utiliza `get_objects`, que puede ocultar errores de
análisis o buscar directorios trusted/useful. Este modo no solicita certificados
ni sigue las rutas analizadas. El binding conserva sus lecturas de bootstrap
nativas ya utilizadas en los diagnósticos anteriores.

Antes de emitir rutas, valida todos los objetos del directorio: tipo X.509,
puntero válido, rutas pares dentro del buffer, tipo PATH/FILE_ID, AID vacío o
coincidente y rangos no negativos salvo count=-1. Máximo 32 directorios, 128
objetos examinados y 32 certificados; las listas cíclicas quedan limitadas.
Datos inválidos o errores detienen, sin inventario parcial de rutas ni fallback.
Los valores inline se clasifican sin imprimir su DER. La salida conserva solo
índice de inventario, marca privado, ruta y rango; omite IDs, nombres y valores.

Pasaron **75 comprobaciones del helper y 77 del lanzador (152)**, mediante
APIs ficticias. Incluyen CDF correcto/vacío, error de análisis, directorio
incorrecto/ausente/duplicado, ATR/AID inválidos, rutas/rangos/objetos inválidos,
objeto privado sin lectura, valores inline omitidos y limpieza de recursos.
El helper real compiló offline; no se modificaron OpenSC ni el observador.
Sus 99 comprobaciones, las 6 del lanzador de firma y las 12 de operación previas
siguen vigentes, sin repetir suites intactas.

Después, el agente ejecutó una vez, sin PIN ni GUI:

```sh
nix develop --offline -c make inspect/jcop4-certificate-paths READER=0
```

Informe privado `local/jcop4-certificate-paths-HN4CjVzr/report.txt`,
**2026-10-13T22:27:46Z**. Inventario y compilación correctos, binding nativo y
análisis del CDF con resultado 0, unlock y proceso 0. Resumen permitido:

```text
certificate_paths_parse_result=0
certificate_path_metadata_count=2
certificate_path_index=0
certificate_path_private=0
certificate_storage=path_no_follow
certificate_path=B001
certificate_path_offset=0
certificate_path_count=-1
certificate_path_index=1
certificate_path_private=0
certificate_storage=path_no_follow
certificate_path=B002
certificate_path_offset=0
certificate_path_count=-1
certificate_paths_result=cdf7005_metadata_only_no_certificate_reads
```

El CDF de esta personalización referencia B001 y B002 como certificados X.509,
sin marca de objeto privado y sin rango explícito. Esto vincula la solicitud
B001 de Idopte con una referencia del CDF de la misma aplicación. No acredita
por sí solo el contenido DER real, la ACL de lectura del EF ni qué objeto es
el certificado de firma frente al de autenticación: no se imprimieron IDs ni
se leyeron los certificados. Tampoco resuelve VERIFY6982 ni demuestra qué
secuencia requiere la autenticación.

No hubo lectura de B001/B002, PIN, VERIFY, MSE, PSO, escritura o Idopte. El guard
Idopte mantiene SELECT/READ B001 bloqueados. Próximo contraste: SELECT-only
B001 bajo este AID para consultar tamaño/estructura/atributos de control antes
de decidir una observación de lectura acotada. No asumir permiso de lectura
por private=0 ni política por coincidencia con el perfil uruguayo. No repetir
el CDF o inventario Idopte idénticos por inercia. Archivos privados0700/0600
fuera de Git; driver intacto y firma JCOP4/RSA3072 pendientes.

## SELECT-only B001 aceptado: 13 de octubre, 22:39:39 UTC

El usuario registró `2cff02e`, principal y OpenSC `3dfef6704` limpios antes de
esta revisión. Después de confirmar que CDF7005 referencia B001 como X.509,
se agregó `make inspect/jcop4-fileb001 READER=0`. Reutiliza el helper SELECT-only
con default driver y exige ATR/AID conocidos y AID anunciado en EF.DIR. Tras
seleccionar ese AID, transmite **00 A4 02 00 02 B0 01 00**, sin binding ni READ.
La respuesta de hasta 256 bytes es información de control, no contenido del EF.
El analizador valida 83=B001 si está presente y resume estructura/tamaño/formas
de seguridad; no imprime valores propietarios, ACL, nombres o respuesta bruta.

Pasaron **77 comprobaciones del helper y 79 del lanzador (156)**, APIs falsas.
Incluyen FID enviado/devuelto, ATR, ausencia de binding/READ/PIN, 6A82 como
resultado contextual y fallos de transporte, SW, 61xx y metadatos truncados sin
comandos posteriores. Helper real compilado offline. Observador 99, lanzador de
firma 6 y operación 12 previos intactos; no se repitieron sus suites.

El agente ejecutó una vez, sin PIN ni GUI:

```sh
nix develop --offline -c make inspect/jcop4-fileb001 READER=0
```

Informe privado `local/jcop4-fileb001-8OxZ9U7P/report.txt`,
**2026-10-13T22:39:39Z**. Inventario, compilación y conexión correctos;
selección de aplicación, unlock y proceso correctos. Resumen permitido:

```text
fileB001_select_transport=0
fileB001_select_sw=9000
fileB001_structure=transparent
fileB001_size=3000
fileB001_security_format=profile_uninterpreted
fileB001_security_direct_tags=86
fileB001_standard_compact_read=uninterpreted
fileB001_result=selected_metadata_only_no_read
```

El archivo existe y su control anuncia 3000 bytes. Ese tamaño corresponde al
EF, **no acredita la longitud DER del certificado**, que podría ocupar solo una
parte. Su referencia X.509 en CDF7005, private=0 y SELECT9000 no especifican la
ACL de lectura ni identifican cuál certificado se usa para firma. Tag86 sigue
sin interpretación aplicable; no extrapolar política de otros perfiles.

No se leyó B001 ni se ejecutó Idopte, PIN, VERIFY, MSE, PSO o escritura. El guard
Idopte sigue bloqueando SELECT/READ B001. Siguiente contraste: preparar un modo
separado de observación Idopte con SELECT B001 observado y lectura limitada al
tamaño de este EF, rangos/presupuesto/contexto controlados y datos únicamente
al cliente. No reescribir APDU ni habilitar autenticación u otros archivos por
inercia. No repetir el SELECT o inventario idénticos sin nueva información.
Informes privados0700/0600 fuera de Git; driver/submódulo intactos. Firma JCOP4,
VERIFY6982 y RSA3072 continúan pendientes.

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
