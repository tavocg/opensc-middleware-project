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
firmar. El siguiente paso es consultar datos públicos de seguridad, sin PIN:

```bash
nix develop
make inspect/jcop4-security-info READER=0
```

Cerrar firmador y otros clientes y mantener esta JCOP4 insertada. Compila
solo una herramienta pequeña contra OpenSC existente y consulta metadatos
de autenticación, versión y el primer bloque de CardAccess. No autentica
ni firma. Informes privados en `local/jcop4-security-info-*/`. Pasaron
30 comprobaciones del helper y 19 del lanzador, con transporte simulado.
La consulta inicial obtuvo `9000` para versión, pero el helper se detuvo
por una suposición de longitud ya corregida; CardAccess sigue pendiente.
No repetir el intento de PIN
hasta identificar el requisito que falta. JCOP4 aún no está integrada en PKCS#11.
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
