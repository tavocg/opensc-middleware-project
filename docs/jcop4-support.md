# Estado del perfil JCOP4 RSA 2048

Estado al 14 de octubre de 2026, Costa Rica. Proyecto `28d3ff0`,
OpenSC `52057cc6a`. Esta es la referencia del estado actual; la investigación
y las etapas anteriores están en [jcop4-opensc-integration.md](jcop4-opensc-integration.md).

La [revisión posterior del parche frente a master](opensc-minimal-diff.md)
documenta la limpieza del árbol de trabajo, su formato y las pruebas de
regresión; las pruebas físicas de esta página corresponden al HEAD citado.

El módulo local funciona con la JCOP4 disponible y el firmador instalado en
GNU/Linux. Es un perfil experimental optativo de `iasecc`, con OpenSSL 3,
PKCS#15 nativo y firma RSA 2048/PKCS#1 v1.5/SHA-256. No caracteriza todas las
tarjetas JCOP4 ni sustituye todavía una revisión para incorporar el código a OpenSC.

## Comprobaciones realizadas

| Recorrido | Evidencia real | Alcance |
| --- | --- | --- |
| Enumeración sin PIN | Z5agqf5n | Dos certificados y dos llaves públicas; privadas ocultas. |
| `CKM_SHA256_RSA_PKCS`, una llamada | v0hqHrzR | Texto técnico, firma verificada, contenido alterado rechazado y cierre completo. |
| `CKM_RSA_PKCS` | XzQvJtgn | DigestInfo SHA-256 canónico, firma verificada, negativo y cierre completo. |
| Firma multipart desde firmador | 2VJ372cf | PDF CAdES válido con el certificado de firma observado. |
| Dos firmas consecutivas | KKBZwTnU | Dos logins y dos PDF verificados, con la misma GUI. |
| Retirada/reinserción | 4h1t4TnA | `CARD_CHANGED`, limpieza del token y nuevo login/firma; PDF anteriores y posteriores verificados. |

Los informes están en `local/`, fuera de Git. Los PDF se comprobaron offline
con Mutool y OpenSSL: rangos activos, Contents/DER/relleno, conservación y
cobertura de bytes originales, CMS, certificado DER coincidente, negativo
de un bit y ambos sellos RFC3161 con sus huellas SHA-512. No se evaluaron
confianza, revocación, identidad del titular o render de páginas.

En XzQvJtgn hubo un login, doce intercambios autenticados, un VERIFY con
credencial y un PSO, resets inicial/final correctos, once locks/unlocks y
desconexión de tarjeta/lector y liberación de contexto correctas. Logout101
es esperado después de que PSO cierre la reserva y la sesión vuelva a PUBLIC.

La prueba de reinserción observó PRESENT|CHANGED, no una consulta que devolviera
ausencia sostenida. Los dos eventos de limpieza de token no equivalen a dos
retiradas físicas. La traza GUI no permite contar VERIFY/reset/PSO físicos.

## Límites del perfil

- Solo la llave de firma nativa validada, referencia `0x91`, RSA 2048. La
  privada de autenticación `0x90` no se expone para firmar.
- SHA-256 con PKCS#1 v1.5; `CKM_RSA_PKCS` acepta el DigestInfo correspondiente,
  no cualquier entrada RSA cruda. PSS, otros hashes de firma, descifrado,
  generación de llaves, ECDSA, RSA 3072 y administración no están anunciados.
- PIN de usuario por software, ASCII imprimible de 6 a 14 bytes para el
  formato observado. Sin PIN-pad, PACE, CAN, cambio o desbloqueo de PIN.
- Estado protegido previo `63C7` exigido por la política conservadora actual.
  No demuestra un máximo de siete intentos: otros estados positivos también
  se rechazan antes de enviar la credencial. Su tratamiento requiere trabajo
  adicional, sin probar PIN incorrecto en la tarjeta por inercia.
- Cachés de PIN y archivos desactivadas. La reserva termina tras PSO; otra
  firma necesita un login explícito. `atomic` y `lock_login` genéricos se rechazan.
- GNU/Linux y el lector probado. No se comprobaron Windows, macOS, otros
  lectores, otras personalizaciones o todos los recorridos de hotplug.

Varios escenarios solo tienen evidencia de software: dos sesiones nativas,
errores MAC/transporte/teardown, consultas de tamaño y buffer pequeño,
configuración incompatible, pérdida de ownership y ausencia/remoción simulada.
No hay prueba real de otros clientes concurrentes durante login/firma.
ASan/UBSan de fixtures y helpers pasó con `detect_leaks=0`; las bibliotecas
precompiladas no se instrumentaron y no se comprobaron fugas.

## Inventario local del cambio

Los once commits de integración posteriores a `3dfef6704`, hasta `52057cc6a`,
tocan 26 archivos: 2803 líneas añadidas y 10 eliminadas. Hay 15 archivos
nuevos y 11 existentes modificados. Esta cuenta excluye tests y herramientas
del repositorio principal y cambios anteriores del mismo submódulo.

| Componente | Ubicación |
| --- | --- |
| Perfil, estado, binding y routing | `iasecc-chipdoc*.h`, `iasecc-chipdoc-bind.c`, `card-iasecc.c`, `iasecc.h`, `cards.h` |
| Canal y operaciones protegidas | `iasecc-chipdoc-sm.c/.h`, headers `jcop4-channel-*` y `jcop4-dh-control.h` |
| Binding nativo y cachés | `pkcs15.c` |
| Estado de login, objetos, mecanismos y cierre | `framework-pkcs15.c`, `misc.c`, `pkcs11-object.c`, `pkcs11-session.c` |
| Compilación y opción | Makefiles de libopensc y `opensc.conf.example.in` |

El historial completo desde la base común local
`c7a377af971df03f879c1697e91ebac1f7e28c56` toca 40 archivos: 3539 líneas
añadidas y 87 eliminadas. Incluye correcciones JCOP3, diagnósticos y experimentos
anteriores. No enviar ese historial completo como un único parche JCOP4 ni
asumir que los once commits recientes pueden aplicarse sin sus dependencias.
Por ejemplo, los archivos compartidos del framework aún contienen llamadas
al logger temporal `api-trace.h`, añadido antes de esa integración.

`origin/master` local apunta a `fc27ee8768cb882fceb99535dce8b6a966534dcb`;
no se actualizó en esta revisión y no se presenta como master vigente. Las
cifras del historial propio usan su base común, para no contar cambios ajenos
de esa rama remota como parte del trabajo local.

## Trabajo local siguiente

1. Obtener y compilar una copia actual de master en un checkout separado,
   conservando el submódulo y build que pasaron las pruebas de tarjeta.
2. Separar funcionalidad JCOP3, integración JCOP4 y diagnósticos temporales;
   resolver dependencias antes de elegir la división final de cambios.
3. Adaptar tests para la infraestructura de OpenSC. Hoy viven en el repositorio
   principal y varios runners fijan `vendor/opensc` y `build/opensc`; no acreditan
   automáticamente un checkout diferente.
4. Revisar el acoplamiento específico al perfil en PKCS#15/PKCS#11, la política
   de estado PIN, las capacidades declaradas y los backends criptográficos.
5. Solo después preparar el cambio para revisión pública. Las instrucciones
   de `vendor/opensc/AGENTS.md` requieren comprobar master antes de preparar
   reportes; aún no se preparó ni envió un PR o reporte de bug.

No hacen falta más firmas para repetir los recorridos ya verificados. RSA 3072
necesitaría parametrizar buffers, tamaños y políticas validadas y disponer de
evidencia de ese modelo; no se activa ampliando un límite por suposición.
