# Middleware libre para firma digital de Costa Rica

Primer objetivo: reconocer una tarjeta JCOP3 emitida en Costa Rica, leer su
certificado y producir una firma verificable mediante PKCS#11 de OpenSC.
La compatibilidad con su aplicación interna todavía está por comprobar.

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

`nix develop` proporciona herramientas empaquetadas, no una compilación del
submódulo. Registrar ambas versiones por separado. Una vez identificado el
perfil de la tarjeta, preparar un entorno de compilación y pruebas para el
OpenSC modificado.

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

Después:

1. Completar el inventario y evaluar el resultado de OpenSC en `docs/jcop3.md`.
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
