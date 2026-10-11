# Investigación: JCOP3 de Costa Rica

## Inventario

| Dato | Valor / evidencia |
| --- | --- |
| Generación indicada por el propietario | JCOP3; pendiente de corroborar técnicamente |
| Interfaz inicial | Contacto |
| Modelo de lector | HID Global OMNIKEY 3x21 Smart Card Reader; índice PC/SC 0 |
| Sistema y arquitectura | Linux 6.18.55 x86_64 |
| Versión de OpenSC empaquetado | 0.27.1; corroborado con `opensc-tool --info` y la ruta del ejecutable |
| Commit del submódulo OpenSC | `c7a377af971df03f879c1697e91ebac1f7e28c56`; sin cambios en la captura |
| Versión y ruta del módulo Idopte | Pendiente |
| ATR por contacto | `3b:df:18:ff:81:91:fe:1f:c3:00:31:b8:64:0c:01:ec:c1:73:94:01:80:82:90:00:b3` |
| Entrada coincidente en OpenSC | `IAS/ECC v1.0.1 Amos`, tipo `SC_CARD_TYPE_IASECC_AMOS`, driver `iasecc` |
| AID raíz IAS/ECC | `F0496173456363526F6F74` (`F0` seguido de `IasEccRoot`), leído de EF.ATR y seleccionado con éxito |
| AID de aplicación PKCS#15 | Pendiente de leer EF.DIR |
| Versión de aplicación | Pendiente |
| Tipo y tamaño de llave del certificado | Pendiente |
| Mecanismo de firma usado por el firmador | Pendiente |

Un ATR o AID coincidente es una pista, no una demostración de compatibilidad.
Las capacidades del chip tampoco demuestran qué permiten las llaves emitidas.

## Primera observación: 2026-10-11

El informe `local/jcop3-inventory-mSphakPR.txt`, generado a las 20:06:50 UTC,
muestra el lector en el índice 0 con tarjeta presente y la lectura exitosa del
ATR. El ATR completo coincide con la entrada Amos en
`vendor/opensc/src/libopensc/card-iasecc.c`, línea 93 del commit registrado.
No coincide con los ATR de `card-eoi.c` de ese commit.

La coincidencia estática no demuestra que la inicialización IAS/ECC, el mapa
PKCS#15 o la firma sean compatibles. El siguiente paso es probar identificación
y enumeración pública con OpenSC, sin PIN, mediante `make inspect/jcop3`.
Si falla, el informe con depuración permitirá localizar el primer comando o
estructura incompatible antes de comparar con Idopte.

En esta captura `opensc-tool --version` devuelve solo la información de revisión
Git; el script ahora usa `--info` para registrar la versión y funciones habilitadas.

## Hitos

- [x] PC/SC enumera el lector y detecta la tarjeta.
- [x] ATR registrado mediante el script de observación.
- [x] ATR comparado con las tablas de IAS/ECC y eOI.
- [x] Driver IAS/ECC inicializa la tarjeta y selecciona su AID raíz.
- [ ] AID y APDU comparados con IAS/ECC y eOI.
- [ ] Enumeración pública de Idopte capturada y documentada.
- [ ] Certificado y referencia de llave encontrados.
- [ ] OpenSC enumera los objetos correctamente.
- [ ] Una firma con el mecanismo observado se verifica externamente.
- [ ] Firma reproducida sin cargar Idopte.
- [ ] Retirada, reinserción, errores y varias sesiones comprobados.

## Registro de comandos observados

No incluir PIN, datos personales ni certificados identificables en este documento.
Guardar los originales en `local/` y publicar únicamente ejemplos revisados.

| Operación | Estado previo | APDU documentada | Respuesta / SW | Evidencia |
| --- | --- | --- | --- | --- |
| Seleccionar aplicación | Pendiente | Pendiente | Pendiente | Pendiente |
| Leer certificado | Pendiente | Pendiente | Pendiente | Pendiente |
| Autenticar | Pendiente | Documentar formato; ocultar PIN | Pendiente | Pendiente |
| Seleccionar llave y algoritmo | Pendiente | Pendiente | Pendiente | Pendiente |
| Firmar | Pendiente | Pendiente | Pendiente | Pendiente |

Para cada captura registrar lector, interfaz, middleware, mecanismo y parámetros,
estado de caché y si se retiró la tarjeta antes de la operación. Distinguir hechos
observados de hipótesis sobre el protocolo.

## Inspección: 2026-10-11

El informe `local/jcop3-inspection-EkcrTRgz.txt` muestra que `opensc-tool --name`
termina con éxito y devuelve `IAS-ECC`. La selección del AID raíz
`F0496173456363526F6F74` y de EF.DIR (`2F00`) también devuelve `9000`.

El primer fallo al enumerar aplicaciones ocurre en el middleware, antes de
transmitir READ BINARY para EF.DIR:

```text
_iasecc_sm_read_binary: iasecc_sm_read() 'READ' ACL not present
sc_enum_apps: sc_read_binary() failed: -1407 (Requested object not found)
```

El FCP de EF.DIR declara un archivo transparente de 256 bytes y contiene:

```text
A1 0C 8C 04 40 00 01 00 9C 04 40 00 01 00
```

En el código revisado, `iasecc_process_fci()` interpreta únicamente el primer
byte de modos de acceso (`40`) y su condición (`00`); no procesa el grupo
siguiente (`01 00`). El formato compacto admite varios grupos de modo y
condiciones. Esto apunta a que la regla de lectura del segundo grupo no se
incorpora al modelo de ACL de OpenSC.

Referencia para el formato compacto: [ETSI TS 102 221, sección 9.2.5](https://www.etsi.org/deliver/etsi_ts/102200_102299/102221/18.01.00_60/ts_102221v180100p.pdf).
La aplicabilidad concreta al perfil IAS/ECC de esta tarjeta debe comprobarse.

Al no poder leer EF.DIR, OpenSC intenta las rutas predeterminadas `5015` y
`5031`, recibe `6A82`, y termina con `PKCS#15 binding failed: Unsupported card`.
Esto no prueba que falten certificados ni que el mapa sea propietario.
La ausencia de `/etc/opensc.conf` tampoco impide la inicialización observada.

Siguiente comprobación: `make read-dir/jcop3`. Utiliza el driver `default` para
seleccionar la raíz observada y EF.DIR, y leer exactamente dos bloques de
128 bytes. No verifica PIN ni modifica datos. La tarjeta conserva el control
de acceso: si deniega la lectura, se registra el estado devuelto.

Si la lectura funciona, analizar el directorio y preparar una adaptación que
procese todos los grupos de permisos, preservando sus condiciones. No sustituir
la falta de ACL por permiso incondicional. Antes de reportar un problema a
OpenSC, comprobarlo en una compilación de `master`; esta observación corresponde
al paquete 0.27.1 y todavía no se ha validado con una compilación del submódulo.
