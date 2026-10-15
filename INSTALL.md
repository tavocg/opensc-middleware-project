# Usar una release de libjcop3p11 con firmador

La release contiene el módulo PKCS#11 `libjcop3p11.so`, su biblioteca
`libopensc`, un lanzador y las configuraciones para las dos generaciones
probadas. El nombre distinto permite elegir este módulo mediante `LIBASEP11`
en el firmador instalado. No instala ni sustituye bibliotecas del sistema.

## Compatibilidad

- GNU/Linux **x86_64**, Nix instalado, lector y servicio PC/SC operativos,
  y `firmador` instalado y accesible en `PATH`.
- JCOP3: perfil Generic PKI/IAS-ECC observado en Costa Rica.
- JCOP4: perfil optativo ChipDoc CR, **RSA 2048 / SHA-256 / PKCS#1 v1.5**.
  RSA 3072, otras personalizaciones y otras plataformas no están validadas.
- Los binarios se compilan con el Nixpkgs fijado en `flake.lock`. Conservan
  dependencias de `/nix/store`: no son bibliotecas universales para cualquier
  distribución. La aplicación/Java y su glibc deben ser compatibles con esa
  compilación. Importar el runtime no actualiza la glibc de una aplicación
  ajena a Nix que ya use otra versión. El entorno NixOS del proyecto es el
  recorrido probado; no se ha validado firmador en todas las distribuciones.

Si necesitas instalar Nix, utiliza sus [instrucciones oficiales](https://nix.dev/install-nix).
El lector/PCSC y firmador se instalan según el sistema operativo; el paquete
no configura servicios del sistema ni necesita Idopte para estas operaciones.

## Descargar y verificar

En la página **Releases** del repositorio, descarga todos los assets de la
versión elegida en un directorio nuevo. También puedes usar GitHub CLI:

```bash
REPO="TU_USUARIO/TU_REPOSITORIO"
VERSION="v0.1.0" # Sustituir por una release existente.
mkdir "libjcop3p11-$VERSION"
cd "libjcop3p11-$VERSION"
gh release download "$VERSION" --repo "$REPO"
sha256sum --check SHA256SUMS
```

Los valores anteriores son ejemplos: todavía no se ha publicado una release
desde esta sesión. No continúes si falla la verificación de un archivo.

Importa las dependencias Nix y extrae el paquete:

```bash
xz -dc nix-runtime-linux-x86_64.nar.xz | nix-store --import
tar -xJf libjcop3p11-linux-x86_64.tar.xz
cd libjcop3p11-linux-x86_64
```

Mantén juntos `bin/`, `lib/` y `config/`. El módulo busca `libopensc` junto a
él mediante `$ORIGIN`. Descargar únicamente el asset `libjcop3p11.so` no basta.
Las referencias de `/nix/store` y sus versiones quedan en
`nix-runtime-paths.txt` y `build-manifest.json`, fuera del paquete extraído.

## Abrir firmador

Cierra primero otras instancias de firmador. Inserta la tarjeta y elige su
generación; no se trata de seleccionar el tamaño RSA automáticamente.

Para la JCOP3 probada:

```bash
./bin/firmador-jcop jcop3
```

El lanzador activa la corrección de consulta de PIN en su contexto y
`lock_login`, con cachés de PIN/archivos desactivadas. Otro cliente PC/SC puede
esperar mientras firmador mantiene la tarjeta reservada.

Para la JCOP4 RSA 2048 probada:

```bash
./bin/firmador-jcop jcop4
```

El lanzador activa `chipdoc_profile = cr-rsa2048`, sin `atomic` ni `lock_login`
genéricos. El driver controla su propia transacción y la cierra tras firmar.
El estado protegido previo `63C7` es una condición conservadora de este perfil;
no acredita un contador máximo universal ni soporte de recuperación del PIN.

En la GUI abre el documento, selecciona el certificado **de firma**, firma y
guarda con un nombre distinto. Introduce el PIN únicamente en el diálogo de
firmador cuando lo solicite. El lanzador no lo recibe por argumentos,
variables, archivos o tuberías y no activa trazas ni depuración de APDU.

Para comprobar firmas consecutivas, vuelve a abrir el **PDF original** y
firma otra copia en la misma GUI. Para reinserción, termina primero la firma,
retira y reinserta la tarjeta y firma otra copia. No retires la tarjeta durante
la operación. Ante un error conserva el mensaje y no repitas el PIN por inercia.

Los sellos de tiempo y consultas de revocación del firmador pueden necesitar
Internet, aunque el cálculo de la firma en la tarjeta es local. Que la GUI
cierre sin error no sustituye verificar el PDF resultante.

## Problemas de carga

- `libopensc.so... not found`: extrae el paquete completo y usa su lanzador.
- Dependencias `/nix/store/...` ausentes: importa el runtime de **esa misma
  release**. No mezcles archivos de distintas versiones.
- `GLIBC_... not found`: usa firmador/Java en un entorno Nix compatible, o
  prepara una compilación adecuada para tu plataforma. Copiar o importar las
  dependencias no cambia la ABI del proceso que carga PKCS#11.
- Fallo de lector/PCSC: comprueba el servicio y el lector desde tu terminal.
  El paquete no inicia un servicio PCSC privado.

La release es experimental. CI comprueba compilación y lectores/PIN
simulados; no contiene pruebas con una tarjeta física ni credenciales.

## Aplicar el parche a OpenSC

El asset `opensc-jcop3-jcop4.diff` es un diff Git normal sin prefijo del
repositorio contenedor. Se aplica **desde la raíz de OpenSC**, en una copia
limpia del commit base fijado:

```bash
git clone https://github.com/OpenSC/OpenSC.git
cd OpenSC
git switch --detach fc27ee8768cb882fceb99535dce8b6a966534dcb
git apply --check /ruta/opensc-jcop3-jcop4.diff
git apply /ruta/opensc-jcop3-jcop4.diff
```

El commit base y árbol resultante están en `opensc-base.json`. No se garantiza
que aplique sin conflictos a otro master. `opensc-patched-source.tar.xz`
contiene también el código OpenSC correspondiente completo. Su `COPYING`
acompaña a las bibliotecas.

## Compilar y publicar desde este proyecto

El workflow `.github/workflows/release.yml` instala Nix, descarga el OpenSC
público base y aplica el parche comprobando su árbol Git. No necesita que los
commits de nuestro submódulo estén publicados en el remoto de OpenSC.
Compila y ejecuta regresiones sin tarjeta antes de empaquetar.

- **Actions → Build and release libjcop3p11 → Run workflow**: compila y deja
  artifacts, sin publicar una release.
- Al subir una etiqueta `v*`, publica una **prerelease** con el mismo nombre,
  después de que pasen compilación y pruebas. No reemplaza una release existente.

Por ejemplo, una vez revisados y publicados los commits del proyecto:

```bash
git tag v0.1.0
git push origin v0.1.0
```

Para regenerar el parche y el diagrama del README después de modificar el
submódulo, primero crea su commit y ejecuta:

```bash
python3 scripts/export-opensc-patch.py
python3 scripts/export-opensc-patch.py --check
```

Para cambiar deliberadamente la base de OpenSC, añade `--base COMMIT` a la
generación y revisa el nuevo diff. Las referencias de las Actions están
fijadas por commit. El instalador se configura para upstream Nix; sus opciones
se documentan en [nix-installer-action](https://github.com/DeterminateSystems/nix-installer-action).
La publicación utiliza [GitHub CLI](https://cli.github.com/manual/gh_release_create).
