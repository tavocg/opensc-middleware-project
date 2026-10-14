# Integrar el perfil JCOP4 observado en OpenSC

Estado al 14 de octubre de 2026,10:13 Costa Rica. Base local del proyecto
`0f51c4c`, submódulo OpenSC `3dfef6704`. Este documento define el siguiente
cambio. **Perfil/estado/limpieza y adaptador interno CA/MF están implementados;
detección/bootstrap y PIN/firma todavía no están conectados al driver público.**

## Evidencia disponible

El informe privado `local/jcop4-channel-signature-extended-JnUUiy9x/report.txt`
confirma una firma real del texto técnico por la JCOP4 disponible:

- Canal, selección MF, estado PIN, login, selección de aplicación y MSE
  autenticados. Un único VERIFY con credencial; consulta previa63C7.
- PSO extendido: respuesta293bytes, objeto87, MAC válido, estado interno y
  exterior9000. Ningún GET RESPONSE, fallback o reintento.
- FirmaRSA2048/SHA256/PKCS1v1.5 verificada con el certificado público guardado.
- Reset final/unlock/disconnect0,14APDU, guard0/interrupted0/helper0;
  texto/firma/certificado guardados con0600 en directorio0700.

OpenSSL CLI verificó también la firma guardada y rechazó el mensaje al
cambiar un bit. El certificado guardado coincide exactamente con el cache
público previo. La comprobación fue offline, sin leer tarjeta o PIN y sin
mostrar mensaje, nonce, identidad del certificado, firma o hashes.

Esto acredita una operación con esa tarjeta/lector y esa secuencia, no
firmas consecutivas, otros lectores/tarjetas, RSA3072, otros hashes,
PKCS#11, PDFs o firmador. Cambiar el formato del PSO hizo suficiente la
secuencia antes fallida, pero DO97/Le/forma extendida se cambiaron juntos:
no se conoce el efecto individual de cada diferencia. Tampoco establece
la política runtime de Idopte ni prueba identidad mediante el canal anónimo.

## Alcance del primer adaptador

Ampliar `iasecc` con un perfil optativo para la aplicación observada.
Conservar lectura PKCS#15 existente y dejar intacto el perfil JCOP3 que ya
funciona. Evitar un driver por tamaño RSA o generación de chip: la selección
debe combinar candidatoATR, AID anunciado y metadatos del perfil, no solo
el nombre JCOP ni el fabricante. Otros perfiles, incluido eOI, no deben
entrar por compartir un ATR.

Primer alcance: firmaRSA2048/SHA256/PKCS1v1.5 con la llave de firma observada,
PIN global1 y formato ya probado. No anunciar automáticamente RNG, generación
de llaves, descifrado, PSS, SHA1, ECDSA, RSA3072 o administración. Resolver
referencia/tamaño desde PKCS#15 y validar contra el perfil soportado;91 es
la referencia corroborada de esta llave, no una regla de todas las tarjetas.
Los datos públicos de política no acreditan un login ni permiten saltar VERIFY.

## Puntos concretos del código local

| Pieza | Funciones/estructura actuales | Trabajo necesario |
| --- | --- | --- |
| Detección e inicialización | `iasecc_match_card`, `iasecc_init`, `iasecc_init_amos_or_sagem` | Candidato/perfil local definidos; wiring a detección/init aún pendiente. No heredar defaultsIAS completos. La tabla de binding actual no tiene una entrada exacta para este ATR. |
| Datos de perfil | `iasecc_private_data`, `iasecc-chipdoc.h`, `iasecc_is_chipdoc_signing_key` | Estado/binding independientes por tarjeta implementados, inicialmente inactivos; el selector legacy sigue reconociendo aplicación/ref3 JCOP3. |
| Canal | `iasecc-chipdoc-sm`, headers privados `jcop4-channel-*` | Implementación compartida y APIs internas CA/MF por dispatcher normal completadas. Quedan providers/libctx/compatibilidad, bootstrap y wiring al driver público. |
| PIN | `iasecc_pin_cmd`, `iasecc_pin_verify`, `iasecc_pin_get_info` | VERIFY/consulta protegidos, un intento, sin copia persistente ni cache de PIN; publicar estado solo autenticado y limpiar login tras reset/error. |
| MSE | `iasecc_set_security_env` | Mapear operación/hash/llave de PKCS#15 al formato probado, manteniendo selección AID bajo SM y orden. |
| Firma | `iasecc_compute_signature`, `iasecc_compute_signature_dst`, `iasecc_compute_signature_at` | Para este perfil usar la operación PSO probada; no INTERNAL AUTHENTICATE o hashing convencional por semejanza. |
| Limpieza | `iasecc_logout`, `iasecc_finish`, `card_reader_lock_obtained` | Limpieza de estado/ownership/MSE/cache y hook de reset implementados. Quedan el contexto crypto real, reset explícito/remoción y reserva/liberación de sesión. |
| PKCS#15/PKCS#11 | `sc_pkcs15_compute_signature`, algoritmos anunciados | Asegurar paso de digestSHA25632; manejar DigestInfo de CKM_RSA_PKCS conforme a la conversión existente, nunca firmarlo como un digest distinto. |

La ruta actual de `iasecc` sigue siendo IAS convencional: no contiene este
establecimientoCA/SSC. No basta añadir un ATR y cambiar un opcode.

## Separación de responsabilidades

1. **Perfil**: AID, candidatoATR, parámetros/encodings de CA/SM, política de
   transporte y operaciones soportadas. Datos independientes del PIN, llave
   concreta, tamaño RSA y estado vivo de tarjeta.
2. **Criptografía**: validación P-521/acuerdo/KDF/AES/CMAC/TLV/SSC, con los
   vectores sintéticos existentes. Sin PC/SC, UI, archivos personales o logs.
3. **Transporte**: constructores cortos/extendidos, límites de tarjeta/lector,
   lock y una respuesta. No reenvío implícito de VERIFY/PSO ni negociación
   mediante barrido de variantes.
4. **Estado de sesión por tarjeta**: canal pendiente/confirmado, SSC, contexto
   seleccionado, login y MSE válidos. Un MAC fallido invalida la sesión;
   no volver a usar el SSC ni enviar credencial en claro como recuperación.
5. **Operaciones del driver**: recibir argumentos OpenSC, validar soporte,
   proteger/transmitir/verificar y mapear resultado autenticado a códigos
   OpenSC. No abrir terminal ni leer `pass`; PIN lo proporciona la aplicación.

El framework `SM_MODE_TRANSMIT` ofrece `get_sm_apdu`/`free_sm_apdu` y cierre.
`sc_sm_single_transmit` valida también la APDU protegida con `sc_check_apdu`.
La integración debe declarar la capacidad/límites correctos para el perfil
antes de usar CASE4_EXT; **no copiar el bypass experimental del helper para
todas las operaciones**. Esa decisión debe probarse con el backend normal.
El codec de diagnóstico solo cubre encodings y operaciones delimitados;
instalarlo como wrapper universal no habilita automáticamente todos los
comandos que pueda emitir PKCS#15 después del login.

Mantener errores63Cx y6A82 autenticados como errores, y rechazar respuestas
desnudas. Devolver plaintext/estado solo tras MAC válido. La política inicial
sin GET/6C/fallback está respaldada por la firma exitosa. La reserva de tarjeta
entre login y firma requiere diseño explícito: el helper tuvo un lock único;
el cliente PKCS#11 divide esa secuencia en varias llamadas y sesiones.
No asumir que los hooks de reset o el cache mantienen permisos por sí solos.

Los tamaños RSA futuros deben parametrizarse a partir de metadata validada,
con cálculo de padding/DO87/TLV/MAC/Le y comprobación de buffers. Pruebas
sintéticas de384bytes podrán preparar RSA3072, pero no acreditarlo en hardware.
Para upstream, revisar también compatibilidad OpenSSL de las APIs usadas por
los prototipos y adaptar mediante `sc-ossl-compat`, en lugar de asumir que
las APIsEVP3 disponibles aquí existen en todas las compilaciones de OpenSC.

## Orden de implementación y validación

1. Perfil/detección y contexto por tarjeta, sin habilitar firma en hardware.
2. Adaptador de canal/transporte y pruebas con APDU/peer/lector sintéticos,
   usando funciones reales OpenSC y serializer, sin alterar límites al azar.
3. PIN y firma mediante API del driver, luego PKCS#15/PKCS#11. Probar argumentos,
   mecanismo/hash/DigestInfo, selección/SSC y operaciones inesperadas además
   del caso exitoso.
4. Reset/remoción/logout/errorMAC/concurrencia/varias sesiones y cierre. Probar
   que falta de capacidad/falloCA detiene antes de enviar PIN y que un fallo
   tras VERIFY no provoca otro intento ni una firma con estado obsoleto.
5. Compilar el módulo y correr las regresiones JCOP3/IAS. Solo entonces pedir
   al usuario una firma PKCS#11 real de texto técnico y verificar offline.
6. Con esa evidencia, probar firmadorCLI y GUI, firmas consecutivas y
   retirada/reinserción. Los comandos de hardware/PIN/GUI los ejecuta el usuario.

No repetir ahora el diagnóstico de firma exitosa, lector o canal. No hay un
nuevo comando de tarjeta preparado en esta revisión. El siguiente trabajo
es local y con transporte simulado; no requiere otro PIN.

## Primera base implementada: perfil, ownership y estado

Cambios sobre principal `4a06cfc` y OpenSC `3dfef6704`, sin hardware. El header
privado `src/libopensc/iasecc-chipdoc.h` separa datos del perfil observado
del control de sesión. No añade ATR a la tabla de binding, escanea EF.DIR,
instala SM, declara algoritmos ni activa las rutas de PIN/firma: esas piezas
se conectarán después. El código está compilado dentro de OpenSC y su header
figura en noinst_HEADERS para su distribución.

ATR/AID exactos solo producen un **candidato**. eOI puede compartir ambos:
preparar una sesión exige además metadata pública validada RSA nativa/sign,
ref91/2048, PIN1, D004/137/selector18. La preparación comprueba también
T=1, ATR de tarjeta/lector, EXT ya declarado y límites de ambos>=139/291.
No modifica flags o tamaños para hacer pasar la comprobación; el futuro
initializer deberá declarar las capacidades justificadas por el perfil.
Metadata coincidente no autentica identidad de tarjeta ni país emisor.

La sesión almacena generación, fase, referencia/tamaño de MSE y ownership
de un contexto opaco. No contiene credencial/hash/digest/firma ni contador
de intentos persistente. El callback de contexto deberá limpiar/liberar
claves/SSC cuando exista el adaptador crypto. Invalidación despega primero
ownership y borra control, luego ejecuta cleanup una sola vez; admite cleanup
recursivo y no permite overflow de generación ni revivir una sesión antigua.
Reemplazar un contexto activo exige invalidación explícita, sin reintento.

Las transiciones representan el orden probado canal/MF/query63C7/login/AID/
MSE/firma. Query63C7 es una precondición de este candidato, no una inferencia
del máximo configurado. Consumir la respuesta de firma borra la autorización
MSE y exige prepararlo de nuevo. **Estas funciones no verifican MAC/CA ni
realizan crypto**: attach/record solo serán llamadas por el futuro adaptador
después de validar MAC, padding, contenido y política SW. Sus errores de
control tampoco sustituyen el mapping de SW autenticado a errores PKCS#11.

`iasecc_private_data` conserva un binding separado del estado efímero para
que perder la sesión no permita volver a una ruta IAS en claro. Ambos son
NONE al inicializar actualmente; ningún caller de producción prepara aún
la sesión. `iasecc_logout` y `finish` limpian ownership. Para el perfil activo
también limpian cache/security_env/key_size/op_method/op_ref. El hook nuevo
`iasecc_card_reader_lock_obtained` hace esa invalidación al recibir was_reset;
lock ordinario y perfiles legacy conservan sus datos. Incluso con la sesión
ya invalidada, un binding activo obliga a limpiar MSE/cache en el próximo reset.
`sc_reset` explícito no llama ese hook en la base local: el adaptador deberá
invalidar antes de invocarlo. No se ha probado remoción ni reset físicos.

`make test/iasecc-chipdoc-profile` pasó nueve grupos con metadata/objetos
sintéticos y código real del driver/SDO en el fixture. Cubre mutaciones de
ATR/AID/metadata, límites/capabilities intactos, hash/ref/tamaño no soportados,
30órdenes incorrectos, errores/generación/ownership/overflow/cleanuprecursivo,
dos estados independientes y callbacks reales de logout/finish/reset con
limpieza MSE/cache. El transporte está prohibido y no se crea contexto PCSC.
Registro del hook comprobado en código/compilación, sin prueba de vtable
runtime o de la cadena completa sc_lock/driver/PKCS#11. Un fallo inicial de
enlace del fixture se resolvió incluyendo el SDO real cuyos símbolos eran
privados, sin stubs permisivos ni cambio al transporte.

ASan/UBSan pasó los nueve grupos con detect_leaks0; no prueba de fugas ni
instrumentación de OpenSC.so/provider precompilados. OpenSC/móduloPKCS11
compilados y suiteIAS/JCOP3 pasó tras los cambios finales, incluyendo este
nuevo grupo. No cambian helper/codec/flake/lock/observador ni artefactos reales.
El siguiente bloque de integración sigue descrito abajo; mantener pruebas
offline antes de otra firma física.

## Adaptador interno de canal y MF por dispatcher normal

Sobre principal `ea2b6d0` y OpenSC `0b511757c`, módulo privado
`iasecc-chipdoc-sm.c/.h`, compilado en libOpenSC. No se instala en una
vtable pública, no añade ATR a binding ni pide PIN. Su apertura exige perfil
y sesión preparada, metadata ya validada, capacidades/límites declarados,
T1, debug0, SM_MODE_NONE y lock exterior del llamador. Este debe haber
seleccionado/validado ADF/D004 y leído137 en la misma conexión/transacción.
El módulo no sustituye ese bootstrap.

Abre con los tres comandos MSE/GA/READ protegido137 (exterior256) de la firma
exitosa. Usa **sc_transmit_apdu normal**, con validación APDU y lock recursivo,
NO_RETRY_WL/NO_GET_RESP/NO_SM; ninguna llamada directa al lector. Solo
conserva el contexto en secure_alloc tras verificar MAC/SW/contenido idéntico
y generación, en SSC2. Un error descarta el contexto/MSE/cache sin borrar el
binding. El contexto real tiene cleanup que limpia/libera claves/SSC.

Selección MF tipada: corto DO97/1/Le256 de la firma o extendido explícito
DO97/2/Le291 ya observado en el diagnóstico independiente. No es una lista
de fallback. El dispatcher normal valida EXT con la capacidad existente;
no se añade el flag ni se alteran tamaños para pasar. Tras SSC3/4 exige MAC,
inner9000 y control MF3F00/DF canónico antes de marcar faseMF. Invalida el
cache ADF/D004 antiguo sin descartar el canal ni inventar un sc_file completo.
Después de IO comprueba ownership/generación antes de tocar contexto: el
hook de reset puede haberlo liberado mientras se transmitía.

La API comunica commands/transport/authenticated/sw/reset_required, sin
buffers, claves o MAC. Duplicados/encoding local inválido no envían otra APDU
ni reemplazan el contexto; si existe canal, sigue requiriendo cierre remoto.
**Reset remoto y outerunlock son responsabilidad del llamador**, también
tras setup fallido. lock_count>=1 es una precondición, no prueba histórica de
que jamás se hizo unlock/relock. El futuro driver debe conservar la
transacción entre llamadas y cerrar/resetear antes de liberarla; el hook
ordinario actual no demuestra esa continuidad. Query/VERIFY/MSE/PSO y el
wrapper SM genérico siguen pendientes, sin activar operaciones por defecto.

Las cinco implementaciones KDF/primitives/agreement/messages/confirmation
se trasladan al submódulo y se convierten en staticinline para uso de headers.
tools/ conserva wrappers de dos líneas que incluyen esa implementación.
El parser MF sale de transport.h a un sexto header controls compartido.
Sin duplicar algoritmos y sin cambiar sus vectores/semántica. Makefile.am
distribuye headers/compila módulo; Makefile.mak incluye obj, sin buildWindows.
Las funciones de adaptador y limpieza son internas, sin símbolos nm-D nuevos.

El backend experimental compila con OpenSSL3. En OpenSSL antiguo/LibreSSL/
sinOpenSSL retorna NOT_SUPPORTED y no toca estado. Se prueba esa rama mediante
una macro de fixture, **no con builds reales de esas bibliotecas**. Todavía
falta adaptar sc-ossl-compat y usar libctx/providers de OpenSC: las primitivas
conservan la selección global de provider de los helpers. Resolver esto antes
de activar el perfil; no presentar el parche como portable/upstream listo.

`make test/iasecc-chipdoc-channel` pasó seis grupos (cinco nativos y uno de
backend no disponible). Usa dispatcher real libOpenSC, serializer real
apdu.c, locks y lector/peer sintéticos; AES/CMAC de respuesta independientes
del codec. CA/MF corto/extendido, SSC2/4/cache, límites/perfil/lock/debug/SM,
errores/setup61/6C/payload/tamaños/MAC64bits/padding/IV/control/innerSW/replay,
reset durante IO y cleanup sin uso de contexto liberado. Sin PIN ni
GET/reintento/fallback. Stub retorna reportes0/NOT_SUPPORTED sin cambiar
tarjeta/sesión. ASan/UBSan pasó6 detect_leaks0; no prueba de fugas ni
instrumentación de dispatcher.so/provider precompilados; fixture del driver,
adaptador, core y serializer sí instrumentados.

OpenSC/móduloPKCS11 y cuatro helpers reales compilaron, sin ejecutarlos.
Pasaron123 grupos previos del core y helpers tras el traslado (47+76) y la
suiteIAS/JCOP3 con el nuevo grupo. No se ejecutó hardware/PIN/GUI/Idopte ni
se alteraron flake/lock/observador/artefactos. La firma real JnUUiy9x sigue
siendo la evidencia vigente; no repetirla por este traslado.
