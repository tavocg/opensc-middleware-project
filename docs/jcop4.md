# Pruebas de JCOP4

El 13 de octubre de 2026 el usuario informó que dispone de una JCOP4.
Se investiga en este repositorio y con el mismo submódulo OpenSC, conservando
las capturas y pruebas de JCOP3. Todavía no hay evidencia de compatibilidad
de esta tarjeta ni se conocen su ATR, aplicación, certificados o tamaño RSA.
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
siguen fuera de estas pruebas. La consulta inicial tiene cuatro comprobaciones
con transporte simulado en `tests/jcop4-probe-launcher.sh`; no prueban hardware.
