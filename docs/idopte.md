# Módulo Idopte instalado

La revisión del lanzador instalado de `firmador`, sin abrir la aplicación,
mostró que usa por defecto:

```text
/nix/store/ihlfngx17qrwp0fmfswx57fnw6mf76n0-idopte-p11-6.23.50.5-1/lib/SCMiddleware/libidop11.so
```

En la generación actual del sistema también está disponible como:

```text
/run/current-system/sw/lib/SCMiddleware/libidop11.so
```

El lanzador permite sustituirlo mediante `LIBASEP11`. Esa variable conserva el
nombre antiguo, pero su valor predeterminado apunta a Idopte. El módulo
instalado se cargó con `pkcs11-tool` y devolvió fabricante `Idopte`, versión de
biblioteca `6.23` e interfaz Cryptoki `2.40`. La sesión de investigación no
ve lectores y terminó con `No slots`; esto no demuestra ningún fallo de la
instalación utilizada por el propietario.

## Por qué falló el módulo del instalador

El directorio mencionado inicialmente,
`vendor/idopte/sfd_ClientesLinux_RPM64_26_08/Firma Digital/Librería`, contiene
`libASEP11.so` y `libaseLaserP11.so`, pero no `libidop11.so`.
Dentro del instalador, el archivo de Idopte está en:

```text
vendor/idopte/sfd_ClientesLinux_RPM64_26_08/Firma Digital/Idopte/usr/lib/SCMiddleware/libidop11.so
```

Ese ELF tiene RPATH `/usr/lib/SCMiddleware`. El instalado en NixOS tiene rutas
de dependencias adaptadas al Nix store. Una prueba de carga del archivo crudo
falló al buscar `libpcsclite.so.1`. Corregir únicamente la ruta al archivo no
basta para reproducir el entorno de carga del módulo instalado.

Para futuras comparaciones se puede consultar el módulo instalado, sin PIN:

```bash
nix develop
pkcs11-tool --module /run/current-system/sw/lib/SCMiddleware/libidop11.so --show-info --list-slots
```

Para la JCOP4 actual, `make inspect/jcop4-idopte READER=0` prepara esta
consulta con la herramienta compilada localmente, inventario/ATR previo,
stdin vacío, trazas desactivadas e informe privado bajo `local/`. No solicita
sesión/login/PIN, acceso a objetos ni firma; permite revisar el reconocimiento
y flags anunciados del token. Detalles en [jcop4.md](jcop4.md).

`make inspect/jcop4-idopte-discovery READER=0` repite solo ese descubrimiento
público bajo un observador de cabeceras PC/SC. Bloquea comandos fuera de una
lista pública acotada; puede causar un error deliberado del módulo. No captura
datos ni permite VERIFY, login/firma o controles del lector. Usar exclusivamente
ese objetivo; no precargarlo en firmador. El diagnóstico normal ya confirmó
NXP/ChipDoc v3.2 y PIN inicializado en la JCOP4 (13 de octubre, 14:17:31 UTC).

No se ha capturado una firma ni una sesión autenticada de Idopte. OpenSC ya
enumera los objetos y produjo una firma RSA SHA-256 verificada con OpenSSL.
La integración pendiente se prueba con `make firmador/jcop3`: fija
`LIBASEP11` al módulo compilado y proporciona `OPENSC_CONF` con el perfil
usado en la firma verificada. El lanzador instalado conserva ambas variables
al ejecutar Java. El código y los binarios del instalador permanecen fuera
del historial del proyecto.
