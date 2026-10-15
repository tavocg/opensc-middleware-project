# Revisión del parche frente a master

14 de octubre de 2026, Costa Rica. Punto inicial: proyecto `d195c2b`,
submódulo `52057cc6a`, ambos limpios. Referencia de comparación:
`origin/master` en `fc27ee8768cb882fceb99535dce8b6a966534dcb`, también presente
en `build/opensc-upstream-review`. El FETCH_HEAD local tiene fecha 18:18 CR.
La consulta remota adicional falló por resolución DNS; no se afirma haber
verificado desde esta sesión que GitHub conserve ese mismo HEAD.

## Cambios

- Se incorporó la corrección de master para pasar `reader_states` por
  referencia y calcular el vencimiento después de detectar la tarjeta. Se
  conservan nuestros controles de `CARD_CHANGED` y el presupuesto por llamada.
- El parser de grupos ACL se comparte entre FCI y SDO mediante un helper
  interno en `iasecc-sdo.h`. No añade un símbolo a la API de libopensc. Se
  conserva el rechazo de truncamientos y alternativas incompatibles, y el
  tratamiento de operaciones que el mapeo de archivos ignora. El cuerpo que
  convierte condiciones a entradas ACL mantiene el estilo de master.
- Las trazas PKCS#11 llaman al mismo helper de metadatos que ya utilizaba su
  wrapper. Se eliminó ese reenvío y su declaración; `src/pkcs11/debug.c` quedó
  idéntico a master. Eventos, campos y conservación de errno siguen probados.
- Los headers nuevos se agregan a `noinst_HEADERS` sin redistribuir la lista
  original. La lista de objetos de Windows incluye también el binding, como
  la de Automake; no se acredita una compilación Windows.
- Los 16 archivos C/header ausentes en master siguen `.clang-format` de
  OpenSC, con clang-format 21.1.8. Se comprobó salida estable y equivalencia
  de tokens C, salvo el orden de includes, que además pasó compilación. No
  se aplicó el formatter a archivos existentes completos. Algunos bloques
  nuestros dentro de esos archivos se separaron en sentencias legibles.

Se conservan los perfiles JCOP3 y JCOP4, sus límites, operaciones protegidas,
limpieza, diagnósticos optativos y lanzadores. Retirar esos diagnósticos
requeriría trasladar sus consumidores; borrarlos ahora perdería funcionalidad.

## Tamaño de la diferencia

Las cuentas incluyen todo el historial funcional/experimental de nuestro
submódulo frente a la referencia anterior, no solo los commits JCOP4.

| Ámbito | Antes | Después |
| --- | --- | --- |
| Archivos existentes en master afectados | 24 | 23 |
| Líneas añadidas en esos archivos | 884 | 868 |
| Líneas eliminadas en esos archivos | 94 | 81 |
| Archivos nuevos respecto a master | 16 | 16 |
| Diferencia completa | 40 archivos, +3546/-94 | 39 archivos, +4485/-81 |

El formato requerido aumenta de 2662 a 3617 las líneas de los archivos
nuevos, sin añadir sus tokens funcionales. Por eso el total textual crece,
aunque disminuyen las modificaciones sobre archivos de master. No se
compactaron declaraciones o controles para reducir artificialmente esa cuenta.

## Verificación

- Compilación de master en `build/opensc-master-build` y de nuestro submódulo
  en `build/opensc`, sin instalar en el sistema ni ejecutar herramientas de
  hardware.
- `bash scripts/test-iasecc.sh`: 733 líneas PASS, no 733 casos independientes.
  Incluye regresiones JCOP3, adaptadores JCOP4, sesiones y cambio de tarjeta.
- Diagnóstico público, firma PKCS#11 y lanzador del firmador: 34 líneas PASS.
  Firma con ambos mecanismos, PIN sintético por PTY, lector simulado, MAC,
  errores, cierre, entorno y configuración; no se ejecutó Java/GUI real.
- 196608 comparaciones offline de los parsers FCI/SDO anteriores, extraídos
  del HEAD inicial, contra los nuevos: máscaras de dos grupos, condiciones
  repetidas/alternativas, EF/DF y operaciones ignoradas. Se comparan errores y,
  en éxito, condiciones y entradas ACL; no se exige conservar salida parcial
  de una llamada que devuelve error. Fixture temporal fuera de Git.
- ASan/UBSan: fixtures FCI, SDO y cambio de tarjeta, 71 líneas PASS, con
  `detect_leaks=0`. La biblioteca compartida precompilada no está instrumentada;
  no es una prueba de fugas ni de todos los componentes.
- `git diff --check` y comprobación de formato de los 16 archivos nuevos.

Los logs están ignorados en `local/opensc-minimize-*.log` y
`local/opensc-master-build.log`. Un primer enlace del fixture FCI señaló que
el helper no era un símbolo exportado; se resolvió haciéndolo interno en el
header, antes de las verificaciones finales. clang-format necesitó una
segunda pasada para estabilizar un inicializador anidado.

No hubo PC/SC, tarjeta, PIN real, Idopte o GUI por el agente. Las firmas
físicas documentadas en [jcop4-support.md](jcop4-support.md) corresponden al
código anterior a esta revisión; no son una nueva validación física de estos
cambios. Permanecen los límites de `63C7`, RSA2048/SHA256, lector y plataforma
observados. No hay PR público preparado o publicado.

El contenido de master se incorporó al árbol de trabajo, sin crear merge ni
modificar la historia de Git. Los commits requieren escritura en `.git`,
que sigue restringida en esta sesión.
