# Integrar el perfil JCOP4 observado en OpenSC

Estado al 14 de octubre de 2026,10:13 Costa Rica. Base local del proyecto
`0f51c4c`, submódulo OpenSC `3dfef6704`. Este documento define el siguiente
cambio. **Perfil/estado/limpieza y adaptador interno CA/MF están implementados;
metadata/bootstrap/providers/reserva/cierre/consulta PIN privados también
implementados; activación optativa y VERIFY/firma aún no están conectados
al driver público.**

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
| Detección e inicialización | `iasecc_match_card`, `iasecc_init`, `iasecc_init_amos_or_sagem` | Validación nativa PKCS15 y bootstrap privados implementados; wiring a detección/init aún pendiente. No heredar defaultsIAS completos. La tabla de binding actual no tiene una entrada exacta para este ATR. |
| Datos de perfil | `iasecc_private_data`, `iasecc-chipdoc.h`, `iasecc_is_chipdoc_signing_key` | Estado/binding independientes por tarjeta implementados, inicialmente inactivos; el selector legacy sigue reconociendo aplicación/ref3 JCOP3. |
| Canal | `iasecc-chipdoc-sm`, headers privados `jcop4-channel-*` | Implementación compartida y APIs internas CA/MF por dispatcher normal completadas. Providers/libctx OpenSSL3 y bootstrap privados completados. Quedan otros backends y wiring al driver público. |
| PIN | `iasecc_pin_cmd`, `iasecc_pin_verify`, `iasecc_pin_get_info` | VERIFY/consulta protegidos, un intento, sin copia persistente ni cache de PIN; publicar estado solo autenticado y limpiar login tras reset/error. |
| MSE | `iasecc_set_security_env` | Mapear operación/hash/llave de PKCS#15 al formato probado, manteniendo selección AID bajo SM y orden. |
| Firma | `iasecc_compute_signature`, `iasecc_compute_signature_dst`, `iasecc_compute_signature_at` | Para este perfil usar la operación PSO probada; no INTERNAL AUTHENTICATE o hashing convencional por semejanza. |
| Limpieza | `iasecc_logout`, `iasecc_finish`, `card_reader_lock_obtained` | Limpieza de estado/ownership/MSE/cache y hook de reset implementados. Contexto y reserva/reset/cierre privados implementados con lifecycle; quedan remoción real, serialización y wiring al llamador público. |
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

## Contexto criptográfico de OpenSC y aislamiento de providers

Sobre principal `35823ef` y OpenSC `1c3d01ba2`, el adaptador ahora requiere
`ctx->ossl3ctx->libctx` no nulo, usando el tipo de `sc-ossl-compat.h`.
OpenSC crea ese contexto y carga su configuración/providers en `ctx.c`;
el canal lo toma prestado, sin cargar providers ni alterar propiedades.
Las variantes privadas `_ex` pasan el mismo `OSSL_LIB_CTX` a generación
aleatoria EC/P521, controles/importación/ECDH, SHA-1, AES y CMAC, incluyendo
el cálculo del IV y la verificación del MAC antes de descifrar. La query de
propiedades es NULL, como en las funciones de compatibilidad OpenSC, para
conservar la política predeterminada del contexto. No hay fallback al global.
Las APIs anteriores de los helpers son wrappers con NULL explícito; siguen
usando el mismo core y mantienen sus vectores y el protocolo observado.

El propietario de sc_context debe conservar libctx/providers hasta cerrar
su tarjeta/canal. El adaptador guarda identidad de sc_context/ossl3ctx/libctx
junto a tarjeta/lector/generación y la comprueba antes/después del transporte.
Una sustitución invalida sesión/MSE/cache sin publicar SW ni continuar con
un contexto distinto; primero comprueba ownership/generación por el posible
reset que libere el canal durante IO. Si cambia la política dentro del mismo
libctx, cada fetch vuelve a respetarla: un algoritmo denegado falla y se
limpia la sesión. No detecta cualquier cambio arbitrario de configuración ni
asume que providers pueden liberarse mientras el contexto sigue en uso.

Contexto ausente o EC denegado al preparar no envía APDU. Si el cambio sucede
tras MSE, el GA ya preparado puede transmitirse antes del próximo fetch;
la restricción impide construir el READ protegido y exige cierre remoto.
Cambios posteriores impiden construir MF o autenticar su respuesta; no se
publican datos/SW ni se reintenta con el global. Se mantienen las obligaciones
del llamador: transacción exterior continua y reset/cierre antes de unlock,
incluso si solo queda estado remoto tras un fallo criptográfico.

`make test/iasecc-chipdoc-channel` pasó **nueve grupos**: ocho nativos y uno
stub. Los tres nuevos cubren aislamiento con providers/fetches reales:

- Contexto privado funcional mientras el global niega algoritmos, incluyendo
  ECDH/KDF/AES/CMAC y respuesta independiente descifrada/verificada por codec.
- Null provider, propiedades restrictivas y contexto OpenSC ausente: sin
  fallback/IO de setup ni claves parciales, con borrado de salidas de error.
- Sustitución de ctx/owner/libctx y restricción antes/durante IO: pérdida de
  sesión y ausencia de SW autenticado, sin siguiente intercambio protegido.

ASan/UBSan9 pasó con detect_leaks=0; no prueba de fugas ni instrumentación de
OpenSC.so/OpenSSL precompilados. Pasaron los **123 grupos** previos (core47,
helpers76) y la suite IAS/JCOP3. OpenSC/móduloPKCS11 y cuatro helpers reales
compilaron sin ejecutarse. No PCSC/hardware/PIN/GUI/Idopte del agente; no
nuevo informe ni repetición de firma real, ni cambios a flake/lock/observador.

Este cambio integra providers con OpenSSL3; no implementa OpenSSL1/LibreSSL.
El stub sin backend se comprueba por macro, sin builds reales de esos otros
entornos. No acredita FIPS, otros providers funcionales o Windows/macOS.
Binding/bootstrap/PIN/MSE/PSO en el driver público siguen pendientes.

Referencias primarias: [contextos EVP de OpenSSL](https://docs.openssl.org/3.5/man3/EVP_PKEY_CTX_new/)
y [CMAC y cipher subyacente](https://docs.openssl.org/3.5/man7/EVP_MAC-CMAC/).
Estas APIs permiten seleccionar libctx y sus algoritmos; las pruebas locales
acreditan el aislamiento de esta implementación, no una certificación.

## Bootstrap desde objetos PKCS15 y parámetros D004

Sobre principal `d0ee1da` y OpenSC `8e107d776`, nueva API interna
`iasecc_chipdoc_channel_bootstrap(p15,key,report)`. Se invoca explícitamente
dentro del driver IAS; no se instala en match/init/vtable ni activa tarjetas
por ATR. Usa objetos PKCS15 ya decodificados para elegir el perfil funcional,
sin descubrirlos o leer certificados/credenciales desde esta API.

El header privado `iasecc-chipdoc-pkcs15.h` exige llave RSA nativa2048/ref91,
uso SIGN o NONREPUDIATION y pertenencia a la lista del mismo p15card. Su
identificador de autenticación debe corresponder a un único PIN global/ref1,
sin SO/PUK. Rechaza emulación, objetos de sesión, asociación ambigua, listas
cíclicas o más de512 objetos (guard local, no capacidad de tarjeta). Requiere
AID conocido anunciado una sola vez, y DDO/pathAID vacíos o iguales. No usa
labels, certificado, contenido/cache de PIN, tries/logged_in o encoding para
reconocer el perfil. Emisor/identidad no quedan autenticados por estos datos.

El llamador debe invalidar estado local y hacer un reset exitoso bajo su
lock exterior antes de entrar, y mantener la misma transacción hasta el
reset/cierre final. La API no adquiere ese lock ni ejecuta resets y no puede
probar esa historia a partir de lock_count. Exige contexto privado OpenSSL3,
debug0, SM_MODE_NONE, T1/ATR y EXT/límites139/291 ya declarados; no los cambia
para aprobar. Solo acepta estado inactivo, y preserva un propietario activo
ante llamada duplicada o error local de argumentos/configuración.

Antes del primer comando reserva BOOTSTRAPPING/generación. Así el hook de
reset puede invalidar la operación cuando todavía no existe binding
permanente. La secuencia plain medida usa el dispatcher normal:

1. SELECT AID conocido, CASE3 corto/P1=4/P2=0C/sinLe: requiere9000 sin payload.
2. SELECT D004, CASE4 corto/P1=2/P2=0/Le256: exige control FCI/FCP completo,
   identidad D004, transparente y137bytes mediante el matcher existente.
3. READ0/Le137: exige9000/exactamente137, selector18 decimal y punto P521
   canónico válido, coordenadas desde5. Los demás bytes de cabecera no se
   interpretan; atributos propietarios del control no acreditan permisos.

Cada comando tiene NO_RETRY_WL/NO_GET_RESP/NO_SM, sin continuación o fallback.
Un fallo detiene en su etapa y exige cierre remoto después de cualquier IO,
aunque no haya MSE/GA/PIN. No conserva ni registra buffers/punto/control.
El matcher D004 se trasladó a `jcop4-dh-control.h` privado y tools conserva
un wrapper; reglas ASN1 y permisos de los diagnósticos anteriores intactos.

La integración encontró una discrepancia local del perfil: selector0x18
significa24, pero el helper/parsers/acuerdo y el punto real validado usaban
**18 decimal (0x12)**. Se corrigió el perfil y se añadió rechazo explícito de
0x18 a los tests. Los antiguos fixtures construían metadatos con la misma
constante equivocada; la nueva prueba cruza esos metadatos con la lectura
sintética y la confirmación existente. No se cambió el protocolo ni se volvió
a consultar tarjeta para resolverlo.

Solo después de validar PKCS15 y D004 establece el binding funcional
persistente CR, limpia la reserva/cache/MSE y prepara el estado. Invoca
channel_open existente para MSE/GA/READ protegido, exterior256. Son6APDU en
total y conserva canal/SSC2 exclusivamente con MAC/SW/contenido iguales.
Puede continuar con MF protegido corto ya medido, SSC4 y7APDU. Fallar CA
limpia contexto pero mantiene binding; fallar D004 cuando ya había binding
tampoco lo borra. No convierte un candidato nuevo fallido en perfil válido.
La futura integración pública debe impedir que ese binding vuelva a las
operaciones IAS en claro: ese wiring y esas guardas aún están pendientes.

Cinco grupos nuevos de `make test/iasecc-chipdoc-bootstrap` pasaron: fixtures
DER generados y decodificados con las funciones PKCS15 reales; reader/locks/
peer de software, serializer/dispatcher reales y respuesta AES/CMAC del
peer independiente. Cubre32 variantes antes de IO, control/READ/selector/
punto/longitud/SW61/6C/transporte, reset/contexto en las seis fases, errores
CA/MAC, binding anterior, argumentos/duplicados y contenido/cache/estado PIN
ignorados. No transmite VERIFY, consulta PIN, firma o reset remoto.

Pasaron perfil9/adaptador9/bootstrap5, **23 grupos nativos** y ASan/UBSan23
con detect_leaks=0. No prueba de fugas ni instrumentación de OpenSC.so,
OpenSSL o decoders ASN1 precompilados. También los76 grupos de helpers tras
compartir control D004 y la suite IAS/JCOP3 completa. Core crypto47 anterior
sigue vigente, sin cambios/no repetido. Compilados OpenSC/móduloPKCS11 y
cinco helpers reales, sin ejecutarlos, ni nuevos símbolos nm-D. Makefile.am
distribuye los dos headers nuevos y el target se incluye en test/iasecc.

Sin PCSC/lector/tarjeta/PIN real/GUI/Idopte del agente ni otro informe. Firma
JnUUiy9x sigue vigente. Siguiente: reserva/reset/cierre, query/VERIFY y luego
MSE/PSO con APIs y transporte sintético, wiring público optativo y PKCS11,
antes de pedir otra firma física. OpenSSL3 solamente; otros backends siguen
con stub, sin builds reales/FIPS/Windows/macOS. RSA3072 aún no acreditado.

## Reserva del lector, cierre y consulta de PIN autenticada

Sobre principal `5e59f0a` y OpenSC `0710faaf1`, APIs privadas
`iasecc_chipdoc_transaction_begin`, `transaction_close` y
`iasecc_chipdoc_channel_query_pin`. No se conectan todavía a init/match,
PIN_CMD público o PKCS11, no añaden mecanismos ni interfaz para credenciales.

Begin valida objetos/contexto/perfil/límites y callbacks de lock/unlock/reset
y hook. Añade un único sc_lock, incluso si el llamador ya tiene otros locks.
Invalida selección/MSE/crypto local antes del único reset inicial; bootstrap
6APDU y MF corto1APDU. Éxito conserva una reserva propia en la misma tarjeta/
lector/contexto, con canal SSC4/faseMF. No modifica límites o capabilities.
OpenSC conserva sus reintentos normales de lock ante reset; el controlador
no reintenta startup ni APDU. Los callers deben serializar las APIs de una
tarjeta: esta reserva entre llamadas no es un mutex nuevo o prueba de hilos.

El header privado `iasecc-chipdoc-transaction.h` implementa close compartido
por APIs y lifecycle. Despega propiedad y borra claves/SSC/MSE/cache antes
del reset final bajo el lock original, luego libera solo su sc_unlock.
Respeta la profundidad restante del llamador, y no repite efectos al cerrar
otra vez. Reporta resultados/flags/etapas fijos, sin buffers, claves o datos
personales. Close inactivo sin estado pendiente es no-op del controlador,
no certifica cierre remoto de APIs raw que usen su propio lock/reset.

Un reset inicial fallido provoca unlock sin repetir reset; fallo de cierre
o unlock deja recovery_required. Esa instancia no puede comenzar de nuevo
hasta que se desconecte y se cree otra. Si sc_lock falla después de aumentar
el contador (p.ej. falla liberar mutex), no supone que posee una transacción
usable: marca pérdida/recuperación y no ejecuta reset/unlock desconocidos.
El reporte conserva la incertidumbre y exige desconectar, sin reintento.

El hook real card_reader_lock_obtained se invoca al adquirir un lock físico,
no en llamadas anidadas. Si aparece mientras la reserva propia estaba
activa, la reserva original se perdió: invalida canal y marca lost/owned0/
recovery aunque was_reset sea0. Close nunca resetea ni libera ese nuevo lock
ajeno. No implica detectar instantáneamente un unlock externo sin otra
operación: la próxima API comprueba lock_count y descarta contexto; una
reacquisición física posterior queda cubierta por el hook. Perfiles legacy
y contextos raw conservan su comportamiento ordinario previo.

Logout/finish cierran reserva activa. Logout con binding reconocido ya
inactivo evita reselección raíz IAS en claro. Finish libera drv_data y
retorna error de cierre. En esta base **sc_disconnect_card registra el error
de finish pero termina retornando0**; por eso el consumidor debe comprobar
transaction_close/report antes de desconectar. No inferir reset/cierre
completo solo de disconnect0 ni presentar esto como reporte de bug upstream.

Query exige reserva propia, faseMF y contexto/lector/generación vigentes.
Solo manda EMPTY VERIFY global1 protegido: CLA0C/INS20/P100/P201, CASE4 corto,
Lc10/DO8E MAC8 y Le exterior256; frame16. **Sin credencial, DO87 o DO97**.
Usa sc_transmit_apdu normal y NO_RETRY_WL/NO_GET_RESP/NO_SM. Verifica MAC
antes de datos/SW; admite exterior9000 o63Cx que coincida exactamente con
DO99 autenticado, y exige payload vacío. Cambios de contexto/ownership
posteriores a transporte se comprueban antes de volver a usar el crypto.

La primera política medida avanza a PIN_AVAILABLE/SSC6 solo con63C7.
Devuelve otros contadores autenticados0..15 pero descarta el canal y falla:
0 es AUTH_METHOD_BLOCKED; otros no7 SECURITY_STATUS. No infiere máximo7 ni
reinicio del contador y no transforma9000 en login o permiso de saltar PIN.
Respuestas inválidas no publican SW y dejan tries_left=-1. Un fallo real
limpia el canal, manteniendo obligación de close; argumentos/duplicados
locales preservan el propietario y no envían otra consulta o usan cache.
Esta etapa solo consulta estado; VERIFY con credencial se añade en la etapa
siguiente. Ningún test lee PIN real.

`make test/iasecc-chipdoc-transaction` pasó **siete grupos**: sc_lock/reset/
unlock y dispatcher reales de libOpenSC, con lector/locks del backend/peer
y fallo de mutex simulados. Incluye anidamiento, rollback en cada etapa,
reset inicial/final/unlock/mutex fallidos, pérdida y reacquisición física
sin reset, todos64bitsMAC, replay/forma/SW/outer/payload y contadores0..15,
provider cambiado, duplicados y lifecycle real logout/finish. Respuestas y
MAC del peer se construyen fuera del codec. Sin PCSC/contexto real/tarjeta.

ASan/UBSan7 pasó con detect_leaks=0, sin prueba de fugas o instrumentación de
OpenSC.so/OpenSSL/lock/reset/unlock/ASN1 precompilados. Código local del
driver/controlador/codec/serializer del fixture sí instrumentado. Pasaron
los23 grupos previos perfil/adaptador/bootstrap y la suite IAS/JCOP3 completa
con el nuevo target (30grupos nativos). Core47/helpers76 no cambiaron y
sus suites previas siguen vigentes; no repetirlas por este cambio. OpenSC/
móduloPKCS11 compilados, sin nuevos símbolos nm-D, sin reconstruir helpers
reales intactos. Makefile.am distribuye el header privado nuevo.

Sin PCSC/lector/tarjeta/PIN real/GUI/Idopte del agente, ni captura o informe.
Los efectos reales de reset/query sobre intentos no se acreditan mediante
esta prueba software. Tampoco concurrencia de hilos/procesos, bloqueo temporal
acotado de PCSC o remoción física. Firma JnUUiy9x vigente; no repetir ahora.
La siguiente etapa añade VERIFY desde argumentos de API; AID/MSE/PSO,
activación pública optativa y pruebas PKCS15/PKCS11 siguen pendientes.

### VERIFY privado desde argumentos OpenSC

Principal `c58d485`, OpenSC `1f36a170d`, limpios al comenzar. Se añade
`iasecc_chipdoc_channel_verify_pin(card,data,tries,report)`; API privada,
sin cambiar PIN_CMD/match/init/vtable públicos. La metadata decodificada
ahora copia exclusivamente formato/flags/límites públicos del PIN asociado.
Transaction_begin exige global1/UTF8/min6/max14/flags0x11 antes de lock/reset
y conserva esa política, sin leer credencial/cache/logged_in/intentos.

El argumento es `const sc_pin_cmd_data`: solo VERIFY, flags0, SC_AC_CHV,
referencia1, encodingASCII0, sin PUK/customAPDU/segundoPIN. Rechaza PIN-pad,
cambio/desbloqueo/query/padding antes de transporte. Los límites opcionales
del llamador no pueden cambiar6/14. `pad_length` suministrado por PKCS15 se
ignora cuando no se solicita padding. UTF8 admite por ahora exclusivamente
ASCII imprimible0x20..0x7E de6..14 bytes, sin conversión/trim/hash/truncamiento.
No leer prompt ni cache/counter/login del argumento. **SC_AC_AUT8 no se
reinterpreta como PIN**: el wiring futuro debe justificar el método de los
objetos nativos; esta API privada no resuelve por sí sola esa integración.

Exige reserva propia, fasePIN_AVAILABLE después de63C7, mismo card/reader/
generation/provider y SM_IDLE/SSC6 antes de leer los bytes. Toma prestado el
buffer del llamador y no conserva copia. Seal limpia sus temporales; caller
debe limpiar su entrada. Un único VERIFY protegido: global1/CLA0C,
DO87 cifrado16+DO8E/MAC8/Lc29, CASE4 corto/frame35/Le256, sin DO97 ni padding
de credencial (sí padding criptográfico método2). SSC7 comando/8 respuesta.
Dispatcher normal con NO_RETRY_WL/NO_GET_RESP/NO_SM, sin 6C/61/repetición.

MAC antes de publicar SW, exterior9000 o63Cx idéntico al DO99 autenticado,
plaintext vacío obligatorio. Solo9000 avanza LOGGED_IN; no se infiere contador
reiniciado/max7 y tries permanece-1.63Cx autenticado publica0..15:0 blocked,
otros PIN_CODE_INCORRECT;6983 blocked/tries-1,6982 security status, otros
CARD_CMD_FAILED. MAC/forma/contexto/IO/criptografía o SW de fallo invalida
canal sin reenviar el PIN. Reserva permanece para close; pérdida de reserva
requiere recuperación sin reset/unlock ajenos. Argumentos locales inválidos
y duplicados no transmiten y preservan dueño; repeat tras login no reautentica.

`make test/iasecc-chipdoc-verify` usa lector/peer y PIN ficticios, dispatcher,
serializer y locks reales. Se comprueban6..14 bytes/frame35 con descifrado y
CMAC independientes, argumento/entrada intactos, política previa a lock,
22 variantes locales, todos64bitsMAC/replay/forma/61/6C/transporte y counters
sintéticos0..15, reset/reattach/remoción/provider/cierre fallido. Nunca intenta
PIN real ni afirma efectos sobre su contador. No hardware/Idopte/GUI/nuevas
capturas. Firma real JnUUiy9x sigue vigente. Siguen selecciónAID/MSE/PSO,
wiring público/PKCS15/PKCS11 y validación optativa con tarjeta; RSA3072 no
acreditada.

Pasaron **seis grupos nuevos**, ASan/UBSan6 con detect_leaks0, treinta grupos
previos y la suite IAS/JCOP3 completa (691líneasPASS, no691casos distintos).
Sin fugas verificadas o instrumentación de libOpenSC/provider/locks/ASN1
precompilados. Stub del backend no soportado probado por macro con VERIFY
sinIO ni cambios de estado; no builds reales de esos backends. OpenSC y
módulo PKCS11 compilados, sin exports nuevos. Core47/helpers76 y helpers
reales sin cambios, resultados anteriores vigentes/no repetidos.

### AID, MSE y PSO privados desde argumentos OpenSC

Principal `f15feb4`, OpenSC `6d149ac38`, limpios al comenzar. Se añaden
`channel_set_security_env(card,const sc_security_env*,se_num,report)` y
`channel_compute_signature(card,in,in_len,out,out_len,report)`. APIs privadas,
sin PIN_CMD/set_security_env/compute_signature públicos, match/init,
algoritmos anunciados ni nuevos exports. Ningún comando de tarjeta del agente.

Set_security_env acepta solo se_num0, flagsALG_PRESENT|KEY_REF_PRESENT,
SIGN/RSA/ref91 de un byte/key_size_bits2048 y PKCS1_TYPE01|HASH_SHA256.
Sin FILE_REF/ALG_REF ni paths/AID/target/params opcionales; referencias de
algoritmos anunciados se ignoran en esta API, no eligen algoritmo de tarjeta.
Campos, digest, PIN o punteros del argumento no se guardan en drv_data.
Operaciones/hash/llave/tamaño/flags/paths/params no soportados se rechazan
antes de seleccionar aplicación. Este contrato acotado debe conciliarse
con los argumentos reales de PKCS15 antes de wiring; no eliminar campos
del llamador a ciegas ni anunciar RSA3072, rawRSA, PSS u otros hashes.

Exige reserva propia/LOGGED_IN/SM_IDLE/SSC8/provider/generación. Selecciona
el AID del perfil confirmado por bootstrap una vez bajo SM: 00A4040C,
sin Le interior, DO87+8E/body29/CASE4 corto/frame35/Le exterior256, SSC9/10.
Respuesta autenticada9000 y payload vacío permiten APPLICATION. Después
único MSE002281B6/data91022891, mismo frame35/Le256/SSC11/12, respuesta
vacía9000; fija referencia91/tamaño2048 de MSE. No DOCP/private components,
certificados, lectura, selección IAS en claro, query o VERIFY adicionales.
Cada etapa verifica owner/generation ANTES de volver a usar crypto tras IO.

Compute_signature exige MSE vigente/ref91/2048/SSC12, input exactamente32
bytes y output con capacidad al menos256. **Input ya es digestSHA256**, sin
hash, DigestInfo, conversión, padding de credencial ni truncamiento local.
PSO002A9E9A, DO87/cipher48+DO97020100+DO8E/body65, CASE4_EXT/frame74,
Le exterior291, SSC13/14. Usa capacidades T1/EXT/envío139/recepción291 ya
exigidas por el perfil, sin alterarlas. NO_RETRY_WL/NO_GET_RESP/NO_SM,
sin chaining, 61/6C/GET RESPONSE, fallback de Le/CLA u otro PSO.

MAC/padding/outer9000 y SW interno9000 + exactamente256 bytes obligatorios.
Conserva solo la política observada para status-only99/8E/6A82 (16bytes
incluidoSW): MAC/SW coincidente son un **error**, sin output/continuación.
Otros fallos autenticados permanecen errores; forma/MAC/IO/crypto/contexto
inválidos no publican SW. FaseMSE/clave se consume y todo error real invalida.
El driver valida procedencia/forma de respuesta protegida; no lee certificado
ni verifica RSA en producción. La verificación matemática de RSA corresponde
al test/consumidor que tenga la llave pública, como ya hizo el helper real.

Una operación PSO aceptada siempre intenta transaction_close, incluso ante
fallo de cifrado previo a envío, transporte, respuesta o resultado. Descarta
claves/MSE/cache antes de reset/unlock y respeta locks anidados del llamador.
Solo devuelve256/copia el resultado después de cierre completo; output
permanece intacto ante fallo, incluido reset/unlock final. Reporte contiene
canal y cierre numéricos, no digest/firma/PIN/claves. No contadores máximos
o reinicio inferidos ni cache/credencial retenida. Pérdida de reserva impide
reset/unlock de otra reserva y marca recuperación hasta recrear instancia.
Errores locales de formato/fase previos a aceptar operación no envían PSO
y dejan cierre a cargo del llamador; ready puede invalidar un contexto viejo.
No uso de nuevo MSE/PSO tras firma; siguiente firma requiere nueva transacción.

`make test/iasecc-chipdoc-sign` pasó **seis grupos**: peer lector/RSA/PIN
sintéticos con dispatcher/serializer/locks reales y construcción CBC/CMAC
independiente; frames35/35/74/SSC9..14, firmaRSA positiva y negativo de un
bit, salida ausente durante callbacks de reset/unlock, locks anidados,
20 ambientes+8 inputs inválidos,64bitsMAC/forma/replay/padding/SW/61/6C/
transport/reset/reattach/remoción/provider en cada etapa, cierre fallido y
cuarentena. ASan/UBSan6 detect_leaks0 pasó: sin fugas verificadas ni
instrumentación de libOpenSC/provider/locks/ASN1 precompilados. Stub de
backend no soportado incluye ambas APIs sinIO/output/estado; no builds
reales OpenSSL1/LibreSSL/noSSL/FIPS/plataformas o concurrencia física.

Siguen activación pública optativa, capacidades justificadas, método PIN
nativo y paths/argumentos reales de PKCS15, anuncio/transformación de
mecanismos (incluido DigestInfo de CKM_RSA_PKCS) y pruebas PKCS15/PKCS11.
`pkcs15-sec.c:use_key` tiene revalidación de PIN cacheado ante security status;
es una ruta existente que el wiring debe controlar y probar con este perfil
para conservar el límite de un intento, no una autorización para repetir.
No hardware/PIN real/GUI/Idopte/capturas nuevas; firma JnUUiy9x sigue vigente.
No pedir otra firma real hasta probar ese wiring; RSA3072 sin acreditar.

Pasaron también36 grupos previos y la suite IAS/JCOP3 completa con el nuevo
target:697líneasPASS (no697casos distintos). OpenSC/móduloPKCS11 compilados;
sin exports nuevos. Core47/helpers76/helpers de hardware, flake/lock/
observador/capturas sin cambios; resultados anteriores vigentes/no repetidos.
