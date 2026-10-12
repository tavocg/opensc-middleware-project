# libjcop3p11: contexto para continuar

Estado documentado el **11 de octubre de 2026**, después de revisar los informes
de las 21:34 UTC y corregir la lectura de ACL de los SDO RSA. Leer este archivo
al retomar el proyecto y contrastarlo con Git y las capturas más recientes.
Es un punto de partida, no una afirmación
de soporte completo de la tarjeta.

## Objetivo y alcance

Desarrollar soporte libre, sobre OpenSC, para una tarjeta **JCOP3 de firma
digital de Costa Rica**, mediante su interfaz de contacto. El usuario tiene
la tarjeta y un lector USB. El primer hito es obtener una firma RSA SHA-256
con `LlaveDeFirma` y verificarla con OpenSSL. Después se probará `firmador`
cargando el módulo PKCS#11 compilado, y se prepararán cambios para OpenSC.

La cadena de comunicación es:

```text
firmador → Cryptoki/PKCS#11 → OpenSC → PC/SC → CCID → lector USB → tarjeta
                                                APDU hacia la tarjeta
```

El nombre elegido por el usuario es **libjcop3p11**. La implementación actual
amplía el driver existente **`iasecc`**; no existe un `card-jcop3.c` ni un módulo
independiente renombrado. La biblioteca compilada sigue siendo:

```text
build/opensc/src/pkcs11/.libs/opensc-pkcs11.so
```

JCOP3 identifica la plataforma, pero la compatibilidad depende de la aplicación
interna y de sus comandos. No asumir soporte de todas las JCOP3. JCOP4,
contactless, CAN/PACE y criptografía poscuántica quedan fuera del primer hito.

## Cómo colaborar con el usuario

- Responder en español. Investigar, editar y verificar de manera autónoma
  mientras sea posible; dar comandos concretos cuando se necesite el lector,
  la tarjeta o una acción desde la terminal del usuario.
- Usar **`nix develop` y objetivos Make**. El prompt conserva el PS1 del usuario
  y antepone `(libjcop3p11)`.
- Mantener commits incrementales, pero **dar al usuario los comandos para
  hacerlos**. No usar `build-commits` ni repositorios alternativos para eludir
  la restricción de escritura de `.git`.
- No pedir nuevamente permiso para investigación o cambios locales ya
  autorizados. El usuario pidió documentar el estado antes de continuar:
  retomar desde los informes existentes, sin repetir automáticamente pruebas.
- En la sesión de desarrollo hasta este punto, el agente no tenía acceso al
  lector y `.git` era de solo lectura. Las pruebas reales las ejecutó el usuario.
- No delegar a otros agentes salvo que se solicite explícitamente.

## Entorno y archivos

Directorio de trabajo: `/home/tavo/Projects/middleware-project`.

| Ruta | Función |
| --- | --- |
| `flake.nix`, `flake.lock` | Herramientas y dependencias fijadas de Nix |
| `Makefile` | Entrada a compilación, consultas y firma de prueba |
| `vendor/opensc/` | Submódulo, rama local `cr-jcop3` |
| `scripts/build-opensc.sh` | Compilación fuera del árbol con PC/SC |
| `scripts/probe-jcop3.sh` | Consultas e informes sin introducir PIN |
| `scripts/sign-jcop3.sh` | Firma interactiva de texto y verificación OpenSSL |
| `scripts/test-iasecc.sh`, `tests/` | Regresiones sin tarjeta |
| `docs/jcop3.md` | Evidencia técnica y evolución de la investigación |
| `docs/idopte.md` | Localización del módulo privativo instalado |
| `local/` | Capturas originales y resultados; excluido de Git |
| `build/opensc/` | Compilación con PC/SC usada por el usuario |
| `build/opensc-offline/` | Compilación de comprobación sin PC/SC |

Consultar las secciones más recientes de `docs/jcop3.md`: algunos pasos
pendientes en secciones antiguas ya se realizaron. No ejecutar de nuevo los
comandos de inicialización Git del README en este repositorio existente.

El entorno incluye OpenSC empaquetado, pcsc-tools, compilador, autotools,
pkg-config, OpenSSL, pcsclite y zlib. En NixOS se necesita el servicio PC/SC
(`services.pcscd.enable = true;`). Compilar no instala nada en el sistema:

```bash
nix develop
make test/iasecc JOBS="$(nproc)"
```

`JOBS` vale 2 por defecto. `READER=0` selecciona lector PC/SC y `SLOT_INDEX=0`
selecciona slot PKCS#11; no son necesariamente el mismo índice.
`inspect/jcop3-pkcs11`, los diagnósticos de política y `sign/jcop3` utilizan
la compilación existente: **no recompilan**.

Las restricciones del entorno del agente pueden impedir usar el daemon de
Nix o acceder a red. Una compilación sin PC/SC sirve para comprobar código,
pero no valida comunicación con la tarjeta.

## Git al momento de esta documentación

Los commits guardados antes de la última corrección son:

- Repositorio principal: **`6a3d4a6`**, `Consolidate agent instructions`.
- Submódulo OpenSC: **`840a7b204`**, `iasecc: support observed ChipDoc PIN policy`.
- Base upstream original: `c7a377af971df03f879c1697e91ebac1f7e28c56`.

Está preparada, todavía sin commit, la corrección de ACL de SDO en
`vendor/opensc/src/libopensc/iasecc-sdo.c`, con regresiones en
`tests/iasecc-sdo.c`, un nuevo diagnóstico `inspect/jcop3-security-env` y
actualizaciones de documentación. Confirmar este estado mediante Git;
el usuario puede haber hecho los commits desde su terminal.

La URL del submódulo aún es `https://github.com/OpenSC/OpenSC.git`. Los commits
locales del driver necesitarán publicarse en un fork y actualizar esa URL
para que otro clon pueda obtenerlos. No se ha enviado un issue ni un PR.
Al continuar, revisar ambos `git status` y conservar cualquier trabajo nuevo;
no restablecer ni actualizar automáticamente el submódulo.

## Identidad y objetos comprobados

- Lector: `HID Global OMNIKEY 3x21 Smart Card Reader`, índice observado 0.
- ATR: `3b:df:18:ff:81:91:fe:1f:c3:00:31:b8:64:0c:01:ec:c1:73:94:01:80:82:90:00:b3`.
  Coincide exactamente con `SC_CARD_TYPE_IASECC_AMOS` de OpenSC. La tarjeta
  eOI es un precedente útil, pero no se debe asumir que usa el mismo perfil.
- AID raíz: **`F0496173456363526F6F74`** (`IasEccRoot`).
- AID de `ChipDoc`: **`E828BD080FD25047656E65726963`**, reconocido como
  `ECC Generic PKI` por OpenSC.
- EF.DIR tiene 256 bytes: un registro ChipDoc y relleno. ODF `5031`,
  TokenInfo `5032`, AODF `7001`, PuKDF `7004`, CDF `7005`.
- Certificados legibles: `001C`, `LlaveDeFirma`, RSA 2048; `001D`,
  `LlaveDeAutenticacion`, RSA 2048.
- PrKDF declara referencias privadas **3 para firma** y **2 para autenticación**.
  Sus permisos PKCS#15 no demuestran todavía cómo realizar la operación RSA.
- ID PKCS#11 de firma: **`4c6c61766544654669726d61`** (`LlaveDeFirma`).
- `Card PIN`: CHV global, referencia **1**; auth ID `436172642050494e`.
  AODF declara mínimo 4 y máximo 20; los atributos públicos CHV indican
  **mínimo 4 y máximo 14**, que usa el driver. No ajustar un PIN existente.
- Existe `Card PUK` como AuthKey. No se ha probado ni se debe utilizar para
  desbloqueo en esta investigación.

PKCS#11 enumera dos certificados y dos llaves públicas RSA, derivadas de los
certificados. Los objetos privados no visibles sin login son esperables.
PuKDF aún produce `-1402`, `Required ASN.1 object not found`; su posible
relleno no se ha resuelto. No afirmar que esta parte funciona completamente.
El módulo anuncia Cryptoki 3.0 y mecanismos RSA, incluido `SHA256-RSA-PKCS`.
Anunciar un mecanismo no demuestra que funcione en esta tarjeta.

## Cambios realizados y límites

1. **ACL compactas de FCP** (`6226bd256`): el parser solo procesaba el primer
   grupo y perdía READ, impidiendo EF.DIR. Ahora recorre todos los grupos,
   valida truncamientos y rechaza alternativas contradictorias que no puede
   representar. La tarjeta confirmó lectura de certificados.
2. **Contexto del PIN global** (`ea864361d`): la ruta PKCS#15 `0001` no es
   seleccionable (`6A82`). Una corrección limitada a Amos + ChipDoc + CHV 1
   global sustituye esa ruta por MF `3F00` en `pkcs15-iasecc.c`.
3. **Conservación de SDO** (`512084237`): `iasecc_sdo_parse_data()` devuelve
   bytes consumidos positivos al tener éxito. El llamador los trataba como
   error y borraba clase/referencia. La comprobación ahora es `rv < 0`.
4. **Política ChipDoc observada** (`840a7b204`): `iasecc-sdo.c/.h` conserva
   `C9 01 01` como `chipdoc_c9`, sin asignarle un significado. Solo acepta
   ese valor en Amos, CHV 1. Para contacto reconoce únicamente la ACL
   capturada `F3 E7 00 E7 FF 00 D0 D3 D3`, conserva los bytes originales y
   expone VERIFY y GET DATA. Otras operaciones no están soportadas; variantes
   de esa ACL se rechazan. No implementa la política contactless.
   `card-iasecc.c` marca `read_verify_only` y bloquea cambio/desbloqueo antes
   de cualquier autenticación.
5. **Todos los grupos de ACL de SDO** (pendiente de commit):
   `iasecc_parse_acls()` también perdía los grupos posteriores al primero.
   Ahora conserva todas las condiciones de los DOCP RSA capturados, acepta
   repeticiones idénticas y rechaza alternativas distintas y truncamientos.
   Las condiciones de firma, autenticación interna y descifrado apuntan al
   entorno de seguridad 3. No se ejecutaron operaciones administrativas.

La condición `00` de VERIFY permite ejecutar el comando sin autenticación
previa; **no acredita el PIN ni evita su comprobación por la tarjeta**.
El significado de `C9` y las condiciones administrativas siguen pendientes.

La corrección de ruta depende de la emulación PKCS#15. Los scripts generan
configuración local con **`try_emulation_first = yes`** y
**`use_file_caching = no`**. El binding nativo por defecto puede omitir esa
corrección: al integrar con firmador habrá que conservar esta configuración
o resolver la selección del perfil. No basta con cambiar la ruta de la .so.

## Verificación y últimas capturas

Hay **49 comprobaciones de regresión** en cuatro archivos: `iasecc-fci.c`,
`iasecc-pin-path.c`, `iasecc-sdo.c`, `iasecc-pin-policy.c`. Incluyen respuestas
capturadas, conservación/copia de metadatos, rechazo de perfiles distintos y
transporte simulado que verifica que cambio/desbloqueo no autentican.
Pasaron en la comprobación local; también terminó la compilación completa
sin PC/SC, registrada en `build/opensc-offline/rsa-acl-build.log`.
Esto no sustituye pruebas de firma en hardware.

| Captura local | Resultado relevante |
| --- | --- |
| `jcop3-inspection-WAZLkGL7.txt` | Certificados legibles tras corregir FCP |
| `jcop3-signature-S8Pd9rPG/report.txt` | Único intento de firma: `C_Login` falla con `CKR_GENERAL_ERROR`; no hay firma |
| `jcop3-pin-context-6Hd0jGpP.txt` | CHV 1 accesible en raíz; `0001` no seleccionable |
| `jcop3-pkcs11-gYBEWLz8.txt` | SDO ya conserva CHV 1, pero parser antiguo rechaza `C9` |
| `jcop3-pin-policy-9U5otg3U.txt` | Selecciones anidadas devuelven DOCP entero; controles idénticos, 7/7 intentos |
| **`jcop3-pkcs11-cRVr2acc.txt`** | Última consulta, 21:34:21 UTC, OpenSC `840a7b204`: política del PIN leída, límites 14/4 e intentos 7/7 |
| **`jcop3-key-policy-egE7wr1j.txt`** | Última consulta, 21:34:35 UTC: SELECT ChipDoc y GET DOCP de RSA_PRIVATE 2 y 3 devuelven `9000` |

Los dos últimos informes ya se revisaron: **no pedir ejecutarlos de nuevo como
primer paso**. Los DOCP RSA se reprodujeron en las regresiones del parser
corregido; ambos declaran tamaño 256 bytes y ACL idénticas. La condición de
firma `13` significa autenticación de usuario mediante SE 3 según OpenSC;
falta conocer su plantilla y CHV asociado. Las consultas solicitaron solo
DOCP `A0`, nunca la plantilla `7F48` ni componentes privados.

TokenInfo no incluye `supportedAlgorithms`, y las llaves muestran
`Algo_refs: 0`. El camino actual de `iasecc_set_security_env()` necesita esa
tabla para resolver las referencias de algoritmo de RSA. Todavía no se ha
preparado una adaptación para su ausencia. Tampoco se ha comprobado que la
ruta de firma `ChipDoc::0003` sea seleccionable. Estos prerrequisitos se
investigan antes de otro intento autenticado.

Las consultas vacías de VERIFY devuelven `63 C7`: PIN no verificado, 7 intentos
restantes. No llevan el valor del PIN. El primer intento de firma no tuvo
depuración APDU; no se puede asegurar a partir del registro si el PIN llegó
a transmitirse. No hay evidencia de PIN incorrecto ni de firma exitosa.
Un `exit_status=0` en una herramienta no garantiza que no haya errores
internos de parsing: revisar las respuestas y el contexto.

## Próximo trabajo, en orden

1. Dar al usuario los comandos de commit para la corrección de ACL de SDO
   y el diagnóstico, y ejecutar desde su terminal:
   `make test/iasecc JOBS="$(nproc)"` y `make inspect/jcop3-security-env`.
   El último objetivo no recompila ni solicita PIN. Envía seis APDU:
   SELECT/GET DATA de SE 3 en ChipDoc y raíz, y SELECT ChipDoc/ruta `0003`.
   Se verificó con transporte simulado, todavía no con la tarjeta.
2. Revisar `local/jcop3-security-env-*` cuando exista una captura real:
   identificar el CHV de SE 3 y si la ruta `0003` es seleccionable. Resolver
   las referencias de algoritmo ausentes mediante evidencia, sin asignarlas
   a ciegas. Evitar barridos arbitrarios y comandos administrativos.
3. Si aparece otro fallo reproducible, preparar una corrección con evidencia
   y regresión pertinente. Dar comandos para commit primero en el submódulo
   y luego en el repositorio principal, registrando su nuevo puntero.
4. Cuando los prerrequisitos estén resueltos, indicar al usuario **un intento**
   con `make sign/jcop3`. Revisar login, firma y verificación OpenSSL; ante
   fallo, detenerse e investigar, sin reintentos automáticos de PIN.
5. Después de una firma verificada, probar firmador con la biblioteca local
   y la configuración PKCS#15 pertinente. La variable que usa su lanzador es:

   ```bash
   LIBASEP11="$(pwd)/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so" firmador
   ```

   Ese comando muestra la selección del módulo; falta definir/proporcionar
   `OPENSC_CONF` para el perfil antes de considerarlo una prueba completa.
6. Ampliar compatibilidad y validación (concurrencia, caché, mecanismos reales,
   versiones/plataformas) antes de reclamar soporte completo o enviar cambios
   upstream. No generar nuevas llaves en una tarjeta emitida para probarlo.

**Todavía no existe una firma producida por este driver, ni una prueba exitosa
del firmador usando OpenSC.**

## PIN, privacidad e Idopte

- El PIN se introduce únicamente por el usuario en una terminal, sin eco;
  nunca en argumentos, variables de entorno, archivos, chat o informes.
- `sign-jcop3.sh` exige stdin y stdout TTY porque `util_getpass` usa stdout
  para desactivar el eco. No redirigir ni pasar por `tee` la salida del comando
  autenticado. El helper fija `OPENSC_DEBUG=0` y `debug = 0`.
- No habilitar capturas APDU durante login/firma: podrían contener el PIN.
  No se ha capturado una sesión autenticada de Idopte.
- La prueba firma el texto técnico visible en `message.txt`, no un PDF.
  Verifica `signature.bin` con la llave pública del certificado mediante
  OpenSSL. No intenta exportar la llave privada de la tarjeta.
- Firmar con la tarjeta no requiere Internet. Descargar dependencias Nix sí
  puede requerirlo; firmador puede usar red para otros servicios como sellado
  de tiempo o comprobación de revocación.
- `local/` contiene certificados y otros datos identificables. Mantenerlo
  fuera de Git y evitar volcar capturas completas al chat. Los scripts usan
  `umask 077`; no publicar los informes sin revisar y anonimizar.
- No inicializar, borrar, generar llaves, cambiar PIN ni desbloquear la tarjeta.

El módulo privativo que funciona con el firmador del sistema es
`/run/current-system/sw/lib/SCMiddleware/libidop11.so`, repaquetado para Nix
(Idopte 6.23.50.5-1). `LIBASEP11` conserva el nombre histórico, pero puede
seleccionar ese módulo. El archivo del instalador en `vendor/idopte/` tiene
dependencias/rutas que no funcionan igual en Nix; ver `docs/idopte.md`.
La revisión estática de `libt_ias.so` no confirmó el significado de `C9`.
No copiar código privativo al driver libre ni incorporar binarios a Git.

## Instrucciones de OpenSC

Leer [vendor/opensc/AGENTS.md](vendor/opensc/AGENTS.md) al trabajar allí.
Antes de preparar un reporte upstream requiere reproducir contra `master`
actual y proponer una solución concreta. Esa actualización debe preservar
la rama local y su evidencia. Los mensajes de commit deben ser concisos.
Si se sospecha una vulnerabilidad, seguir `SECURITY.md` y su procedimiento
privado; no publicar un reporte de seguridad. No se ha iniciado tal reporte.

Actualizar este documento cuando cambie el hito, el bloqueo o la evidencia
principal, conservando las capturas originales en `local/`.
