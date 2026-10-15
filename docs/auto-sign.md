# Prueba de firma con autodetección

Un mismo comando sirve para las JCOP3 y JCOP4 admitidas por este proyecto.
Carga el módulo local y usa la selección normal de drivers, sin forzar
`iasecc`, tipo de tarjeta, perfil ni políticas PKCS#15/PKCS#11. El lanzador
establece `OPENSC_CONF=/dev/null` para evitar configuraciones del sistema o
de pruebas anteriores; no crea archivos `.conf`.

Compilar no accede a la tarjeta:

```bash
nix develop -c make build/opensc
```

La siguiente operación la ejecuta el usuario, con una sola tarjeta y los
otros clientes cerrados:

```bash
nix develop -c make sign/auto SLOT_INDEX=0
```

`SLOT_INDEX` es el índice entre los slots PKCS#11 que tienen token presente,
no el número de lector PC/SC. El valor predeterminado es `0`. Se busca un
certificado único con etiqueta `LlaveDeFirma`; para otra etiqueta explícita:

```bash
nix develop -c make sign/auto SLOT_INDEX=0 KEY_LABEL=LlaveDeFirma
```

Antes del PIN comprueba que el token anuncia `CKM_RSA_PKCS`, que la sesión
está en estado público y que el certificado seleccionado contiene RSA 2048.
Muestra un texto técnico fijo por la terminal y solicita el PIN una sola vez,
sin eco (ASCII imprimible de 6 a 14 caracteres). No recibe el PIN por argumentos,
variables, archivos ni entrada estándar. Después de un único `C_Login`, busca
una llave privada de firma única con el mismo ID del certificado.

Realiza un solo `C_SignInit` y un solo `C_Sign` con `CKM_RSA_PKCS` y DigestInfo
SHA-256 canónico de ese texto. Verifica la firma con la llave pública del
certificado y comprueba que un mensaje con un bit cambiado sea rechazado.
Solicita logout, cierre de sesión y finalización; no reintenta login ni firma,
ni cambia de mecanismo. Tiene un límite de 120 segundos y restaura la terminal
al salir. Ante un error, revisar el informe antes de otro intento.

El informe está en `local/auto-sign-*/report.txt`, con directorio `0700` y
archivo `0600`. Solo contiene resultados numéricos y estados fijos: no guarda
PIN, certificado, ID, digest ni firma. Para éxito deben aparecer:

```text
configuration=none_dev_null
detection=normal_driver_dispatch
signature_verification=valid
changed_message_verification=rejected
session_close_result=0
finalize_result=0
test_result=signature_verified_cleanup_api_ok
helper_exit_status=0
```

`logout_result=257` (`CKR_USER_NOT_LOGGED_IN`) también es válido cuando el
driver ya cerró la autenticación al firmar. El cierre se comprueba a nivel de
API: este helper no filtra ni cuenta APDU, resets o VERIFY, ni impone una
reserva propia como el diagnóstico específico de JCOP4. La autodetección puede
hacer lecturas públicas y PKCS#11 puede descubrir otros lectores. La verificación
criptográfica no comprueba confianza o revocación del certificado, ni firma PDF.

## Firmador con autodetección

Para abrir la GUI con la misma biblioteca y selección automática de tarjeta:

```bash
nix develop -c make firmador/auto
```

El usuario abre el PDF, elige la llave y escribe el PIN en el diálogo del
firmador. El lanzador no recibe documentos ni PIN por argumentos, no crea
`.conf` y no fuerza driver, perfil ni políticas de sesión. Usa
`OPENSC_CONF=/dev/null`, el módulo y libOpenSC de `build/opensc`, debug cero
y un entorno limitado que conserva el escritorio y los certificados de
confianza. No hereda opciones Java o experimentos de diagnósticos previos.

Guarda `report.txt` y `api.log` en `local/auto-firmador-api-*/`, con directorio
`0700` y archivos `0600`. La traza existente solo contiene nombres fijos de
API y metadatos numéricos, sin PIN, PDF, certificados o firmas. El estado de
salida del firmador se propaga al comando; salir con estado cero no acredita
firma ni cierre de tarjeta. La GUI permite varias operaciones y no tiene la
cuota de una firma ni el timeout del helper `sign/auto`.

El lanzador se comprueba sin Java, GUI ni tarjeta con:

```bash
make test/auto-firmador-launcher
```

## Regresión sin hardware

```bash
nix develop -c make test/auto-sign
```

Compila el helper real con un módulo PKCS#11 independiente de software y RSA
sintético, sin cargar OpenSC ni PC/SC. Prueba firma/verificación, logout ya
realizado, fallos de login/firma/cierre, firma corrupta, objetos ambiguos,
entrada PIN inválida, cancelación y restauración de terminal. También prueba
el lanzador con comandos simulados, entorno vacío, permisos y propagación de
errores. Estas pruebas no sustituyen ejecutar `sign/auto` con cada tarjeta.
