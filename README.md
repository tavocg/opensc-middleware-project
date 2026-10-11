# Middleware libre para firma digital de Costa Rica

Primer objetivo: reconocer una tarjeta JCOP3 emitida en Costa Rica, leer su
certificado y producir una firma verificable mediante PKCS#11 de OpenSC.
La compatibilidad con su aplicación interna todavía está por comprobar.

## Organización

- `vendor/opensc/`: submódulo de OpenSC, pendiente de agregar.
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
git add .gitignore .gitmodules README.md docs scripts vendor/opensc
git commit -m "Prepare JCOP3 investigation workspace"
```

El submódulo queda fijado al commit descargado. Registrar ese commit en cada
prueba; no actualizarlo automáticamente durante una investigación.
Para clonar este proyecto más adelante, usar `git clone --recurse-submodules`.

## Primera observación de la tarjeta

En NixOS, el servicio PC/SC debe estar disponible. Si no está habilitado,
configurar `services.pcscd.enable = true;` en la configuración del sistema y
aplicar el cambio desde la terminal del usuario.

Conectar el lector e insertar la tarjeta por contacto. Abrir las herramientas:

```bash
nix shell nixpkgs#opensc nixpkgs#pcsc-tools
bash scripts/probe-jcop3.sh
```

Si hay varios lectores, identificar el índice en la lista y repetir con él:

```bash
bash scripts/probe-jcop3.sh 1
```

El script guarda un informe nuevo en `local/`. Solo consulta la versión de
OpenSC, la lista de lectores y el ATR. No solicita PIN ni envía comandos de
firma o administración. Esta primera observación no demuestra todavía que
OpenSC pueda leer certificados o firmar con la tarjeta.

`nix shell` proporciona herramientas empaquetadas, no una compilación del
submódulo. Registrar ambas versiones por separado. Una vez identificado el
perfil de la tarjeta, preparar un entorno de compilación y pruebas para el
OpenSC modificado.

## Después del ATR

1. Completar el inventario de `docs/jcop3.md`.
2. Comparar los identificadores y el protocolo con IAS/ECC y eOI.
3. Capturar la enumeración pública realizada por Idopte, anotando su versión
   y si la caché está vacía. Revisar los datos antes de compartir la captura.
4. Documentar selección de aplicación, certificados y referencias de llaves.
5. Implementar lectura y enumeración en OpenSC.
6. Capturar una firma controlada con el PIN correcto, implementar la secuencia
   y verificar el resultado con la llave pública del certificado.

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
