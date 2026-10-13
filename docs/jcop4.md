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
