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
ese objetivo; no activarlo en firmador. El diagnóstico usa la delegación
`LIBPCSCLITE_DELEGATE`, con dependencia explícita del transporte
`libpcsclite_real.so.1`, sin instalar ni sustituir bibliotecas. Descarta
variables de precarga/delegación externas y conserva únicamente cabeceras,
estados y resultados de conexión. La primera versión con LD_PRELOAD no
observó eventos en y7dskS3z; eso no acredita un fallo de la tarjeta. El
diagnóstico normal ya confirmó
NXP/ChipDoc v3.2 y PIN inicializado en la JCOP4 (13 de octubre, 14:17:31 UTC).
El informe aPCZL12P, 14:53:29 UTC, confirmó conexión y bloqueo de CA 01 00
antes del transporte. La lista pública ahora permite exclusivamente el
caso-2 corto de ese GET DATA en canal básico; no registra la respuesta.
El precedente de identificación ChipDoc y sus límites están en [jcop4.md](jcop4.md).
El informe fZcrQuOX, 14:57:38 UTC, confirmó nombre/versión y SELECT inicial
9000, antes de bloquear SELECT EF 00 A4 02 00. El formato v3 añade solo
el identificador de dos bytes de ese SELECT bloqueado si su forma es válida,
manteniendo el bloqueo y sin capturar contenido de archivos.
El informe FIyDDxFN, 15:07:25 UTC, identificó ese EF como D003. La consulta
actual permite exclusivamente su SELECT EF básico desde MF confirmado y
READ BINARY básico, con respuestas omitidas. El precedente de serie pública
en otros perfiles no demuestra todavía el contenido en Costa Rica. Mantener
etiqueta/serial/URI del inventario Idopte fuera de Git y de mensajes públicos.

`pcsc-spy` estándar registra los buffers transmitidos, que pueden incluir el
PIN durante autenticación. No usarlo con el PIN real ni considerar un filtro
de pantalla o una anonimización posterior equivalente a descartarlo antes
del registro. El observador público actual **bloquea**, no permite estudiar
una firma autenticada. Esa instrumentación requerirá otro diseño y pruebas.
Referencias del proyecto PC/SC: [advertencia sobre el PIN](https://pcsclite.apdu.fr/)
y [delegación de bibliotecas](https://pcsclite.apdu.fr/api/libredirect_8c_source.html).

No se ha capturado una firma ni una sesión autenticada de Idopte. OpenSC ya
enumera los objetos y produjo una firma RSA SHA-256 verificada con OpenSSL.
La integración pendiente se prueba con `make firmador/jcop3`: fija
`LIBASEP11` al módulo compilado y proporciona `OPENSC_CONF` con el perfil
usado en la firma verificada. El lanzador instalado conserva ambas variables
al ejecutar Java. El código y los binarios del instalador permanecen fuera
del historial del proyecto.
