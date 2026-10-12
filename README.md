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
firma de 256 bytes y OpenSSL confirmó `Verified OK`. Pasan 112 comprobaciones
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
intermedio. Se amplió el registro para distinguir selección, MSE y operación
RSA, sin capturar PIN ni contenido de APDU. Para usarlo:

```bash
make test/iasecc JOBS="$(nproc)"
make firmador/jcop3-diagnose
```

Cerrar previamente las instancias del firmador. Realizar una sola firma de
un PDF de prueba y cerrar el programa. El diagnóstico guarda exclusivamente
funciones, códigos de retorno, sesiones y metadatos numéricos de llave en
`local/jcop3-firmador-api-*/api.log`. Mantiene depuración 0 y caché de PIN
desactivada. El objetivo normal deja este registro desactivado. Revisar
el informe antes de otro intento; todavía no se ha corregido este fallo.

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
