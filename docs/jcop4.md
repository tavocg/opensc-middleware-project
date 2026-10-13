# Pruebas de JCOP4

El 13 de octubre de 2026 el usuario informó que dispone de una **JCOP4 con
certificados RSA 2048**. Ese tamaño lo aporta el usuario y se contrastará
con los certificados públicos. También requiere que el diseño permita
modelos RSA 3072 y futuras generaciones, sin duplicar un driver por modelo.
Se investiga en este repositorio y con el mismo submódulo OpenSC, conservando
las capturas y pruebas de JCOP3. Ya se obtuvo el ATR; todavía no hay evidencia
de compatibilidad de su aplicación ni se han leído sus certificados.
La etiqueta JCOP4 la aporta el usuario; el ATR por sí solo no demuestra la
generación de hardware ni que la aplicación sea idéntica a la JCOP3.

## Primera consulta

Cerrar firmador y otros clientes de tarjeta antes de sustituir la JCOP3 por
la JCOP4. El perfil `lock_login` puede reservar el lector hasta cerrar la GUI.
Con la compilación existente:

```bash
nix develop
make probe/jcop4 READER=0
```

El objetivo utiliza `build/opensc/src/tools/opensc-tool` para consultar
versión, lectores y ATR. No recompila ni instala. No identifica aplicaciones
ni inicializa el driver de tarjeta: `--atr` termina antes de
`sc_connect_card()`; `--list-readers` se ejecuta sin `--verbose`.
No envía APDU de exploración, no solicita PIN ni intenta autenticar o firmar.
Cada consulta recibe stdin vacío, depuración 0 y configuración privada, sin
las banderas experimentales de JCOP3 ni trazas API heredadas.

Los resultados quedan en `local/jcop4-probe-*/report.txt`, junto con una
configuración propia y los commits/estados Git. Los directorios tienen
permisos 0700 y los archivos 0600. Ante un error se detiene, sin reintentos.
El mensaje de consulta terminada indica que las herramientas salieron con
estado 0; revisar que el informe tenga lector y ATR antes de avanzar.

## Resultado inicial: 13 de octubre, 07:19:50 UTC

Informe `local/jcop4-probe-9XynOJDU/report.txt`: lector OMNIKEY 3x21,
índice 0, tarjeta presente; las tres consultas terminaron con estado 0.
OpenSC compilado `3dfef6704`, versión anunciada 0.27.1, PC/SC habilitado.
La consulta se ejecutó antes del commit del lanzador, que ahora está guardado
en el principal como `0c883c2`.

ATR observado:

```text
3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a
```

Coincide exactamente con la entrada de contacto de `eoi_atrs[]` en
`vendor/opensc/src/libopensc/card-eoi.c`. Difiere del ATR de la JCOP3 probado
en este proyecto y no aparece en `iasecc_known_atrs[]`. Esto es una coincidencia
de identificación, sin prueba de que sean iguales la aplicación, las llaves
o la política de autenticación.

Además, `build/opensc/config.h` tiene `ENABLE_SM`, pero no
`ENABLE_OPENPACE`; el script de compilación usa `--disable-openpace`.
Con esa combinación `sc_get_eoi_driver()` devuelve NULL, por lo que eOI
no está habilitado en esta compilación. No cambiar esa dependencia ni la
detección de iasecc sin identificar primero la aplicación de esta tarjeta.

## Consulta de AID conocidos

Con firmador cerrado y manteniendo la misma JCOP4 insertada:

```bash
make inspect/jcop4-aids READER=0
```

No recompila. Comprueba primero lector y ATR; si este no coincide con el
observado, se detiene antes de enviar SELECT. Después ejecuta cuatro SELECT
por nombre, cada uno con `--card-driver default`, para evitar la detección
automática y los caminos de PIN/CAN/PACE de drivers especializados:

| Candidato | AID | Fuente |
| --- | --- | --- |
| Raíz IAS/ECC | `F0496173456363526F6F74` | Capturas de la JCOP3 |
| ChipDoc Generic | `E828BD080FD25047656E65726963` | EF.DIR de la JCOP3 |
| NXP1 | `E828BD080F014E585031` | Documentación eOI de OpenSC |
| NXP0 | `E828BD080F014E585030` | Documentación eOI de OpenSC |

Los dos últimos AID están documentados para eOI en la
[wiki oficial de OpenSC](https://github.com/OpenSC/OpenSC/wiki/Slovenian-EID).
No se trasladan a esta tarjeta sus roles, algoritmos ni requisitos de PIN.
Es una lista acotada; no se hace un barrido de identificadores.

El informe y respuestas quedan en `local/jcop4-aids-*/`, con permisos privados,
stdin vacío, depuración 0 y experimentos JCOP3 desactivados. Solo se envía
SELECT; no hay VERIFY, MSE, lectura de componentes privados, firma ni
operaciones administrativas. `9000` indica SELECT aceptado y `6A82` que el
candidato no se encontró en esa consulta. Ninguno demuestra todavía que
funcione el perfil PKCS#15 o la firma. Otras respuestas, fallos de transporte
o un estado ausente/ambiguo detienen la consulta sin reintentos. Se revisa
el SW aunque `opensc-tool` devuelva estado de proceso 0.

## Resultado SELECT: 13 de octubre, 07:28:12 UTC

Informe `local/jcop4-aids-qixa9ZwC/report.txt`, OpenSC `3dfef6704`:
lector 0 con tarjeta presente y el mismo ATR. Los cuatro SELECT devolvieron
**6A82**; ninguno se seleccionó en esta consulta. Los comandos de herramienta
salieron con estado 0, pero se revisaron los SW reales.

Esto no demuestra que la tarjeta carezca de aplicaciones PKCS#15 ni que
requiera PACE; podrían ser otros AID o depender del contexto de selección.
No ampliar la tabla ATR de iasecc, reutilizar las excepciones JCOP3 o
habilitar eOI únicamente por la coincidencia del ATR.

## Lectura pública de EF.ATR y EF.DIR

Con firmador cerrado y la misma JCOP4 insertada:

```bash
nix develop
make inspect/jcop4-files READER=0
```

Se compila únicamente `tools/card-public-info.c`, contra el OpenSC existente,
en `build/opensc/card-public-info`. No recompila OpenSC ni instala nada.
Después de la comprobación inicial del ATR, la herramienta conecta con driver
`default` y vuelve a comprobar el ATR conectado antes de consultar archivos.
Mantiene una transacción durante las dos consultas y la libera al terminar.

Usa las funciones de OpenSC `sc_parse_ef_atr()` para `3F00/2F01` y
`sc_enum_apps()` para `3F00/2F00`. Estas seleccionan y leen los archivos públicos;
no se autentica, firma, configura MSE ni se lee un archivo de CAN. Un SELECT
fallido no procede a leer ese archivo. Si EF.ATR no se encuentra, consulta
EF.DIR; otros errores de EF.ATR detienen. Un EF.DIR ausente queda registrado
como no encontrado en este contexto, sin afirmar ausencia de aplicaciones.
No hay reintentos ni cambios de driver automáticos.

El informe `local/jcop4-files-*/report.txt` registra resultados de cada operación,
AID raíz y capacidades publicadas por EF.ATR, y AID/etiquetas de EF.DIR.
Depuración 0, stdin vacío, configuración propia y permisos privados. Un valor
0 de tamaño máximo APDU significa que no se obtuvo ese dato; no prueba que
no admita APDU extendidas. Una enumeración de aplicaciones con salida de
proceso 0 tampoco demuestra que los certificados o la firma funcionen.

## Diseño requerido para 2048, 3072 y futuras generaciones

Separar tres dimensiones: plataforma JCOP, protocolo/aplicación y tamaño
de la llave. El ATR es una pista para descubrir el perfil; los AID y metadatos
de la aplicación deben delimitar sus adaptaciones. El diseño debe permitir
varios perfiles sin copiar un driver por generación de hardware.

La rama actual tiene excepciones medidas para JCOP3 que aún se limitan a
Amos + ChipDoc + llave de firma 3 + RSA 2048. No equivale a una implementación
modular terminada. Al integrar JCOP4:

- Centralizar la identificación y las diferencias de perfiles, conservando
  las restricciones de seguridad que se hayan confirmado en cada tarjeta.
- Separar las reglas de PIN de los tamaños RSA y obtener referencias/tamaños
  de PKCS#15/DOCP cuando esos metadatos sean fiables.
- Separar hash/DigestInfo, operación RSA y transporte. RSA 2048 produce una
  firma de 256 bytes; RSA 3072 produce 384. Revisar buffers, Le, GET RESPONSE
  y APDU extendidas, además de los mecanismos anunciados. El camino AT actual
  usa un buffer de 261 bytes y Le de 256; no basta con agregar 3072 a la lista.
- Mantener regresiones por perfil, probar respuestas de 384 bytes mediante
  transporte simulado y validar RSA 3072 con hardware cuando esté disponible.
  Las pruebas de la tarjeta actual corroborarán RSA 2048, no RSA 3072.

La consulta pública nueva es independiente del modelo y del tamaño RSA;
el lanzador JCOP4 conserva la restricción al ATR observado como control de
la prueba. No usar ese control diagnóstico como detección final del driver.
Se decidirá cómo reutilizar iasecc/eOI cuando se conozca el perfil real.

## Después del ATR

1. Comparar el ATR y la detección prevista por OpenSC con la evidencia JCOP3.
2. Preparar consultas públicas de aplicación, directorio PKCS#15 y certificados
   según ese resultado, registradas aparte. No reutilizar a ciegas las APDU
   ni las excepciones de PIN/referencia privada medidas en JCOP3.
3. Comparar AIDs, política pública de PIN, referencias privadas, mecanismos
   y tamaños RSA antes de modificar la detección o habilitar una firma.
4. Si corresponde al mismo protocolo, ampliar un perfil delimitado dentro de
   `iasecc` y agregar regresiones con las respuestas nuevas. No crear un
   driver por generación de plataforma sin diferencias de protocolo.
5. Obtener una firma de prueba verificada con el certificado de esta tarjeta
   y después probar firmador. RSA 3072 requiere revisar el transporte de
   respuestas de 384 bytes; no basta con anunciar ese tamaño.

Se mantienen las restricciones de privacidad y de operaciones sobre tarjetas
emitidas de [AGENTS.md](../AGENTS.md). NFC, CAN/PACE y algoritmos poscuánticos
siguen fuera de estas pruebas. Pasaron once comprobaciones del lanzador en
`tests/jcop4-probe-launcher.sh` y ocho del flujo de consulta pública en
`tests/card-public-info.c`. Esta última se compila contra OpenSC pero simula
las API de conexión/lectura, sin PC/SC ni lector. También compiló la herramienta
real contra `build/opensc-offline`. Resultados C en
`build/opensc-offline/card-public-info-tests.log`. No prueban hardware ni se
repitió toda la suite JCOP3.
