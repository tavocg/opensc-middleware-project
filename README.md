# libjcop3p11

Proyecto de middleware libre para firma digital de Costa Rica, desarrollado
sobre OpenSC, centrado inicialmente en la aplicación IAS/ECC de la JCOP3
observada. Su identificador es `libjcop3p11`; el entorno de `nix develop`
conserva el prompt de Bash y le antepone `(libjcop3p11)`.
El driver de tarjeta es `iasecc` y el módulo PKCS#11 compilado se llama
`opensc-pkcs11.so` en GNU/Linux.

Primer objetivo: reconocer una tarjeta JCOP3 emitida en Costa Rica, leer su
certificado y producir una firma verificable mediante PKCS#11 de OpenSC.
La enumeración pública funciona y se obtuvo una firma RSA SHA-256 verificada
con OpenSSL en la tarjeta observada.

## Punto actual para continuar

Con OpenSC `3dfef6704` y el perfil optativo `lock_login`, se verificaron
PDF firmados con `LlaveDeFirma`, firmas consecutivas y una retirada/reinserción
entre dos firmas en la misma GUI. Las últimas pruebas son del 12 de octubre
de 2026 a las 23:18 de Costa Rica. La validación de JCOP3 incluye 288 comprobaciones sin
tarjeta; estas no sustituyen las pruebas reales descritas en
[docs/jcop3.md](docs/jcop3.md).

Para usar ese perfil con la compilación existente:

```bash
nix develop
make firmador/jcop3-lock-login
```

El perfil mantiene consultas físicas sin reseleccionar la aplicación después
del login, verifica el PIN en cada login y reserva la tarjeta hasta cerrar
la GUI en las pruebas realizadas. Otros clientes pueden esperar durante
esa reserva. Mantiene cachés de PIN/archivos desactivadas y depuración 0.
Es optativo y limita el uso simultáneo por otros programas. El objetivo
normal `firmador/jcop3` conserva su configuración anterior.

Un rechazo local intermitente `CKR_ARGUMENTS_BAD` al preparar VERIFY sigue
sin causa confirmada; los últimos dos rechazos no transmitieron el PIN.
Ante error, detenerse y revisar registros, sin reintentos automáticos.
PuKDF, integración por defecto, otras tarjetas, plataformas y mecanismos
siguen pendientes. No hay soporte completo ni cambios publicados upstream.

Consultar [AGENTS.md](AGENTS.md) para continuar desde las capturas recientes.
No repetir pruebas de firma/reinserción ya verificadas como primer paso.

La JCOP4 disponible tiene dos certificados **RSA 2048**, corroborados con
OpenSSL. Se leyeron PKCS#15 y certificados sin PIN: LlaveDeFirma referencia
`0x91`, autenticación `0x90`, PIN global 1. Su AID ChipDoc
`E828BD080FA00000036364796E6964` y referencias difieren de JCOP3.
La selección temporal RSA/SHA-256 obtuvo `9000`; VERIFY vacío devolvió
`63C7`, con siete intentos disponibles, PIN global 1 y formato UTF-8.
El primer intento autenticado devolvió **6982 en VERIFY**, sin llegar a
firmar. La consulta pública posterior obtuvo versión `03020052` y CardAccess
no disponible por SFI `1C` desde raíz (`6A82`). La consulta IAS/ECC de política
del PIN devolvió `6D00` en raíz y aplicación, sin obtener datos. Idopte reconoce
ChipDoc v3.2, anuncia PIN inicializado y límites 6–14. El directorio `5031`
confirmó referencias a `7001` (autenticación), `7002` (metadatos de llaves
privadas), `7004` (llaves públicas) y `7005` (certificados); no a `4141` en el
bloque leído. SELECT `4141` obtuvo `9000`: es un archivo transparente de
400 bytes en la aplicación conocida. Su propósito y contenido siguen sin
identificarse; no se leyó. Idopte también lo seleccionó con `9000` y después
intentó READ BINARY: **el observador lo bloqueó antes de transmitirlo**.
La consulta de seguridad posterior terminó correctamente y produjo
`profile_uninterpreted`: el helper reconoció atributos dependientes del perfil,
sin resolver su política de lectura. No prueba que requiera PACE ni que el
archivo sea público. Se mantiene el bloqueo de READ; no repetir estas consultas
sin el resumen ampliado.

El usuario confirmó que **esta misma JCOP4 ya firmó satisfactoriamente con
Idopte instalado**, en una prueba previa. No se ejecutó otra firma para
comprobarlo ni se identificó todavía el protocolo de autenticación.
La última captura, `wYs4iWG8` (**17:33:49 UTC**), funcionó y fue decodificada
por `pcsc-spy`: SELECT 4141 respondió `9000`, con FCI exterior `6F`, archivo
transparente de 400 bytes y atributo **86 = 00 FF FF**. Idopte salió 1 cuando
el observador bloqueó el siguiente READ BINARY antes de transmitirlo; eso no
es un fallo de captura ni una respuesta de rechazo de la tarjeta a READ.
No hubo lectura de 4141, PIN ni autenticación. El guard emite solo este
intercambio en formato compatible; no carga libpcscspy ni captura todo el tráfico.

ISO/IEC 7816-4 clasifica 86 como formato propietario. Los bytes no bastan para
interpretar la política de acceso o identificar el archivo. **No repetir esta
consulta por esos bytes**. Sigue pendiente la definición
ChipDoc de 4141/86 y del contexto requerido antes de VERIFY, mediante el User
Guide Manual ref. 678510 o documentación del perfil BCCR. READ sigue bloqueado.
La pregunta sobre acceso a esa documentación está pendiente de respuesta.

La revisión estática posterior del ZIP `local/driverfd.zip` aportado por el
usuario añade una hipótesis de **Chip Authentication/ECDH antes de VERIFY**.
Su MSE contiene un OID CA/AES-128 aunque el prototipo lo llama 3DES; no incluye
las capturas originales que permitan corroborarlo. eOI usa PACE/CAN antes de
VERIFY incluso por contacto, pero su etiqueta ChipDocLite es una constante del
driver y no identifica por sí sola el perfil CR. El usuario aclaró que la firma
exitosa en **otra JCOP4** fue solo APDU. El candidato encontrado es
`python3 sign_llavefirma.py --sign`: verifica el PIN antes de MSE, a diferencia
de nuestra prueba fallida. No se ejecutó el ZIP ni cambió el driver.

Se preparó un diagnóstico que cambia solo ese orden, conservando el formato
de PIN y los comandos MSE/PSO ya usados con la tarjeta actual. Comando histórico,
**ya ejecutado; no repetir ahora**:

```sh
nix develop -c make sign/jcop4-verify-first READER=0
```

Firma un texto técnico y exige verificación local con OpenSSL; no firma PDF.
Un rechazo VERIFY detiene antes de MSE/PSO, sin reintentos. No copia el
truncamiento/relleno del PIN ni las referencias alternativas del ZIP, ni lee
4141/D004. El helper ya prescinde de 4141/86 y PuKDF; omitirlos no elimina un
rechazo de autenticación.

Resultado en `local/jcop4-signature-6m61lek4/report.txt`, 19:00:58 UTC:
metadatos/certificado correctos, estado 63C7, **VERIFY 6982 antes de MSE**;
no MSE/PSO, reset/unlock correctos. Cambiar el orden no resolvió el fallo.
La revisión adicional del ZIP encontró un MAC calculado pero omitido del
APDU protegido y mensajes de canal establecido sin confirmación con tarjeta.
No usar esas ramas como prueba de canal funcional. Hace falta corroborar
su preparación mediante documentación del perfil. El usuario confirmó que
solo conserva el ZIP, sin registros originales. No pedir repetir el PIN ni
usar mensajes del prototipo como confirmación de canal.

Por solicitud posterior del usuario, se preparó una comparación empírica
con Idopte. Con la misma JCOP4 y firmador/otros clientes cerrados:

```sh
nix develop -c make inspect/jcop4-idopte-compare READER=0
```

**Este modo amplía la observación:** permite a Idopte leer hasta 400 bytes
de 4141, sin guardar su contenido, para ver qué comando intenta después.
No deduce que sea público por 00FFFF. Mantiene bloqueados PIN, autenticación,
firma y escrituras; no introducir PIN ni usarlo con firmador. Los modos
anteriores conservan el bloqueo de esa lectura. Informes privados bajo
local/jcop4-idopte-compare-*/; un error de Idopte por el siguiente comando
bloqueado puede ser esperable. Detalles y comparación en docs/jcop4.md.
Pasaron 41 comprobaciones de observador, 43 del lanzador y 6 de aislamiento
del lanzador de firma, sin tarjeta. Driver y helper de firma intactos.

La comparación ejecutada permitió dos READ de 4141 (9000) y encontró un
SELECT posterior de **4109**, bloqueado por el guard. Su consulta independiente
`make inspect/jcop4-file4109 READER=0` confirmó un EF transparente de **17 bytes**,
SELECT 9000 y atributo propietario 86 sin interpretar. Solo obtiene metadatos:
no lee 4109 ni establece autenticación. Informe privado
local/jcop4-file4109-LKNy75nY/report.txt; 58 comprobaciones del helper y 45 del
lanzador correctas. El agente puede ejecutar estos diagnósticos sin PIN/GUI,
según autorización vigente en AGENTS.md. JCOP4 aún no firma con el driver libre.

El modo de comparación permite ahora SELECT 4109 exclusivamente después de
una lectura acotada de 4141 en la aplicación conocida. La ejecución posterior
confirmó SELECT 4109 -> 9000 y un intento de READ BINARY de 4109 bloqueado
antes de transmitir. **No se habilita READ 4109**; no fue un rechazo de tarjeta.
Pasaron 44 comprobaciones del observador y 45 del lanzador; no hubo PIN ni firma.

Nuevo modo independiente `make inspect/jcop4-idopte-read4109 READER=0`: permite
una sola solicitud READ de 17 bytes de 4109, además de la lectura acotada de
4141. Mantiene bloqueadas autenticación/firma y omite ambos contenidos de los
registros. El modo compare anterior sigue sin leer 4109. La ejecución real
obtuvo 9000 en esa lectura y encontró un SELECT posterior de **7001**, bloqueado
antes de transporte. El ODF observado identifica 7001 como directorio de
objetos de autenticación; aún no explica VERIFY6982. Pasaron 52 comprobaciones
del observador, 47 del lanzador y 6 de aislamiento del lanzador de firma.

Se agregó `make inspect/jcop4-file7001` para metadatos SELECT-only (9000,
transparente, 300 bytes, seguridad 86 sin interpretar) y el modo separado
`make inspect/jcop4-idopte-aodf READER=0` para observar la lectura de ese
directorio PKCS#15, con máximo dos READ contiguos/300 bytes y sin guardar
contenido. Idopte obtuvo 9000 en ambos READ y luego intentó SELECT7002, que
el guard bloqueó. El ODF identifica 7002 como directorio de metadatos de
llaves privadas; todavía no se observó la ruta de autenticación del proveedor.
Pasaron 59 comprobaciones del helper, 57 del observador, 51 del lanzador y 6
de aislamiento del lanzador de firma. OpenSC/driver intactos.

`make inspect/jcop4-file7002` confirmó SELECT9000 y un directorio transparente
de 2048 **bytes**; ese tamaño no es una longitud de llave RSA. El modo separado
`make inspect/jcop4-idopte-prkdf READER=0` permite leer sus metadatos hasta
2048 bytes en diez solicitudes contiguas de hasta256 bytes, sin registrar el
contenido ni autenticar. La ejecución obtuvo 9000 en diez READ y encontró
SELECT7004, bloqueado antes del transporte. ODF7004 es el directorio de llaves
públicas; sus algoritmos todavía no se contrastaron con esta observación.
Pasaron 60 comprobaciones del helper, 64 del observador, 55 del lanzador y 6
del lanzador de firma. El driver libre todavía no firma con esta JCOP4.

`make inspect/jcop4-file7004` confirmó un PuKDF transparente de 4096 bytes,
SELECT9000 y seguridad86 sin interpretar. `make inspect/jcop4-public-keys`
analiza únicamente ese directorio anunciado y resume algoritmos/referencias,
sin seguir rutas de llaves ni imprimir IDs, etiquetas o material de llaves.
Resultado: **dos objetos RSA2048 nativos, referencias0x90/0x91; ningún objeto
EC en ese directorio**. No identifica la clave P521 del prototipo ZIP ni
descarta otra clave de canal fuera de PuKDF7004. Pasaron 65 comprobaciones
del helper y 59 del lanzador; observador y driver intactos.

`make inspect/jcop4-idopte-pukdf READER=0` añade una excepción separada para
leer PuKDF7004 hasta4096 bytes/19 solicitudes contiguas de hasta256 bytes,
sin guardar contenido ni autenticar. La ejecución real obtuvo 9000 en19 READ
y encontró SELECT7005, bloqueado antes de transmitir; ODF7005 es el directorio
de certificados. Los modos anteriores mantienen7004 bloqueado. Pasaron70
comprobaciones del observador,61 del lanzador y6 del lanzador de firma.

`make inspect/jcop4-file7005` confirmó un CDF transparente de2368bytes.
`make inspect/jcop4-idopte-cdf` permite su lectura acotada (máximo2368bytes/
11peticiones); Idopte llegó después a MF/3F00 y VERIFY1 sin datos, bloqueado.
Aunque el proceso salía0, C_GetTokenInfo devolvía CKR_TOKEN_NOT_PRESENT: el
lanzador ahora registra ese resultado y devuelve error si el inventario falla.
Modo separado `make inspect/jcop4-idopte-pin-status READER=0` permite únicamente
ese VERIFY vacío una vez en MF, nunca PIN/login. Obtuvo63C7 y después Idopte
intentó SELECT DF0F bajo la aplicación conocida, bloqueado antes de transmitir.
DF0F sigue de propósito desconocido. Pasaron66 comprobaciones del helper,
83 del observador,69 del lanzador y6 del lanzador de firma. No firma JCOP4 nueva.

`make inspect/jcop4-filedf0f READER=0` consulta únicamente el control de DF0F
bajo el AID conocido. Resultado: SELECT9000, transparente,150bytes, seguridad86
sin interpretar. Sin READ, binding o PIN; esto no identifica contenido ni permiso
de lectura. Pasaron68 comprobaciones del helper y71 del lanzador. El guard
El modo de descubrimiento original bloquea SELECT/READ DF0F; las excepciones
optativas se describen a continuación. Driver y protocolo de firma intactos.

Nuevo modo `make inspect/jcop4-idopte-df0f READER=0`: permite solo un SELECT
DF0F tras estado vacío satisfactorio en MF y selección del AID conocido.
La ejecución real obtuvo9000 y después intentó READ BINARY offset0, bloqueado
antes de transmitir. No contenido DF0F ni PIN; C_GetTokenInfo continúa con
CKR_TOKEN_NOT_PRESENT por el bloqueo del diagnóstico. Pasaron90 comprobaciones
del observador,73 del lanzador y6 del lanzador de firma. Modos anteriores
mantienen DF0F bloqueado; el driver libre aún no firma con esta JCOP4.

`make inspect/jcop4-idopte-read-df0f READER=0` agrega una excepción separada
para una única lectura de DF0F, offset0/Le150, con contenido solo al cliente y
sin PIN ni login. La prueba real obtuvo SELECT y READ9000; después Idopte
seleccionó MF/ADF e intentó SELECT B001, bloqueado antes de transmitir.
C_GetTokenInfo sigue con CKR_TOKEN_NOT_PRESENT por el bloqueo. B001 aún no está
identificado para este perfil; el próximo contraste es su posible referencia
en CDF7005. Pasaron **180 comprobaciones** de los componentes modificados.
El driver libre sigue pendiente de firma JCOP4; no se cambió OpenSC.

`make inspect/jcop4-certificate-paths READER=0` analiza solo las referencias
de certificados de CDF7005. La consulta real confirmó rutas **B001 y B002**,
ambas sin marca de objeto privado; no siguió las rutas ni leyó certificados.
Eso vincula el SELECT B001 de Idopte con el CDF de esta tarjeta, pero no
identifica su ACL ni cuál certificado sirve para firma. Pasaron **152
comprobaciones** del helper/lanzador modificados. El próximo contraste es
SELECT-only B001; el observador conserva su bloqueo de ese archivo.

`make inspect/jcop4-fileb001 READER=0` consulta únicamente el control de B001.
La prueba real obtuvo **9000, transparente, 3000 bytes**, con seguridad86 sin
interpretar. No leyó el certificado ni pidió PIN; el tamaño anunciado es del EF,
no necesariamente del DER. Pasaron **156 comprobaciones** del helper/lanzador.
El próximo contraste es observar la lectura acotada de B001 solicitada por
Idopte; sus permisos actuales siguen bloqueando SELECT/READ de ese archivo.

`make inspect/jcop4-idopte-b001 READER=0` habilita una excepción separada para
SELECT B001 y lecturas contiguas, hasta 3000 bytes solicitados/16 solicitudes,
con datos solo al cliente. La prueba real obtuvo SELECT9000 y **14 READ9000**;
luego bloqueó SELECT B002 antes de transmitir. No PIN ni login; C_GetTokenInfo
sigue con CKR_TOKEN_NOT_PRESENT por el guard. Pasaron **197 comprobaciones**
de los componentes modificados. Próximo contraste: SELECT-only B002 para
conocer sus metadatos; no asumir el tamaño de B001. Driver libre JCOP4 pendiente.

`make inspect/jcop4-fileb002 READER=0` consulta únicamente el control de B002.
Resultado real: **9000, transparente, 3000 bytes**, seguridad86 sin interpretar.
No leyó el certificado ni pidió PIN. Pasaron **162 comprobaciones** del helper/
lanzador modificados. Próximo contraste: observación separada y acotada de la
lectura B002 solicitada por Idopte; el guard actual todavía la bloquea.

`make inspect/jcop4-idopte-b002 READER=0` observa SELECT/READ B002 con un
presupuesto propio de 3000 bytes/16 solicitudes, sin registrar el certificado.
La prueba real obtuvo **14 READ9000**; luego una segunda conexión PC/SC activó
la barrera global del guard. C_GetTokenInfo sigue con CKR_TOKEN_NOT_PRESENT,
sin nuevo rechazo APDU ni PIN. Pasaron **213 comprobaciones** de los componentes
modificados. El próximo contraste es caracterizar esa conexión y su primer
comando omitido, conservando los límites de transporte y privacidad.

Capturas privadas bajo local/jcop4-idopte-spy-*/; no agregarlas a Git.
Pasaron previamente **30 comprobaciones del observador/decoder y 40 del
lanzador** con transporte ficticio. El diagnóstico de firma pasó además
**12 comprobaciones de operación y 6 de lanzador**, y su helper compiló offline.
Esas regresiones usan transporte ficticio; OpenSC/driver intactos. Evidencia y fuentes en
[docs/jcop4.md](docs/jcop4.md).

El descubrimiento Idopte previo (`make inspect/jcop4-idopte-discovery`) compila
solo un observador y consulta información de módulo/slots/tokens, sin login
ni PIN. El observador guarda cabeceras/estados y bloquea comandos ajenos al
descubrimiento permitido; **Idopte puede fallar por ese bloqueo**. No usar
el observador para firmar. Usa `LIBPCSCLITE_DELEGATE`, sin precarga ni cambios
al sistema, y registra resultados de conexión además de cabeceras APDU.
Nombre/versión y selección inicial dieron `9000`; el siguiente SELECT EF
identificó `D003`, usado para identificación/serie en perfiles documentados.
Ahora se permite su selección y lectura binaria únicamente tras confirmar
el MF, sin guardar su respuesta ni serial. Informes privados en
`local/jcop4-idopte-discovery-*/`. Las pruebas del
observador/constructor se ampliaron a 25 comprobaciones. El intento OUj06FEQ (15:45:03 UTC)
obtuvo transporte 0 en la consulta de capacidades 42000D48 y 9000 en todas
las APDU transmitidas; el registro omite FIDs/AID permitidos y respuestas.
El bloqueo de esa captura fue SELECT EF `4141`, cuyo propósito sigue sin
identificarse. El observador
v6 permite su SELECT acotado, tras el diagnóstico independiente Kzi6aI8S
(16:12:31 UTC), y registra etiquetas fijas de raíz/aplicación/selección de
metadatos para aclarar el contexto. No registra respuestas ni datos arbitrarios
y no concede acceso a READ de ese archivo.
La captura mEpNvPwv (16:30:07 UTC) confirmó selección de aplicación/4141 con
9000 y bloqueo del siguiente READ BINARY. No hay SW de tarjeta a ese READ
porque no se transmitió. No repetir esa captura como siguiente paso.
No repetir el intento de PIN hasta identificar el requisito que falta.
JCOP4 aún no está integrada en PKCS#11.
RSA 3072 y modularidad para futuras generaciones siguen pendientes.
La evidencia y el protocolo están en [docs/jcop4.md](docs/jcop4.md).

## Historial de desarrollo y comandos de diagnóstico

Consultar [AGENTS.md](AGENTS.md) y las últimas secciones de
[docs/jcop3.md](docs/jcop3.md). Ya se leen certificados RSA y la política
del PIN (7 intentos restantes en la última consulta pública). Se corrigieron
las ACL RSA y la ruta de firma: la llave es
un objeto nativo en ChipDoc y `0003` no es seleccionable como archivo.
SE 3 apunta al PIN global 1. La tarjeta acepta las configuraciones MSE de
RSA PKCS#1 y RSA/SHA-256; se agregó una adaptación limitada a las referencias
medidas de la llave de firma. El mecanismo combinado `SHA256-RSA-PKCS`
falló en `C_SignFinal`: el camino QSign no soporta OpenSSL 3. El helper
prepara el DigestInfo SHA-256 en el equipo y pide `RSA-PKCS` a la tarjeta.
El 11 de octubre de 2026 a las 20:18 de Costa Rica, esta ruta produjo una
firma de 256 bytes y OpenSSL confirmó `Verified OK`. Pasan 237 comprobaciones
locales. Para reproducir la prueba técnica:

```bash
nix develop
make test/iasecc JOBS="$(nproc)"
make sign/jcop3
```

El helper solicita el PIN sin eco, con depuración y caché de PIN desactivadas,
firma un texto técnico mediante RSA PKCS#1 v1.5 y verifica con OpenSSL.
El mecanismo combinado `SHA256-RSA-PKCS` también produjo una firma verificada
el mismo día a las 20:37, con OpenSC `34a040c32`. La corrección hace que
OpenSC prepare SHA-256 y DigestInfo en software y use RSA-PKCS en la tarjeta.
Para reproducir esa modalidad, usar `make sign/jcop3-sha256`. Ejecutar la firma directamente en la
terminal, sin redirigir stdout ni usar `tee`; no reintentar tras un fallo.
Las secciones siguientes conservan los pasos anteriores de preparación e
investigación; no es necesario repetirlos al retomar este directorio.

Para abrir el firmador instalado con el módulo local:

```bash
make firmador/jcop3
```

No recompila ni instala nada. El lanzador fija `LIBASEP11` al módulo compilado
y `OPENSC_CONF` a una configuración privada en `local/`, con emulación
PKCS#15 prioritaria, cachés de archivos/PIN desactivadas y depuración 0.
Utilizar un PDF de prueba y realizar una sola firma; si falla, detenerse
para revisar el error. La firma técnica verificada no demuestra todavía
compatibilidad con firmador. El primer intento falló con
`CKR_FUNCTION_NOT_SUPPORTED` y `signatureValue cannot be null!`.
La negociación del hashing SHA-256 ya se corrigió y validó en la tarjeta
mediante `make sign/jcop3-sha256`. El intento posterior del firmador pidió
el PIN y falló con `CKR_USER_NOT_LOGGED_IN`. El diagnóstico de las 21:06
confirmó login exitoso, llave de firma 3 y fallo en `C_SignFinal`, sin logout
intermedio. La ampliación de las 21:24 confirma selección y MSE exitosos,
pero rechazo `6982` al ejecutar RSA. Falta explicar la condición de seguridad.
El experimento de las 21:39 omitió las consultas de `C_GetSessionInfo`
posteriores al login y el rechazo persistió. No se confirmó esa hipótesis.
El diagnóstico de las 21:54 muestra una consulta de `C_GetTokenInfo`
inmediatamente después del login, que vuelve a seleccionar la raíz IAS.
No estaba cubierta por el experimento previo. Ahora la comparación omite
ambas clases de consulta física después de login, solo en el perfil observado:

```bash
make test/iasecc JOBS="$(nproc)"
make firmador/jcop3-session-state
```

Cerrar previamente las instancias del firmador. Realizar una sola firma de
un PDF de prueba y cerrar el programa. El diagnóstico guarda exclusivamente
nombres fijos y metadatos numéricos en `local/jcop3-firmador-api-*/api.log`.
Mantiene depuración 0 y caché de PIN desactivada; no registra PIN, su longitud,
rutas, documentos, certificados ni buffers APDU. Las banderas de intentos
del token utilizan metadatos públicos anteriores y pueden estar desactualizadas.
Login sigue verificando el PIN. El experimento está desactivado por defecto,
no modifica los modos normal/diagnóstico y produjo una firma en hardware.
La captura de las 23:34 confirmó login, RSA AT y `C_SignFinal` exitosos,
con una firma de 256 bytes y llave 3. No repetirla como primer paso.
La firma del PDF guardado y su sello de tiempo se verificaron después con
OpenSSL: el certificado firmante coincide con el de la tarjeta y el sello
abarca los rangos del archivo final. Se conserva el contenido de la página
original. Esta comprobación local no valida confianza ni revocación de
certificados. Falta diseñar la corrección definitiva; el experimento sigue
desactivado por defecto.

La consulta vacía aislada de `make sign/jcop3-pin-status` **permitió firmar**
el 12 de octubre a las 00:04 de Costa Rica. El registro confirma la consulta
inmediata después del VERIFY y la firma RSA; OpenSSL la verificó. Dos intentos
previos se detuvieron en `C_OpenSession CKR_TOKEN_NOT_PRESENT` antes del login.
Ese éxito no descarta efectos de selección, política, consultas repetidas o
cambios de transacción.

Los dos intentos de `make sign/jcop3-select-root` de las 00:13 reprodujeron
el rechazo RSA `6982` después de un VERIFY y un único SELECT raíz exitosos.
Esto reduce el fallo a la reselección en el contexto observado. El CLI además
intentó multipart tras fallar C_Sign; el helper ahora desactiva ese fallback
con el módulo actualizado, sin cambiar el modo normal del CLI.

La candidata `make firmador/jcop3-pin-status-in-place` produjo una firma PDF
el 12 de octubre a las 00:37 de Costa Rica, con OpenSC `23b3d25fa`.
El registro confirma cuatro consultas físicas después del login, ninguna
reselección raíz y una operación RSA exitosa de 256 bytes. La candidata mantiene
consultas físicas de estado de PIN, sin reseleccionar ni releer política una
vez conocida. Solo conserva el máximo público de intentos; estado e intentos
restantes proceden de la respuesta actual, incluso si quedan desconocidos.
No omite las consultas como el experimento anterior ni guarda el PIN.
OpenSSL verificó la firma CAdES del nuevo PDF, con el certificado de
`LlaveDeFirma`, y el CMS y la huella SHA-512 del sello de tiempo. La página
original se conserva. No se validaron confianza ni revocación.
Las firmas consecutivas encontraron otro bloqueo: la primera pasó y su PDF
se verificó, pero tras logout y nuevo login la segunda recibió `6982`.
El driver tiene un atajo por hash de PIN previo y estado CHV que puede
omitir VERIFY real después de reseleccionar raíz. La candidata actualizada
desactiva ese atajo en el perfil observado: cada nuevo login verifica la
entrada en la tarjeta, sin conservar su hash ni reintentar errores. Mantiene
consultas físicas sin selección y caché PKCS#15 desactivada.
Con OpenSC `381c469b2`, el 12 de octubre a las 01:07 de Costa Rica, **ambas
firmas consecutivas pasaron**. El registro confirma dos VERIFY reales y
dos operaciones RSA de 256 bytes; OpenSSL verificó la firma y el sello de
ambos PDF. Firmador hace logout entre documentos y un nuevo login, por eso
pidió PIN dos veces en esta prueba. No se validaron confianza ni revocación.
Una primera prueba de retirada/reinserción falló antes del segundo VERIFY,
con tarjeta retirada/sesión inválida y NullPointerException de Java.
El diagnóstico añadido registra presencia y comienzo de limpieza del token,
sin cambiar recuperación ni registrar PIN/APDU. La siguiente prueba GUI de
las 01:39 **pasó con cinco segundos fuera y cinco tras reinsertar**: OpenSC
observó ausencia y cambio de tarjeta, y ejecutó un nuevo VERIFY antes de
la segunda firma. Ambos PDF y sellos se verificaron con OpenSSL, sin validar
confianza ni revocación. El fallo anterior sigue pendiente; este pase no
prueba reinserción rápida. La candidata continúa desactivada por defecto.
La prueba rápida de las 01:46 reprodujo el fallo: OpenSC observó tarjeta
cambiada pero conservó el token/conexión anterior. Se corrigió localmente
card_detect para procesar CARD_CHANGED aunque la espera de eventos no vea
novedades, invalidar sesiones y reconectar. El límite de reintentos ahora
se renueva por llamada. Pasan 262 comprobaciones sin tarjeta, incluyendo
19 de detección, sesiones nuevas sin autenticación y fallos de reconexión.
Con OpenSC `8842f394b`, la prueba GUI de las 02:02 **pasó con reinserción
rápida**: se procesó el cambio aunque no se observó ausencia, se limpió el
token y se ejecutó un nuevo VERIFY antes de la segunda firma. Ambos PDF y
sellos se verificaron con OpenSSL; no hubo errores de login o sesión.
No se validaron confianza ni revocación. El segundo PDF quedó en la raíz y
los archivos /jcop3-*.pdf se excluyen de Git. La candidata sigue optativa;
ciclos repetidos con tarjeta y concurrencia siguen pendientes.

El modo de terminal usa la misma candidata y ya produjo un PDF verificado:

```bash
set -o pipefail
pass personal/firma-digital/pin | make firmador/jcop3-cli
```

Firma `doc.pdf` hacia `local/jcop3-cli.pdf`; se pueden cambiar con `PDF_INPUT`
y `PDF_OUTPUT`. Rechaza una salida ya existente. El usuario autorizó enviar
el PIN desde su gestor por stdin: el wrapper no lo lee, almacena ni pasa en
argumentos o variables; firmador `-dargs` recibe el flujo directamente.
No redirigir ni capturar ese flujo como registro. La prueba del usuario de
las 02:11 pasó con OpenSC `8842f394b`: VERIFY real, llave LlaveDeFirma,
SHA256-RSA-PKCS, firma de 256 bytes y PDF/sello verificados con OpenSSL.
La página original se conserva; no se validaron confianza ni revocación.
El agente no ejecutó pass ni la firma: verificó el resultado sin lector,
PIN o red. La salida ya existe, por lo que otra prueba requiere otro PDF_OUTPUT.
La prueba GUI de ciclos repetidos también pasó: seis VERIFY/firmas, cuatro
cambios de tarjeta procesados y seis PDF/sellos verificados (cinco previstos
y uno extra). El driver no cambió tras el pase; la candidata sigue optativa.
Ahora pasan 262 regresiones, con seis nuevas del helper de segundo cliente.

El próximo caso deja la GUI en reposo después de firmar el original hacia
`local/jcop3-peer-before.pdf`, manteniendo tarjeta y lector conectados. Desde
otra terminal con nix develop, ejecutar `make inspect/jcop3-peer-context`.
Este helper manda solo SELECT raíz IAS observada, sin PIN/firma, con debug 0,
y exige transporte exitoso y SW 9000. Si pasa, firmar nuevamente el original
hacia `local/jcop3-peer-after.pdf`. Detenerse al primer error y cerrar al terminar.
Así se prueba otro cliente entre firmas; no operaciones de firma simultáneas.
El helper usa la compilación existente; no requiere recompilar.

## Organización

- `vendor/opensc/`: submódulo de OpenSC.
- `flake.nix`: entorno de herramientas para `nix develop`.
- `Makefile`: comandos de investigación.
- `docs/jcop3.md`: inventario, observaciones y próximos hitos.
- `scripts/probe-jcop3.sh`: inventario inicial del lector y ATR, sin PIN.
- `local/`: resultados y capturas originales, excluidos de Git.

Los cambios del driver se desarrollarán dentro del submódulo, en una rama
propia. El repositorio principal registra el commit exacto de OpenSC utilizado.
Para compartir cambios del submódulo será necesario publicarlos en un fork y
actualizar su URL; un commit local no estará disponible para otros clones.

## Preparar Git y OpenSC

Ejecutar desde este directorio en una terminal con acceso de escritura a `.git`
y acceso a GitHub:

```bash
git init -b main
git submodule add https://github.com/OpenSC/OpenSC.git vendor/opensc
git -C vendor/opensc switch -c cr-jcop3
git add .gitignore .gitmodules flake.nix Makefile README.md docs scripts vendor/opensc
git commit -m "Prepare JCOP3 investigation workspace"
```

El submódulo queda fijado al commit descargado. Registrar ese commit en cada
prueba; no actualizarlo automáticamente durante una investigación.
Para clonar este proyecto más adelante, usar `git clone --recurse-submodules`.

## Primera observación de la tarjeta

En NixOS, el servicio PC/SC debe estar disponible. Si no está habilitado,
configurar `services.pcscd.enable = true;` en la configuración del sistema y
aplicar el cambio desde la terminal del usuario.

Conectar el lector e insertar la tarjeta por contacto. Entrar al entorno y
ejecutar la observación:

```bash
nix develop
make probe/jcop3
```

La primera ejecución de `nix develop` genera `flake.lock`, que debe agregarse
al repositorio para fijar la versión de las dependencias. Si `flake.nix` es un
archivo nuevo en un repositorio Git, agregarlo al índice antes de ejecutar Nix:

```bash
git add flake.nix
nix develop
git add flake.lock
```

También se puede ejecutar la observación sin abrir una shell interactiva:

```bash
nix develop --command make probe/jcop3
```

Si hay varios lectores, identificar el índice en la lista y repetir con él:

```bash
make probe/jcop3 READER=1
```

El objetivo `probe/jcop3` guarda un informe nuevo en `local/`. Solo consulta la versión de
OpenSC, la lista de lectores y el ATR. No solicita PIN ni envía comandos de
firma o administración. Esta primera observación no demuestra todavía que
OpenSC pueda leer certificados o firmar con la tarjeta.

Los objetivos `probe/jcop3`, `inspect/jcop3` y `read-dir/jcop3` utilizan las
herramientas empaquetadas de OpenSC. El entorno también incluye compilador y
dependencias para compilar el submódulo. Registrar ambas versiones por separado.

## Después del ATR

La primera tarjeta observada tiene un ATR idéntico a la entrada IAS/ECC Amos
de OpenSC. Probar primero identificación y enumeración con ese soporte:

```bash
make inspect/jcop3
```

Acepta también `READER=1`. Consulta el nombre identificado por OpenSC y enumera
certificados y llaves públicas mediante PKCS#15, con caché de archivos desactivada
y depuración APDU. No verifica PIN ni ejecuta firmas. Guarda los resultados en
`local/jcop3-inspection-*.txt`; pueden contener datos identificables y deben
revisarse antes de publicarlos. Solo imprime la ubicación del informe y el estado
de la ejecución, para no volcar la depuración completa en la terminal.

La primera inspección inicializa IAS/ECC pero falla al interpretar los permisos
de lectura de EF.DIR. Para comprobar su lectura con APDU directas conocidas:

```bash
make read-dir/jcop3
```

Este objetivo selecciona la raíz observada y EF.DIR con el driver `default`, y
lee dos bloques de 128 bytes. No verifica PIN ni modifica la tarjeta. Guarda
la captura en `local/jcop3-dir-*.txt` e imprime los estados de respuesta.
El diagnóstico y la evidencia están documentados en `docs/jcop3.md`.

## Comprobar la corrección de IAS/ECC

La lectura directa de EF.DIR funcionó. Contiene la aplicación `ChipDoc`, con un
AID que OpenSC ya reconoce como `ECC Generic PKI`. El primer fallo se reprodujo
en el parser de permisos de IAS/ECC y se corrigió para procesar todos los grupos
del formato compacto. Aún falta validar la enumeración en la tarjeta.

Volver a entrar al entorno si la shell actual se abrió antes de agregar las
dependencias de compilación:

```bash
nix develop
make test/iasecc
make inspect/jcop3-built
```

`make test/iasecc` compila OpenSC con PC/SC en `build/opensc/` y ejecuta las
pruebas del parser real, sin tarjeta. `make inspect/jcop3-built` utiliza las
herramientas de esa compilación para repetir la inspección pública y guarda el
informe en `local/`. No instala OpenSC en el sistema ni solicita PIN.

El módulo Idopte utilizado por el firmador instalado y el motivo del fallo al
cargar el archivo del instalador están documentados en `docs/idopte.md`.

La inspección con la tarjeta ya confirmó la lectura de ambos certificados RSA
de 2048 bits. El siguiente paso es consultar la interfaz PKCS#11, sin PIN:

```bash
make inspect/jcop3-pkcs11
```

Utiliza la compilación existente y guarda el informe en `local/`. Consulta
metadatos de llaves y PIN, slots, mecanismos y objetos. `READER=0` selecciona
el lector PC/SC y `SLOT_INDEX=0` el slot PKCS#11; estos índices pueden diferir.
La consulta PKCS#11 ya publica certificados y llaves públicas. Aunque anuncia
`SHA256-RSA-PKCS`, ese mecanismo fallaba antes de corregir la negociación de
hashing en `34a040c32`; ahora ambas modalidades están verificadas. La prueba
usa `RSA-PKCS` con un DigestInfo SHA-256 preparado antes de solicitar el PIN:

```bash
make sign/jcop3
```

Solicita el PIN en la terminal, firma un texto técnico y verifica el resultado
con OpenSSL usando la llave pública del certificado. Guarda los archivos en
`local/jcop3-signature-*`, con depuración APDU desactivada y sin reintentar el
PIN. Esta prueba ya produjo una firma verificada; falta validar el firmador
con OpenSC mediante `make firmador/jcop3`.

El primer intento de firma falló en `C_Login`. La enumeración previa también
mostró que la selección del contexto `0001` del PIN devuelve `6A82`. Antes de
otra autenticación, ejecutar el diagnóstico sin PIN:

```bash
make inspect/jcop3-pin-context
```

Compara selecciones y consulta metadatos en los dos AID observados, utilizando
la compilación existente. Guarda el informe en `local/`.

Esa consulta confirmó que el CHV 1 es global y accesible desde la raíz, mientras
`0001` no es seleccionable. Se agregó una corrección de los metadatos PKCS#15
para usar MF en ese perfil y la consulta en tarjeta confirmó ese contexto.
También se corrigió el parser SDO, que descartaba metadatos válidos y borraba
la referencia del PIN antes de la siguiente consulta. Para compilar y ejecutar
las comprobaciones locales y consultar el estado sin introducir PIN:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
```

La configuración local activa el callback IAS/ECC con `try_emulation_first`.
La política del PIN contiene además un campo `C9` que el parser todavía no
reconoce. Revisar el nuevo informe antes de repetir una firma. La investigación
y el alcance de la corrección están registrados en `docs/jcop3.md`.

La consulta con el parser corregido ya conserva la referencia del PIN y recibe
su DOCP; se detiene al interpretar `C9`. Para comprobar si la tarjeta permite
consultar los campos estándar por separado, sin recompilar ni introducir PIN:

```bash
make inspect/jcop3-pin-policy
```

Compara selectores de metadatos mediante SELECT y GET DATA. Su compatibilidad
se determina por las respuestas de la tarjeta y queda registrada en un nuevo
informe `local/jcop3-pin-policy-*`.

Esa comparación confirmó que la tarjeta entrega el DOCP completo aun con los
selectores de campos. La adaptación experimental conserva la extensión `C9`
y reconoce el formato exacto observado para consulta y verificación del PIN;
cambio, desbloqueo y contactless quedan pendientes. Pasan 38 comprobaciones
locales. La siguiente validación sigue sin introducir PIN:

```bash
make test/iasecc JOBS="$(nproc)"
make inspect/jcop3-pkcs11
make inspect/jcop3-key-policy
```

La consulta de llaves pide exclusivamente metadatos DOCP de las referencias
RSA declaradas; no solicita componentes de las llaves privadas. Primero se
revisarán estos informes antes de otro intento de firma.

Después:

1. Validar la corrección en la tarjeta y registrar el resultado en `docs/jcop3.md`.
2. Si la enumeración falla, comparar el APDU o estructura incompatible con una
   captura de Idopte, anotando su versión y el estado de la caché.
3. Documentar selección de aplicación, certificados y referencias de llaves.
4. Adaptar lectura y enumeración en OpenSC si es necesario.
5. Capturar una firma controlada con el PIN correcto, adaptar la secuencia si
   hace falta y verificar el resultado con la llave pública del certificado.

El PIN debe introducirse de forma interactiva. Las capturas APDU de una sesión
autenticada pueden contenerlo y no deben publicarse sin una revisión explícita.
No explorar comandos de inicialización, borrado, desbloqueo o administración
en la tarjeta emitida.

## Referencias

- [Código de OpenSC](https://github.com/OpenSC/OpenSC)
- [Añadir soporte para una tarjeta](https://github.com/OpenSC/OpenSC/wiki/Adding-a-new-card-driver)
- [Compilar OpenSC](https://github.com/OpenSC/OpenSC/wiki/Compiling-and-Installing-on-Unix-flavors)
- [Driver IAS/ECC](https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/card-iasecc.c)
- [Driver eOI](https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/card-eoi.c)
