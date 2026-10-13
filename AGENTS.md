# libjcop3p11: contexto para continuar

**JCOP4, 13 de octubre de 2026:** último informe revisado
`local/jcop4-idopte-discovery-4riCxCot/report.txt`, **15:28:07 UTC**, principal
`e332630`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario/ATR
correctos; **observer_build_status=1**, pkg-config no encontró libpcsclite.
No se cargó Idopte ni se creó public-headers.txt; no hubo nuevas APDU de
descubrimiento, lectura de D003, PIN ni autenticación. No es un rechazo de
tarjeta ni un nuevo bloqueo del observador. No se conoce la causa exacta de
la ausencia de la dependencia en la terminal usada; el flake ya la declara.
El constructor ahora comprueba pkg-config/libpcsclite antes de compilar,
muestra el comando con nix develop y conserva el fallo de consulta de flags
sin continuar por una sustitución de comando dentro de read. Pasaron **16
comprobaciones del observador/constructor** con transporte ficticio, incluidas
dos nuevas de dependencias ausentes/flags fallidos y compilador no invocado.
Lanzador y driver sin cambios; sus 30 comprobaciones previas siguen vigentes.
`nix develop --offline -c pkg-config --exists libpcsclite` terminó con 0
en este entorno; no usó hardware ni cargó Idopte.
Siguiente intervención, misma JCOP4 y otros clientes cerrados:
`nix develop -c make inspect/jcop4-idopte-discovery READER=0`.
No pedir nuevo PIN. Sigue pendiente comprobar SELECT/READ D003 en hardware
y resolver VERIFY 6982.

Informe anterior, último descubrimiento APDU observado:
`local/jcop4-idopte-discovery-FIyDDxFN/report.txt`, **15:07:25 UTC**, principal
`3423abc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Mismo contexto/
conexión 0, GET DATA nombre/versión y SELECT inicial 9000. **SELECT EF bloqueado
identificado como D003**, no transmitido, sin SW de tarjeta. Módulo 1, slot
vacío, events_recorded. No hubo PIN ni autenticación ni lectura de D003.
ANS CPS4 sección 3.1.2 y CIE 3.0 documentan D003 bajo MF como identificación/
serie legible sin autenticación; son perfiles distintos, no prueba del contenido
en CR. No universalizar el FID: OpenSC esteid2018 lo usa para número de documento.
Observador preparado para permitir **solo SELECT EF 00 A4 02 00 / D003 tras
confirmar MF**, y READ BINARY de ese archivo en canal básico, sin SFI ni datos
de entrada. Sus respuestas/seriales no se registran. MF se confirma únicamente
por selección básica de 3F00 o AID raíz conocido aceptada 9000/61xx. Una
selección absoluta de archivo bootstrap desde 3F00 también conserva contexto
MF; cambios de aplicación/canal, selecciones fallidas y reconexión lo retiran.
Forma de SELECT distinta, READ RECORD, otros canales y otros archivos siguen
bloqueados. No nuevos GET DATA/operaciones administrativas ni cambios en OpenSC.
Pasaron **14 comprobaciones del observador**; las **30 del lanzador** anteriores
siguen vigentes (lanzador sin cambios). Integración con redirector real y
transporte ficticio confirma MF/D003/READ seguidos de VERIFY bloqueado y omisión
del serial sintético. Pendiente una consulta pública con mismo Make, sin PIN.
Revisar los SW de D003 y siguiente bloqueo antes de ampliar; VERIFY 6982
sigue pendiente, no solicitar otra firma autenticada todavía.

Informe anterior antes de identificar el EF:
`local/jcop4-idopte-discovery-fZcrQuOX/report.txt`, **14:57:38 UTC**, principal
`7c046dc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Contexto y
conexión PC/SC 0. GET DATA **00 CA 01 00** y **00 CA 01 16** -> transporte
0/SW 9000; SELECT **00 A4 00 00** -> 0/9000; siguiente SELECT **00 A4 02 00
bloqueado antes de transmitir**. Segunda conexión 0, pero transporte sigue
bloqueado. Módulo 1, slot vacío, events_recorded. No es un rechazo de tarjeta
al último SELECT: no llegó a ella. No hubo PIN ni autenticación. La captura
omite los datos, por lo que **no identifica el EF solicitado** ni el contenido
de las respuestas. No permitir SELECT/lectura de cualquier archivo por inercia.
Observador v3 preparado: añade exclusivamente blocked_select_ef=XXXX para
un SELECT EF 00 A4 02 00 válido con Lc 2 y total 7/8 bytes, sin transmitirlo;
formas distintas se marcan unsupported sin registrar payload/longitud. Es
un identificador de archivo, no contenido ni credencial. Se mantienen todos
los permisos de v2. Pasaron **12 comprobaciones del observador y 30 del
lanzador**, con transporte ficticio; OpenSC sin cambios. Siguiente intervención:
consulta pública realizada en FIyDDxFN: D003 identificado y estudiado.
No repetir ese paso como primer trabajo. No repetir firmas todavía.

Informe anterior con el primer GET DATA bloqueado:
`local/jcop4-idopte-discovery-aPCZL12P/report.txt`, **14:53:29 UTC**, principal
`7f31abc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. El observador
v2 funcionó: SCardEstablishContext y SCardConnect devuelven 0; primer intento
APDU **00 CA 01 00 bloqueado antes de transmitir**. Una segunda conexión
devuelve 0, pero transporte sigue bloqueado. Idopte muestra slot vacío y
sale 1; consecuencia compatible con el bloqueo, no rechazo de la tarjeta.
No hubo PIN ni autenticación. ANSSI-CC-2018/07, sección 1.2.4, documenta
00 CA 01 00 00 como GET DATA del nombre de la aplicación ChipDoc Lite.
Es precedente público para interpretar el comando, no prueba del contenido
de la respuesta en la JCOP4 CR. Se permite únicamente caso-2 corto, CLA 00,
P1 01/P2 00, sin datos de entrada; respuestas no se registran. Los demás
tags, casos con payload, otros canales, SM y autenticación siguen bloqueados.
Pasaron **30 pruebas del lanzador y 11 del observador**, incluida la nueva
consulta a través del redirector real con transporte ficticio. Pendiente una
consulta pública realizada y revisada en fZcrQuOX; no repetir esa versión
como primer paso. No hubo cambios del driver.

Informe anterior del observador v1:
`local/jcop4-idopte-discovery-y7dskS3z/report.txt`, **14:32:27 UTC**, principal
`fbf3434`, OpenSC `3dfef6704`. Compilación del observador 0, inventario/ATR
correctos; Idopte mostró slot vacío y salió 1. public-headers.txt contiene
solo format=public_discovery_headers_v1, **ningún evento**, ni comando bloqueado.
No demuestra un rechazo APDU de la tarjeta ni que Idopte no transmitiera.
No repetir la versión v1. La prueba local reprodujo un defecto de esa ruta:
RTLD_NEXT desde una precarga sin dependencia PC/SC no encuentra el transporte
de un plugin RTLD_LOCAL. Explica una limitación comprobada del observador,
pero **no demuestra la causa exacta de la ausencia total de eventos en hardware**.
Se corrigió el diagnóstico con LIBPCSCLITE_DELEGATE y una dependencia explícita
de libpcsclite_real.so.1; añade resultados de contexto/conexión/reconexión,
sin nombres/handles/ATR/payloads. Su funcionamiento ya se comprobó en el
informe aPCZL12P anterior. En su preparación pasaron **30 pruebas del
lanzador y 10 del observador**, incluidos redirector PC/SC real, plugin local,
scope global/local, transporte ficticio y rechazo de VERIFY/control/atributos.
No se cargó Idopte ni usó hardware por el agente.

Informe normal anterior ya revisado:
`local/jcop4-idopte-6aaJ5sIO/report.txt`, **14:17:31 UTC**, principal
`697eb0e`, OpenSC `3dfef6704`; ambos Git limpios al comenzar. Idopte 6.23,
Cryptoki 2.40 reconoce **NXP / ChipDoc v3.2**, slot 0, PIN 6/14. Flags:
login required, rng, token initialized, PIN initialized; no bloqueo, último
intento ni cambio pendiente anunciados. No prueba PIN correcto, contador
exacto ni firma; solo información pública, proceso 0. Mantener etiqueta,
serial y URI privados, no copiarlos a documentación/chat. No repetir esta
consulta normal como primer paso.

Informe anterior `local/jcop4-pin-policy-pMGzFYNL/report.txt`, **14:10:23 UTC**,
principal `292c6b4`: binding y
metadatos correctos. SELECT MF y aplicación correctos; ambos GET DATA
00 CB 3F FF / CHV 1 DOCP A0 -> transporte 0/**6D00**, sin datos. Esta
consulta IAS/ECC no está soportada en los contextos probados; no se obtuvo
política. Unlock y proceso 0 indican limpieza/consultas terminadas, no una
política interpretada. Sin PIN ni autenticación. No repetir ni barrer nuevas
referencias/plantillas por inercia. No aplicar política JCOP3 a JCOP4.

Informe anterior `local/jcop4-security-info-pveu5cDK/report.txt`,
**14:03:00 UTC**, principal `83b2eda`: binding y
metadatos correctos: clase AUTH tiene Card PIN y Card Admin Key (AuthKey,
derived=1); no objeto Card CAN en esta enumeración. No usar la llave
administrativa ni interpretar su ausencia de ruta como permiso para leerla.
GET DATA de versión obtuvo transporte 0/SW 9000, **cuatro bytes 03020052**.
Coinciden exactamente con ChipDoc 3.2.0.52 en identificación pública ANSSI,
enlazada en docs/jcop4.md; no acredita por sí solo personalización/certificación
CR ni variante exacta de hardware. SELECT MF correcto; READ B0 9C 00 / Le 256
de CardAccess por SFI 1C -> transporte 0/**6A82**, no disponible en esa
consulta/contexto (no demuestra ausencia general de PACE). Unlock y proceso 0.
No hubo PIN ni autenticación. No repetir esta consulta como primer paso.

El informe anterior 9FQ37Lk3 (14:00:01, b41f16b) falló por exigir seis bytes
exactos y no llegó a CardAccess. El helper corregido registra longitud/bytes
y acepta 0–6 sin imponer formato eOI; superior al buffer detiene. Corrección
validada en el nuevo informe. No repetir el intento autenticado.

Informe autenticado anterior `local/jcop4-signature-5Dpadr1N/report.txt`,
**13:44:00 UTC**, principal `b9c5696`: metadatos y
certificado RSA 2048 validados; VERIFY vacío 63C7 y MSE 9000. **El único
VERIFY con PIN transmitido devolvió 6982**, condición de seguridad no
satisfecha. No hubo PSO ni firma. Reset y unlock dieron 0. No es la respuesta
típica de PIN incorrecto (63Cx), pero tampoco demuestra PIN correcto ni
contador intacto: solo se consultó el contador antes del intento.
**Excluir `jcop4-signature-4FGWOtpI` del diagnóstico por instrucción expresa
del usuario (error humano); preservar el original sin utilizarlo como evidencia.**
No solicitar otro intento de PIN hasta resolver el requisito de autenticación.

Evidencia anterior ya revisada:
- `jcop4-pin-status-BYJtNyTV/report.txt`, 08:10:14 UTC, principal `765ef89`:
  VERIFY vacío 00 20 00 01 -> 63C7. PIN global 1, auth_method CHV=1,
  auth_type PIN=0, UTF8=2, flags 0x11, pad 00, límites 6–14, ruta vacía.
  Sin PIN; no repetir como primer paso.
- `jcop4-sign-setup-qob4FMSh/report.txt`, 08:03:22 UTC, principal `4611409`:
  **MSE 00 22 81 B6 / 91 02 28 91 -> transporte 0/SW 9000**. TokenInfo no
  anuncia algoritmos (count=0), PrKDF tampoco refs. No demuestra firma.
- `jcop4-pkcs15-msMLoEtW/report.txt`, 07:50:37 UTC: dos DER públicos y
  metadatos sin PIN. OpenSSL confirma RSA 2048/exponente 65537; IDs asociados
  a LlaveDeFirma 0x91, autenticación 0x90, ambas nativas. PIN auth ID Card PIN
  vinculado a llave de firma. Auth ID administrativo del objeto PIN no usado.
  DER/PEM/certificate-summary.json privados junto al informe; sin validar cadena.
- `jcop4-probe-9XynOJDU/report.txt`, 07:19:50 UTC: OMNIKEY 3x21/lector 0,
  ATR `3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a`; coincide con contacto
  eOI, difiere de JCOP3/iasecc. OpenPACE deshabilitado y eOI NULL.
- `jcop4-aids-qixa9ZwC/report.txt`, 07:28:12 UTC: cuatro candidatos 6A82.
- `jcop4-files-DORk47gE/report.txt`, 07:39:47 UTC: raíz A0000000003F00,
  pre-issuing 0400ECC2 sin interpretar, máximos APDU 0/0 desconocidos;
  aplicación E828BD080FA00000036364796E6964 / ChipDoc Generic.
La generación JCOP4 la indica el usuario, RSA 2048 sí se corroboró. No activar
excepciones JCOP3/eOI por ATR/etiqueta; precedente ChipDoc ANS enlazado en docs.

**Siguiente intervención:** `make inspect/jcop4-idopte-discovery READER=0`,
desde nix develop, firmador y otros clientes cerrados y misma JCOP4 insertada.
Inventario PC/SC/ATR conocido primero, después pkcs11-tool local con
--module /run/current-system/sw/lib/SCMiddleware/libidop11.so --show-info
--list-slots. Solo información módulo/slots/tokens: sin C_OpenSession,
C_Login, objetos, PIN ni firma solicitados por la herramienta. Compila solo
tools/pcsc-public-observer.c (biblioteca de diagnóstico, no driver) y delega
PC/SC de este proceso mediante LIBPCSCLITE_DELEGATE, sin LD_PRELOAD.
Requiere libpcsclite_real.so.1 de pkg-config; no sustituye archivos del sistema.
**Nunca usar con firmador/login/firma**. Registra nombres de tres funciones
PC/SC (establecer contexto, conectar, reconectar) y resultados, con máximo
128 registros de ciclo de vida. Además, cabeceras de cuatro bytes y transporte/SW,
sin buffers de datos, tamaños, Lc/Le, PIN, hash ni respuestas completas.
**Única excepción de datos de comando en formato
v3:** identificador de dos bytes de un SELECT EF bloqueado, exclusivamente
CLA 00/INS A4/P1 02/P2 00, Lc 2 y tamaño total 7/8; otras formas solo
unsupported. No registra payloads arbitrarios ni cambia permisos por ese dato.
Bloquea antes del transporte todo
lo ajeno a la lista de descubrimiento: SELECT ISO limitado al MF/AID raíz/
aplicación observados y archivos bootstrap públicos, CA 01 16 de versión,
CA 01 00 de nombre de aplicación solo en canal básico y caso-2 corto,
READ BINARY/RECORD solo tras SELECT de 2F00/2F01/5031/5032 y GET RESPONSE
solo tras 61xx de una operación permitida. Se agrega D003 solo con SELECT
EF básico exacto desde MF confirmado; admite únicamente READ BINARY básico
sin SFI, no READ RECORD. MF/permiso se invalidan antes de cada selección y
se restablecen únicamente tras 9000/61xx con el contexto esperado.
Otros archivos, GET DATA, CLA
protegida/propietaria, VERIFY (incluso vacío), MSE, PSO, autenticación,
escrituras, SCardSetAttrib y todos los controles del lector se bloquean. Tras primer bloqueo,
error de transporte/69xx, otro handle o más de 128 APDU, no transmite más.
Log privado O_EXCL/NOFOLLOW/0600 requerido; sin log no transmite.
No usa flags JCOP3 ni helper autenticado. Stdin vacío y debug OpenSC 0.
Config y delegación aisladas: env externo OPENSC_DEEPBIND, LD_PRELOAD/trace y ambos
LIBPCSCLITE_DELEGATE/LIBPCSCLITE_SPY_DELEGATE se eliminan antes del inventario;
el delegado se activa solo en pkcs11-tool/Idopte. Conexión/reconexión descartan
permisos de lectura/respuesta anteriores; no rehabilitan transporte bloqueado.
Faltas de herramienta/módulo o error terminan sin fallback. Informe privado
local/jcop4-idopte-discovery-*/report.txt y public-headers.txt; el informe
contiene etiquetas/seriales identificables,
mantener fuera de Git y no volcarlo completo al chat. Idopte enumera todos
sus slots, que pueden ser varios por lector; no asumir que índice PC/SC
selecciona el slot PKCS#11. Se revisará reconocimiento del perfil y flags
de PIN sin tomar lo anunciado como prueba de autenticación/firma.
Un error de Idopte puede ser provocado por el bloqueo del observador: revisar
primera cabecera bloqueada antes de concluir fallo del módulo o tarjeta. No
reintentar login ni ampliar permisos del observador sin interpretar ese comando.
Si solo hay ciclo de vida, informa pcsc_lifecycle_only y falla conservando sus
resultados; si no hay eventos, no_pcsc_event_observed. Ninguno acredita
observación APDU ni éxito. Cabeceras y estado del módulo se conservan siempre.

Se pidió al usuario confirmar si esta JCOP4 ya había firmado con Idopte,
sin solicitar una nueva prueba de PIN. Confirmación pendiente en esta revisión.

El código local eoi_pin_cmd y el driver ChipDoc italiano abren PACE/CAN
antes de VERIFY; eOI lo hace sin exigir interfaz contactless en esa condición.
Es una hipótesis para 6982, **no prueba del requisito en Costa Rica**. No
activar eOI por ATR: AID/perfil/RSA difieren. También sigue sin resolverse
si el orden MSE antes de VERIFY influye. La consulta nueva obtiene evidencia
pública sin cambiar el protocolo autenticado ni el submódulo.
La revisión estática del módulo instalado muestra funciones de canal seguro
y restricciones de PIN; los nombres no demuestran qué usa esta tarjeta.
La documentación pública NXP de ChipDoc 3.2 contempla PACE con PIN/CAN/MRZ
y canales de confianza, sin resolver aún el requisito de la personalización CR.
No copiar implementación privativa ni intentar autenticación administrativa.
Las **35 comprobaciones del helper** pasaron en la preparación previa.
Ahora pasaron **30 del lanzador** y **14 del observador**, incluyendo
bloqueo anterior a transporte, privacidad, reproducción de RTLD_NEXT local y
delegación PC/SC real contra transporte falso. make test/pcsc-public-observer
y test/iasecc incluyen este último conjunto.
Biblioteca real compilada; no se cargó Idopte ni usó tarjeta por el agente.
Sin cambios ni recompilación del driver OpenSC; delegación y primer bloqueo
observados en hardware. CA 01 00/01 16 y SELECT A4 00 00 ya dieron 9000;
el siguiente SELECT A4 02 00 es D003 y quedó bloqueado. Su lectura pública
acotada está preparada, pendiente comprobar los SW en hardware.
Reconocimiento normal Idopte ya comprobado.

Herramienta autenticada anterior, conservada para investigación futura:

El lanzador compila solo tools/card-sign-test.c con util_getpass de OpenSC,
crea local/jcop4-signature-*/ (0700/0600), muestra message.txt técnico y calcula
SHA256 en digest.bin. Helper exige stdin/stdout TTY, comprueba ATR conectado,
AID anunciado y binding nativo sin cachés. Exige LlaveDeFirma RSA 2048/0x91,
nativa/uso SIGN/ruta vacía, único PIN vinculado con parámetros observados y
certificado público del mismo ID/RSA 256 bytes. Estas restricciones delimitan
el diagnóstico actual, no arquitectura final del driver. Lee DER antes de
PIN. En una sola transacción: selecciona aplicación, VERIFY vacío de estado,
MSE 91022891, **un VERIFY con PIN**, **un PSO 00 2A 9E 9A con digest de 32
bytes y Le 256**. No SELECT/lecturas de metadatos después del PIN. Sin
reintentos de VERIFY/PSO por 6C; GET RESPONSE permitido solo para respuesta
normal de PSO. Sin CAN/PACE, componentes privados ni comandos administrativos.

Tras intento autenticado llama sc_reset(card,0) para terminar autenticación,
incluso ante fallo; registra resultado y libera transacción. Borra memoria
del PIN inmediatamente tras transmisión y en rechazos locales, sin guardar
valor, longitud real o hash. Debug 0 y trazas/env JCOP3 desactivadas. Reporte
solo etapas, resultados/SW y tamaño público de firma. Un fallo detiene sin
reintentos automáticos ni variantes de protocolo. Solo acepta PSO 9000 y
256 bytes; OpenSSL verifica message.txt con el certificado de esa sesión.
Ante fallo, revisar informe antes de cualquier nuevo PIN; no pedir repetir
por inercia. No afirmación de firma exitosa hasta ver report y Verified OK.

Verificados sin hardware: helper real compiló contra OpenSC offline; **7
comprobaciones de operación/metadatos** con credenciales sintéticas y **4
regresiones del lanzador con PTY y helper/OpenSSL simulados**. Log C
build/opensc-offline/jcop4-sign-operation-tests.log. Incluidos en test/iasecc;
util-linux agregado al flake para script (PTY), sin cambiar lock. No se
repitió la suite JCOP3 ni modificó submódulo. Cambios solo en principal;
dar comandos de commit al usuario. Hubo un VERIFY JCOP4 rechazado; aún no
hay autenticación exitosa ni firma. Modularidad y transporte RSA 3072 (384 bytes) siguen
pendientes, no anunciar soporte completo.

Estado actualizado tras revisar capturas del **13 de octubre de 2026,
05:13–05:18 UTC** (12 de octubre, 23:13–23:18 de Costa Rica), OpenSC
`3dfef6704`. **Reinserción con lock_login validada entre dos firmas en una
misma GUI; ambos PDF verificados.** Principal `d26d23e` y submódulo limpios
al comenzar. Revisión documental, sin cambios de driver ni nuevas firmas
por el agente. No hace falta recompilar ni repetir pruebas como primer paso.

Última GUI lqSwrf8P: 404 eventos; VERIFY/login 117/119 OK, RSA/SignFinal
205/207 OK; presencia 208=7 (PRESENT|CHANGED|INUSE), 213=3 (PRESENT|CHANGED),
limpieza de token/sesiones. Ocho SLOT_TOKEN_REMOVED son dos rondas de cuatro
slots, no ocho retiradas. No hay muestra de ausencia. Después nueva reserva
CARD_LOCK 307, PRE_VERIFY_SELECT 312, VERIFY/login 314/316 OK, RSA/SignFinal
402/404 OK. No hereda autenticación previa ni revalida PIN automáticamente.
Dos PDF: local/jcop3-lock-hotplug-before.pdf 80482 bytes, verificación/copias
sdz525u1; after.pdf 80462, pnc0zlig, bajo local/jcop3-pdf-verification-.
Ambos CAdES con certificado exacto LlaveDeFirma y sello CMS/imprint válidos;
original/texto/render preservados. No confianza/revocación/LTV. Los eventos
no tienen tiempos individuales ni nombres de documento.

**Error inicial sin causa confirmada:** GUI M6TUGgHo 382 eventos, primera firma
OK, cambio de tarjeta y luego dos PIN_APDU_BUILD -1300/C_Login 0x7.
PRE_VERIFY_SELECT pasa; no PIN_TRANSMIT en esos rechazos. Fallan antes de
transmitir el PIN; no son respuestas de PIN incorrecto ni RSA 6982. Usuario
cree que hubo error humano; no darlo por confirmado. No registrar secreto,
longitud o hash ni alterar validación. Causa del rechazo local sigue pendiente.
Otras GUI: RjjRHcGv 207/1 firma, jVrFj373 343/2, 4CWjxEB0 207/1; todas
VERIFY/login/RSA OK, sin cambio de tarjeta registrado en esas capturas.
Se preservaron api-reviewed.log y analysis.json privados de las cinco GUI.

Perfil make firmador/jcop3-lock-login sigue optativo: lock_login=yes,
atomic=no, candidata y cachés desactivadas, debug 0. Pruebas anteriores
confirman que otro cliente espera hasta cerrar GUI y luego obtiene SELECT
9000, con seis firmas exitosas en API y un PDF verificado (37si22cp).
SW_PIN_LOGOUT_ONLY=1 omite unlock en logout; login fallido puede conservar
reserva. Cerrar GUI al terminar/ante error. Limita compartición PKCS#11 y no
prueba concurrencia independiente. Sin reserva, helper tras PIN provocó RSA
6982 aunque login fue aceptado; intención/regla interna de tarjeta desconocida.

Cambios actuales solo en principal: consulta pública JCOP4, regresiones y documentación.
Suite anterior 288 comprobaciones; seis del helper pasan en la revisión
previa. No repetir suite por documentación. Dar comandos de commit del
principal; el agente no escribe Git ni firma/lee pass. Mantener datos privados
fuera de Git. No modificar el perfil normal por defecto.

**Próximo trabajo:** revisar primeras cabeceras de descubrimiento Idopte y determinar
qué condición falta para VERIFY 6982 antes de solicitar otro PIN.
Para integrar JCOP3, revisar/separar correcciones del perfil frente
a instrumentación y experimentos antes de integrar. Diferencia local contra
base original: 21 commits, 20 archivos, +736/-77 líneas; no equivale a revisión
contra master actual. Antes de preparar reporte/PR upstream se debe reproducir
contra master actual conforme al AGENTS.md del submódulo, preservando rama y
capturas. No se ha publicado fork/issue/PR. Solo pedir intervención si hace
falta nuevo acceso de hardware/Git/red; no reclamar soporte completo.
Pendientes: rechazo local intermitente, binding/perfil por defecto, PuKDF,
compartición entre procesos, otras tarjetas/mecanismos/plataformas.

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
interna y de sus comandos. No asumir soporte de todas las JCOP3. JCOP4 se
investiga ahora como etapa separada; contactless, CAN/PACE y criptografía
poscuántica quedan fuera de las pruebas actuales.

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
| `scripts/probe-jcop4.sh` | Inventario, SELECT, archivos y PKCS#15 público con driver default |
| `scripts/build-card-public-info.sh`, `tools/card-public-info.c` | EF.ATR/EF.DIR y binding PKCS#15 nativo; certificados públicos/metadatos, sin autenticación |
| `scripts/sign-jcop3.sh` | Firma interactiva de texto y verificación OpenSSL |
| `scripts/sha256-digestinfo.sh` | Prepara DigestInfo SHA-256 DER para `RSA-PKCS`, sin PIN |
| `scripts/firmador-jcop3.sh` | Abre firmador con módulo local y configuración PKCS#15, sin depuración APDU |
| `scripts/peer-context-jcop3.sh` | Segundo cliente: solo SELECT raíz IAS, sin PIN/firma y debug 0; comprueba SW 9000 |
| `scripts/test-iasecc.sh`, `tests/` | Regresiones sin tarjeta |
| `docs/jcop3.md` | Evidencia técnica y evolución de la investigación |
| `docs/jcop4.md` | Alcance y pasos de investigación de la nueva tarjeta |
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
`firmador/jcop3-pin-status-in-place`, `firmador/jcop3-lock-login` y
`firmador/jcop3-cli` utilizan
la compilación existente: **no recompilan**.

Las restricciones del entorno del agente pueden impedir usar el daemon de
Nix o acceder a red. Una compilación sin PC/SC sirve para comprobar código,
pero no valida comunicación con la tarjeta.

## Git al momento de esta documentación

Los últimos commits guardados son:

- Repositorio principal: **`775ad54`**, `Inspect public card application metadata`.
- Submódulo OpenSC: **`3dfef6704`**, `Select ChipDoc before VERIFY`.
- Base upstream original: `c7a377af971df03f879c1697e91ebac1f7e28c56`.

Ambos árboles estaban limpios al comenzar esta revisión. Principal agrega
el binding nativo público JCOP4 y documenta su alcance/modularidad requerida. El
submódulo queda intacto; no recompilar por este cambio. Dar comandos de commit
del principal. No hay commits, logins o firmas ejecutados por el agente.

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

La validación previa de JCOP3 tiene **288 comprobaciones de regresión** en dieciséis fixtures C: `iasecc-fci.c`,
`iasecc-pin-path.c`, `iasecc-key-path.c`, `iasecc-sdo.c`, `iasecc-pin-policy.c`
y `iasecc-sign-setup.c`, más `iasecc-rsa-pkcs.c`, `pkcs11-api-trace.c`,
`pkcs11-session-state.c`, `iasecc-context-trace.c`,
`iasecc-post-login-status.c`, `pkcs15-pin-status-in-place.c`,
`pkcs11-card-change.c`, `iso7816-pin-trace.c`, `iasecc-pin-application.c`, `pkcs11-login-config.c`, y los scripts
`sha256-digestinfo.sh`, `firmador-launcher.sh`, `peer-context-launcher.sh` y `pkcs11-sign-fallback.sh`, este último con un módulo C
PKCS#11 completamente simulado. Incluyen respuestas
capturadas, conservación/copia de metadatos, rechazo de perfiles distintos y
transporte simulado que verifica que cambio/desbloqueo no autentican.
Pasaron en la comprobación local; también terminó la compilación completa
sin PC/SC, registrada en `build/opensc-offline/pin-application-build.log`.
Las últimas regresiones están en `build/opensc-offline/login-lock-tests.log`.
El checker de configuración se ejecuta desde los casos del lanzador.
Esto no sustituye pruebas de firma en hardware.
Se agregaron trece comprobaciones de `tests/jcop4-probe-launcher.sh`, que
pasaron por separado sin lector: comandos limitados a inventario/SELECT,
aislamiento de configuración/stdin, validación de ATR/SW, errores sin
reintentos, argumentos y compilación local.
Hay además quince comprobaciones de `tests/card-public-info.c`, con API
simuladas, que verifican perfil default, ATR, secuencia, rechazo sin login,
errores y liberación de transacción, binding nativo sin emuladores, certificados
privados omitidos y metadatos RSA 3072/referencia distinta sin prueba de RSA.
El runner `scripts/test-iasecc.sh`
incluye ambas; no se repitieron las 288
anteriores al agregar esta consulta pública.
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
| **`jcop3-firmador-api-und1k27k/api-reviewed.log`** | 12 de octubre, 08:02:29 UTC, 8842f394b limpio: 463 eventos revisados; dos CARD_CHANGED procesados y limpieza del token, dos VERIFY reales y dos RSA de 256 bytes; reinserción rápida y ambos PDF/sellos verificados |
| **`jcop3-firmador-api-Z0aJ18Ge/api-reviewed.log`** | 12 de octubre, 08:11:29 UTC, 8842f394b limpio: primer CLI, 174 eventos; VERIFY/login/RSA OK, llave 3 de 2048 bits, firma de 256 bytes y PDF/sello verificados |
| **`jcop3-firmador-api-tYcckn70/api-reviewed.log`** | 12 de octubre, 08:18:56 UTC, 8842f394b limpio: 1170 eventos, seis VERIFY y RSA exitosas, cuatro cambios con limpieza; cinco PDF previstos y un extra, todos con firma/sello verificados |
| **`jcop3-firmador-api-GbaOMELO/api-reviewed.log`**, **`jcop3-firmador-api-j0FPyGAS/api-reviewed.log`** | 08:35/08:36 UTC, 8842f394b limpio: 241/145 eventos, cuatro/dos logins CKR_ARGUMENTS_BAD; VERIFY -1300, sin firma ni fallos de lock; causa pendiente de marcas ISO |
| **`jcop3-firmador-api-PkFq1kEy/api-reviewed.log`** | 08:51:29 UTC, c052ee925 limpio: 147 eventos, dos builds -1300, ningún PIN_TRANSMIT o firma; rechazo local confirmado, comparación CLI ya pasó en DgJlCm1N |
| **`jcop3-firmador-api-DgJlCm1N/api-reviewed.log`** | 08:56:49 UTC, c052ee925 limpio: 176 eventos, build/VERIFY/login/firma exitosos vía CLI; PDF/sello verificados en 0xwdzxbh; rechazo GUI aún sin causa establecida |
| **`jcop3-firmador-api-Ie57KYQq/api-reviewed.log`** | 09:00:25 UTC, c052ee925 limpio: 210 eventos preservados, build/VERIFY/login/firma GUI exitosos; doc-firmado.pdf verificado/copias en bkt8tbsu; causa del rechazo previo desconocida |

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

## Interferencia de otro cliente: nueva candidata

XVK9ZA8V: 582 eventos, tres logins con VERIFY real 9000; RSA 9000/6982/9000.
xDibKvHa: 230 eventos, login real 9000 y RSA 6982. Locks/presencia normales,
sin retirada o atajo VERIFY_CACHED. Helper SELECT raíz 9000 en los informes
Bpr8civj/9oASsRGl/HqnS20vq/rhal9Uiu; el usuario reporta ejecución con diálogo
PIN abierto. El mecanismo exacto que pierde autorización no está demostrado.
La candidata agrega iasecc_chipdoc_select_for_verify después de merge_policy
antes de iasecc_chv_verify, solo opt-in y perfil/política conocidos. No afecta
GET_INFO o VERIFY vacío; errores de SELECT/política paran antes del PIN.
Diecisiete regresiones modelan CHV aceptado pero RSA sin autorización desde
raíz, control anterior, nueva secuencia, transporte/SW/PIN incorrecto sin
reintentos, fallo de política/consulta vacía y nueve perfiles excluidos.
Son simulaciones de contexto, no criptografía/hardware. La reproducción
PL8z86NE ya confirmó PRE_VERIFY_SELECT → VERIFY → RSA 9000 y PDF verificado
tras helper antes del PIN. Helper posterior al PIN todavía falla en 6xTEQpUr.
Mantener candidata optativa. Próximo protocolo en el encabezado de este
archivo **tiene prioridad sobre los pasos históricos que siguen**.

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
3. **Reinserción rápida corregida y validada en hardware; no repetir el caso.**
   Captura `und1k27k`, OpenSC 8842f394b limpio, analysis.json y copia estable
   api-reviewed.log con 463 eventos. Primera firma en línea 227; presencia
   PRESENT|CHANGED en 228 y 233, limpieza del token en 229 y luego slots vacíos.
   No se observó ausencia: el pase procesa cambio aun sin capturar tarjeta fuera.
   VERIFY real en 133 y 331, logins OK en 134 y 332, segunda firma en 425.
   Cada login tiene cuatro consultas físicas antes de firmar y ningún SELECT
   raíz en ese intervalo. Todos los locks y consultas de sesión/slot pasan.
   Ocho SLOT_TOKEN_REMOVED son dos rondas de cuatro slots, solo una con b=1;
   no representan ocho retiradas. No hay atajo por hash ni omisión de consultas.
   Los PDF `local/jcop3-hotplug-fixed-before.pdf` (80465 bytes) y
   `jcop3-hotplug-fixed-after.pdf` en raíz (80485 bytes) están verificados:
   informes privados `local/jcop3-pdf-verification-o7f8m3jn/report.json` y
   `local/jcop3-pdf-verification-m9wzm0yz/report.json`. Ambos usan LlaveDeFirma,
   CMS/imprint SHA-512 válidos y preservan original, texto y render sin anotaciones.
   No se validaron confianza/revocación. La raíz se conserva y queda ignorada.
   No hubo cambios del driver en esta revisión ni nuevas firmas del agente.
   Los fallos previos (`9ujB6ts6` y `y17eLwTD`) quedan como evidencia, no como
   bloqueo actual del caso. slot.c excluye CARD_CHANGED del atajo no_change,
   limpia ese estado al procesar el cambio y usa retry por llamada. La corrección
   afecta la capa PKCS#11 general; no modifica VERIFY/RSA ni el opt-in de firma.
   La prueba real cubre una reinserción y la simulación cinco reemplazos;
   múltiples ciclos reales, otros lectores y concurrencia siguen pendientes.
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
6. **CLI ya probado: no repetir el mismo caso como primer paso.** Captura
   `Z0aJ18Ge`, 174 eventos; copia api-reviewed.log y analysis.json. VERIFY real
   en 96, C_Login OK en 97, llave 3 RSA 2048 en 165 y C_SignFinal OK en 174.
   SHA256-RSA-PKCS, RSA AT 9000 y 256 bytes. Dos consultas físicas después
   del login sin SELECT raíz. No hay atajo por hash o errores de lock/sesión.
   PDF local/jcop3-cli.pdf de 76918 bytes, informe privado
   `local/jcop3-pdf-verification-fttds461/report.json`: certificado LlaveDeFirma,
   firma CMS y sello RFC 3161/imprint SHA-512 válidos, original/texto/render
   conservados; no confianza/revocación/LTV. El usuario ejecutó pass por stdin,
   el agente no leyó el gestor ni firmó. El wrapper conserva stdin hacia
   firmador y rechaza salida existente; no registra el PIN. El tamaño distinto
   al PDF GUI no demuestra un error: la revisión CAdES mide 34916 bytes frente
   a los 38403 de las pruebas GUI, y contenido/render coinciden. No se atribuye
   esa diferencia a una causa concreta. No inferir identidad byte a byte de archivos firmados por ambos
   modos. Nueve regresiones del lanzador y helper TTY siguen vigentes.
7. **Selección ChipDoc antes de VERIFY validada tras helper previo al PIN.**
   PL8z86NE y PDF peer-fixed verificado. No repetir ese control automáticamente.
   Helper posterior al PIN en 6xTEQpUr: login OK, RSA 6982. No es rechazo local
   ni PIN incorrecto demostrado. No activar revalidación automática del PIN.
8. **Reserva de tarjeta desde login observada y firmas exitosas.** Helpers
   esperan hasta cerrar GUI y terminan 9000; control sin GUI pasa directamente.
   Seis operaciones RSA/SignFinal OK, con dos pares consecutivos en API.
   Nuevo doc-firmado.pdf verificado/copias en 37si22cp. No repetir estos casos.
   lock_login sigue optativo: limita compartición y puede durar hasta cerrar
   GUI; no prueba concurrencia independiente ni regla interna de tarjeta.
9. **Reinserción con transacción mantenida validada.** Última GUI lqSwrf8P,
   cambio de tarjeta entre dos firmas, nueva autenticación y RSA/SignFinal OK.
   Ambos PDF lock-hotplug-before/after verificados y copiados sdz525u1/pnc0zlig.
   No repetir caso ni compilar por esta revisión documental. Error inicial
   M6TUGgHo: dos rechazos locales -1300 antes de transmitir PIN; causa pendiente,
   no atribuir a error humano solo por la impresión del usuario.
10. **Consolidar integración.** Revisar 21 commits/20 archivos contra base local,
    separar correcciones, instrumentación y experimentos. Preservar rama y
    reproducir contra master actual antes de preparar reporte/PR según
    vendor/opensc/AGENTS.md. No cambiar configuración por defecto todavía;
    lock_login limita compartición. PuKDF y otros mecanismos/plataformas no
    validados. Si cambia driver, commit de submódulo primero y principal después.
    No generar llaves ni pedir otro PIN sin motivo nuevo.

**Firma RSA/PDF, consecutivas, reserva frente a otro cliente y reinserción
están respaldadas para la tarjeta/lector y perfil optativo probados. No se
reclama soporte completo ni solución del rechazo local intermitente.**

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
- `doc.pdf`, `doc-firmado.pdf` y los PDF de prueba /jcop3-*.pdf son archivos
  del usuario excluidos de Git. No incorporarlos a commits.
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
