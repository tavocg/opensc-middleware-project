# Instalar y usar con firmador

**Una misma biblioteca sirve para JCOP3 y JCOP4.** La compilación genera
`opensc-pkcs11.so`; al copiarla o descargarla se llama `libjcop3p11.so`.
`LIBASEP11` permite elegirla en el firmador instalado. La configuración
selecciona el perfil de la tarjeta; no hay que recompilar al cambiar de generación.

Los ejemplos son para GNU/Linux con Bash, `firmador` instalado y un lector
con su servicio PC/SC operativo. El soporte probado incluye JCOP3 Generic
PKI/IAS-ECC y JCOP4 ChipDoc CR RSA 2048 / SHA-256 / PKCS#1 v1.5.
RSA 3072, otras personalizaciones y otras plataformas siguen pendientes.

## Compilar OpenSC sin Nix

Necesitas un compilador C, Make, Autoconf, Automake, Libtool, `pkg-config`,
Git, Python 3 y las bibliotecas de desarrollo de OpenSSL 3, PC/SC y zlib.
Por ejemplo, en Debian/Ubuntu:

```bash
sudo apt-get install build-essential autoconf automake libtool pkg-config \
  git python3 curl xz-utils libssl-dev libpcsclite-dev zlib1g-dev pcscd libccid
```

En otras distribuciones instala sus paquetes equivalentes. Consulta también
los [requisitos de compilación de OpenSC](https://github.com/OpenSC/OpenSC/wiki/Compiling-and-Installing-on-Unix-flavors).
El servicio PC/SC y el lector deben estar configurados según tu distribución.

En un directorio nuevo, descarga el proyecto y prepara OpenSC desde la base
pública con nuestro parche. Así no dependes de que los commits propios del
submódulo estén publicados en el remoto de OpenSC:

```bash
git clone https://github.com/tavocg/middleware-project.git
cd middleware-project
git clone https://github.com/OpenSC/OpenSC.git vendor/opensc
git -C vendor/opensc switch --detach "$(python3 -c \
  'import json; print(json.load(open("patches/opensc-base.json"))["base_commit"])')"
git -C vendor/opensc apply --check ../../patches/opensc-jcop3-jcop4.diff
git -C vendor/opensc apply ../../patches/opensc-jcop3-jcop4.diff
make build/opensc JOBS="$(nproc)"
```

Si ya trabajas en este proyecto y tienes el OpenSC modificado en
`vendor/opensc/`, ejecuta solamente el último comando. No vuelvas a aplicar
el parche. La compilación queda en `build/opensc/` y no instala nada en el
sistema. Para ejecutar las regresiones de software, sin lector ni PIN:

```bash
make test/iasecc JOBS="$(nproc)"
```

Nix es opcional para compilar localmente: si lo usas, puedes entrar en
`nix develop` en lugar de instalar las dependencias de desarrollo del sistema.

## Guardar la biblioteca en ~/.local/lib

Desde la raíz del proyecto, después de compilar:

```bash
JCOP_PREFIX="$HOME/.local/lib/libjcop3p11"
mkdir -p "$JCOP_PREFIX/lib" "$JCOP_PREFIX/config"
cp build/opensc/src/pkcs11/.libs/opensc-pkcs11.so "$JCOP_PREFIX/lib/libjcop3p11.so"
cp -a build/opensc/src/libopensc/.libs/libopensc.so* "$JCOP_PREFIX/lib/"
cp config/jcop3.conf config/jcop4.conf "$JCOP_PREFIX/config/"
```

Conserva también `libopensc`: el módulo PKCS#11 la necesita. No mezcles
bibliotecas o configuraciones de compilaciones distintas.

## Abrir firmador con la misma biblioteca

Cierra otras instancias del firmador. Para la copia instalada en tu usuario:

```bash
JCOP_MODULE="$HOME/.local/lib/libjcop3p11/lib/libjcop3p11.so"
JCOP_CORE="$HOME/.local/lib/libjcop3p11/lib"
JCOP_CONFIG="$HOME/.local/lib/libjcop3p11/config"
```

O, para usar directamente tu compilación sin copiar archivos, ejecuta desde
la raíz del proyecto:

```bash
JCOP_MODULE="$PWD/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
JCOP_CORE="$PWD/build/opensc/src/libopensc/.libs"
JCOP_CONFIG="$PWD/config"
```

Elige **uno** de los siguientes comandos según la tarjeta. Ambos cargan
exactamente el mismo `JCOP_MODULE`.

JCOP3:

```bash
LIBASEP11="$JCOP_MODULE" LD_LIBRARY_PATH="$JCOP_CORE" \
  OPENSC_CONF="$JCOP_CONFIG/jcop3.conf" OPENSC_DEBUG=0 \
  OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes firmador
```

JCOP4 RSA 2048:

```bash
LIBASEP11="$JCOP_MODULE" LD_LIBRARY_PATH="$JCOP_CORE" \
  OPENSC_CONF="$JCOP_CONFIG/jcop4.conf" OPENSC_DEBUG=0 firmador
```

La asignación va **antes** del comando: `LIBASEP11=/ruta/biblioteca.so firmador`.
Usa una terminal sin trazas ni opciones experimentales de sesiones anteriores.
`OPENSC_CONF` es necesario: activa el perfil y las opciones comprobadas de
cada tarjeta. JCOP3 usa `lock_login` y puede hacer esperar a otros clientes
PC/SC mientras el firmador mantiene la reserva; JCOP4 controla su propia
transacción y la cierra tras firmar.

Abre el PDF, selecciona el certificado de firma, introduce el PIN en el
diálogo y guarda otra copia. Para probar otra firma puedes partir de nuevo
del PDF original. Retira la tarjeta solo después de terminar la operación.
Ante un error, conserva el mensaje y detente antes de volver a introducir el PIN.
El sellado de tiempo y la consulta de revocación pueden necesitar Internet;
el cálculo de la firma en la tarjeta es local.

## Descargar un binario de GitHub

Las [releases de tavocg/middleware-project](https://github.com/tavocg/middleware-project/releases)
contienen un paquete para GNU/Linux x86_64, las fuentes y el parche.
**Los binarios del workflow actual dependen de Nix**: requieren importar el
runtime de esa versión y una glibc compatible en la aplicación/Java.
Para usar exclusivamente las bibliotecas de tu distribución, compila como
se indica arriba. Extraer el `.so` de la release no elimina esas dependencias.

Este bloque descarga la última publicación listada, incluidas prereleases,
verifica los tres archivos descargados y extrae el paquete bajo `~/.local/lib`.
Necesita `curl`, Python 3, `tar`, `xz`, `sha256sum` y Nix instalado:

```bash
(
  set -euo pipefail
  mkdir -p "$HOME/.cache/libjcop3p11"
  cd "$HOME/.cache/libjcop3p11"
  RELEASE_URL=$(curl -fsSL \
    'https://api.github.com/repos/tavocg/middleware-project/releases?per_page=1' |
    python3 -c 'import json, sys
releases = json.load(sys.stdin)
if not releases:
    sys.exit("No hay releases publicadas")
print(releases[0]["html_url"].replace("/tag/", "/download/"))')
  for asset in libjcop3p11-linux-x86_64.tar.xz nix-runtime-linux-x86_64.nar.xz SHA256SUMS; do
    curl -fL "$RELEASE_URL/$asset" -o "$asset"
  done
  sha256sum --check --ignore-missing SHA256SUMS
  xz -dc nix-runtime-linux-x86_64.nar.xz | nix-store --import
  mkdir -p "$HOME/.local/lib/libjcop3p11"
  tar -xJf libjcop3p11-linux-x86_64.tar.xz \
    -C "$HOME/.local/lib/libjcop3p11" --strip-components=1
)
```

Si quieres una versión concreta, sustituye la asignación de `RELEASE_URL` por
`RELEASE_URL="https://github.com/tavocg/middleware-project/releases/download/v0.1.0"`,
usando una etiqueta publicada. Para la última versión **estable** puedes usar
`RELEASE_URL="https://github.com/tavocg/middleware-project/releases/latest/download"`.
[GitHub excluye las prereleases de `latest`](https://docs.github.com/en/rest/releases/releases#get-the-latest-release);
el workflow actual publica versiones experimentales, por eso el ejemplo
principal consulta la lista de releases.

Después de extraer, usa los comandos de `LIBASEP11` anteriores o el lanzador
incluido, que prepara la configuración y limpia opciones de diagnóstico heredadas:

```bash
"$HOME/.local/lib/libjcop3p11/bin/firmador-jcop" jcop3
# Para la JCOP4, en lugar del comando anterior:
"$HOME/.local/lib/libjcop3p11/bin/firmador-jcop" jcop4
```

## Problemas de carga

- `libopensc.so... not found`: conserva esa biblioteca junto al módulo y
  configura `LD_LIBRARY_PATH` como en los ejemplos.
- Referencias `/nix/store/...` ausentes: importa el runtime de la misma release
  o compila con las dependencias de tu distribución.
- `GLIBC_... not found`: compila para tu sistema o usa firmador/Java en un
  entorno compatible. Importar el runtime Nix no actualiza la glibc del proceso.
- Lector no disponible: revisa el servicio PC/SC y el lector en tu sistema.

## Publicar una release

[GitHub Actions](https://github.com/tavocg/middleware-project/actions) compila
con Nix y ejecuta pruebas con lectores y credenciales simulados. La ejecución
manual deja artefactos; una etiqueta `v*` publica una prerelease si pasan los
controles. No reemplaza versiones existentes ni prueba una tarjeta física.

Después de subir los commits, por ejemplo para una etiqueta nueva:

```bash
git tag -a v0.1.0 -m "Primera versión experimental"
git push origin v0.1.0
```

Si modificas el submódulo, crea primero su commit y regenera los archivos
exportados antes de publicar:

```bash
python3 scripts/export-opensc-patch.py
python3 scripts/export-opensc-patch.py --check
```

El [manifiesto](patches/opensc-base.json) fija la base y el árbol del parche.
El asset `opensc-patched-source.tar.xz` contiene el código OpenSC completo
correspondiente; su `COPYING` acompaña a las bibliotecas.
