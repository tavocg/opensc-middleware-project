# Integrar el perfil JCOP4 observado en OpenSC

Estado al 14 de octubre de 2026. Base de esta etapa: proyecto
`90ea080`, submódulo OpenSC `52057cc6a`. Este documento describe la integración.
**Perfil/estado/limpieza, canal/bootstrap/providers y operaciones
PIN/firma privados están implementados. Frontend PKCS15 y routing privado
instalado por tarjeta pasaron con callbacks de producción y lector simulado.
Configuración/init y activación automática desde el binding nativo PKCS15
están implementados para una opción explícita, con firma verificada en
transporte simulado. El módulo PKCS11 del build ya pasó nueve grupos de
mecanismos, objetos, sesiones y firmas con contexto/lector/PIN sintéticos.
El diagnóstico público y una firma SHA256_RSA_PKCS ya pasaron en la JCOP4
real. El firmador produjo un PDF CAdES y dos sellos RFC3161 verificados offline;
dos firmas consecutivas también pasaron. Falta reinserción en la misma GUI.**

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
| Detección e inicialización | `iasecc_match_card`, `iasecc_init`, `iasecc-chipdoc-init.h` | Opción explícita y ATR exacto seleccionan candidato; inicialización sin APDU ni defaults IAS completos. Activación tras validar directorios nativos. No hay entrada de ATR automática. |
| Datos de perfil | `iasecc_private_data`, `iasecc-chipdoc.h`, `iasecc_is_chipdoc_signing_key` | Estado/binding independientes por tarjeta implementados, inicialmente inactivos; el selector legacy sigue reconociendo aplicación/ref3 JCOP3. |
| Canal | `iasecc-chipdoc-sm`, headers privados `jcop4-channel-*` | CA/MF, providers/libctx OpenSSL3 y bootstrap conectados por callbacks de instancia. Quedan otros backends y pruebas reales de esta integración; PKCS11 pasó con transporte simulado. |
| PIN | `iasecc_pin_cmd`, `iasecc_pin_verify`, `iasecc_pin_get_info` | VERIFY/consulta protegidos, un intento, sin copia persistente ni cache de PIN; publicar estado solo autenticado y limpiar login tras reset/error. |
| MSE | `iasecc_set_security_env`, routing de instancia | Mapeo de operación/hash/llave y AID protegido implementado, probado desde PKCS11 con tarjeta de software. |
| Firma | `iasecc_compute_signature`, routing de instancia | PSO medido implementado con entrega después de cierre propio; firmas PKCS11 simple/multipart verificadas en software. Falta tarjeta real con módulo. |
| Limpieza | `iasecc_logout`, `iasecc_finish`, `card_reader_lock_obtained` | Limpieza de estado/ownership/MSE/cache y hook de reset implementados. Contexto y reserva/reset/cierre privados implementados con lifecycle; logout/última sesión PKCS11 cierran reserva; quedan remoción real y serialización concurrente. |
| PKCS#15/PKCS#11 | `sc_pkcs15_compute_signature`, algoritmos anunciados | DigestSHA25632, DigestInfo RSA_PKCS y hashing SHA256_RSA_PKCS simple/multipart pasaron con firma verificable de software; queda módulo con tarjeta real. |

La ruta IAS convencional se conserva. El perfil optativo usa el nuevo
establecimientoCA/SSC y routing de instancia; no basta añadir un ATR y
cambiar un opcode.

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

### Contrato de operaciones y frontend PKCS15 nativo

Principal `8dc88cd`, OpenSC `b391a94b3`, limpios al comenzar. Revisión local
de metadata ya disponible `jcop4-pin-policy-pMGzFYNL/report.txt`: PIN
auth_type0/auth_method1/global1/flags17/UTF8/6..14, sin path; llave145/ref91
RSA2048 nativa/uso12, sin path de archivo. No se muestra auth_id ni identidad,
credencial, contenido de certificado o bytes personales. `pkcs15-pin.c`
decode_aodf_entry asigna CHV1 a la alternativa PIN y AUT8 solo a authKey;
el 8 observado en inventarios de autenticación no exige convertirlo a PIN.
PrKDF añade el AID del DDO al path del SDO incluso si no hay archivo. Son
campos distintos de los tipos internos de Idopte; no reinterpretar esos tipos.

El header privado añade `iasecc_chipdoc_pkcs15_operation(p15,key,operation)`:
vista prestada del único PIN asociado, flagsPKCS1_TYPE01|SHA256 y booleano de
selecciónAID. Valida metadata existente, SDO sin archivo y AID ausente o
conocido/typePATH; PIN_CHV global sin path/AID, UTF8/6..14/flags0x11.
Rechaza presencia de contenido cacheado en cualquier objetoPIN, mirando
solo puntero/tamaño, sin leer ni borrar credencial. No modifica metadata,
descubre objetos, lee certificado, envía APDU o instala driver/capacidades.
Un fallo borra toda la salida. Vista válida solo mientras objetos viven.

`iasecc_chipdoc_pkcs15_disable_pin_cache` es opt-in **privado y explícito**:
primero valida todo, después cambia exclusivamente opts.use_pin_cache a0.
Un fallo conserva opciones/objetos. Transaction_begin ahora exige ese
contrato y cache desactivado antes de lock/reset/APDU; el llamador mantiene
esa opción durante toda operación y serializa acceso. No guardar credencial
para recuperarla ni vaciar cache existente en silencio. Bootstrap sin
autenticación conserva su validación anterior; no exige contrato de operación.

Se extrae `channel_select_application(card,report)` como API privada para
el callback select_file de use_key: solo LOGIN/SSC8 -> SELECT AID conocido
protegido, frame35/SSC9/10/vacío9000 ->APPLICATION. MSE acepta LOGIN/SSC8 y
selecciona entonces, o APPLICATION/SSC10 ya confirmado por esa API y envía
solo MSE. Nunca repetir SELECT para el segundo caso. Duplicados/fases/SSC
viejos siguen rechazados; no aceptación de archivos/MF/path arbitrario.
Ninguna nueva variante APDU, CLA/Le o seguimiento autenticado.

`make test/iasecc-chipdoc-pkcs15` pasó **cinco grupos**, con funciones reales
de libOpenSC `sc_pkcs15_verify_pin`, `sc_pkcs15_compute_signature`, native
DER AODF/PrKDF, conversión DigestInfo y use_key/pincache. **Los callbacks del
driver son routing explícito exclusivo del fixture**, que llama APIs privadas
de producción; no prueba configuración/detección/init/bind reales ni PKCS11.
No sc_context_create/PCSC, hardware, PIN real, GUI o Idopte; build actual
sin ENABLE_NOTIFY, fixture rechaza esa opción para no activar notificaciones.

Prueba cuatro rutas positivas: digestSHA25632 o DigestInfoSHA25651,
con o sin AID de SDO. Perfil de algoritmos de fixture anuncia únicamente
RSA2048/PAD_TYPE01|SHA256, sin raw/hashnone/onboard gen. El núcleo PKCS15
quita el DigestInfo de CKM_RSA_PKCS y entrega exactamente32/flagsSHA256,
sin cambiar el input original; caller hace hash antes (no prueba mecanismos
PKCS11 completos). Todos producen frames/SSC medidos y RSA verificable.
La selecciónAID opcional queda protegida y ocurre una sola vez; environment
real tiene flagsALG_PRESENT|KEY_REF_PRESENT, ref91/bits2048, sin FILE_REF.

Cache desactivado impide ambas llamadas a pincache_add durante VERIFY y
también la de su wrapper. Ante security status en SELECT/MSE/PSO, use_key
intenta su ruta existente de revalidate, que devuelve error antes de llamar
al PIN. Se verifica un único VERIFY, sin segunda selección/MSE/PSO y sin
output. PIN incorrecto/bloqueado autenticado se propaga con contador6/0 de
fixtures sintéticos, sin cache/firma/reintento. No prueba ni infiere los
contadores reales. Mal DigestInfo/otrohash/PSS fallan antes de firmar. Método
AUT8/path/formato/padding/cache presente se rechazan antes de mutación/IO.

La reserva propia se añade dentro del lock de VERIFY del frontend y queda
retenida tras su unlock. PSO cierra/reset la reserva gestionada, conserva el
lock exterior de use_key; ese frontend lo libera después de devolver el
callback. **close.complete acredita nuestra reserva/reset, no el unlock
ajeno posterior**, cuya responsabilidad sigue en el llamador. No se cambia
el comportamiento genérico de sc_unlock/use_key ni se presenta un reporte
upstream. Un reset final gestionado fallido impide output y marca recuperación.
No nueva garantía de hilos/concurrencia/timeout/retirada física.

ASan/UBSan5 detect_leaks0 pasó, sin fugas verificadas/instrumentación de
libOpenSC/PKCS15/ASN1/provider/locks precompilados; driver privado/codec/
serializer y routing del fixture sí instrumentados. Quedan install/uninstall
del binding optativo, lifetime de objetos/cache/borrowing, init/capacidades,
GET_INFO y otras operaciones públicas bloqueadas durante canal, mecanismos
PKCS11 y pruebas de sesiones/remoción antes de pedir tarjeta. RSA3072 sin
acreditar; firma real JnUUiy9x sigue vigente, no repetida.

Pasaron42 grupos previos y suiteIAS/JCOP3 completa:702líneasPASS (no702casos
distintos). OpenSC/móduloPKCS11 compilados, sin nuevos exports. Core47,
helpers76 y helpers de hardware/observador/flake/lock/capturas intactos,
resultados anteriores vigentes/no repetidos.

### Instalación explícita del routing y liberación de objetos

Principal `d458d4d`, OpenSC `55a3c314a`, limpios al comenzar. Nuevo header
privado `iasecc-chipdoc-routing.h`, distribuido por Makefile.am e incluido
en pkcs15-iasecc.c para compilación. **No se llama automáticamente al
instalador** desde configuración, match/init/parse/bind; caller debe optar
explícitamente con objetos nativos ya decodificados y contexto válido.

`iasecc_chipdoc_routing_install(p15,key)` valida el contrato estricto de
operación, ausencia de cachePIN, magic/owner, estado inactivo/sin recovery,
plataforma quieta OpenSSL3/T1/capsEXT/límites existentes139/291 y callbacks
reset/lock/unlock/finish/hook. Exige tabla de algoritmos existente de una
entradaRSA2048/PAD_TYPE01|SHA256 y caps acotadas a EXT/USE_FCI_AC/PIN_INFO.
No heredar RNG/generación/raw/otros hashes ni modificar esos límites/tablas
para habilitar el routing. El initializer futuro debe justificarlos.
Rechaza una tablaops que sea la global card.driver.ops. Valida y asigna
memoria antes de mutar; fallo no cambia opciones/ops/objetos ni envía IO.

Asocia un único p15/key/PIN a una tarjeta, guarda su tablaops original y
original_clear, cambia cache0, hookclear y solo esa tabla de instancia.
Struct propio de tarjeta mantiene referencias prestadas; caller mantiene
tarjeta/contexto/lector/owner vivos y serializa las APIs. Antes de usar
objetos, comprueba pertenencia de key/PIN en obj_list acotada, identidad
card/reader/context/table, metadata y opcióncache0. Un cambio detectado
invalida canal y bloquea esa asociación, intentando cerrar reserva propia.
No prueba acceso concurrente/identidad mediante canal anónimo/timeoutPCSC.

Tabla filtrada conserva selección/read_binary/read_record/get_data para
metadata solo cuando está inactiva y sin recuperación. Durante canal
solo select_file de SDO (PATH/len0/AID exacto, sin FCI solicitado) va a
SELECT AID protegido. PIN_CMD admite VERIFY medido y GET_INFO local.
MSE/PSO son privados medidos; antes de esos callbacks se exige login vivo.
Logout cierra nuestra reserva. Deshabilita RNG/GET_RESPONSE/descifrado/
admin/escrituras/controles y otros callbacks ajenos. Check_SW/FCI puros y
hookreset de la tabla original se conservan. La restricción corresponde
a APIs del driver; no es sandbox de llamadas raw sc_transmit_apdu/PCSC.

VERIFY valida referencia/método/flags/encoding/bounds/ASCII antes de iniciar;
una transacción, query y credencial una vez. Respuestas fallidas cierran
reserva, incluso error anterior a PSO. GET_INFO válido no transmite: política
pública6..14/ASCII, logged_in solo de prueba local vigente y SSC/fase/provider
correctos, tries=-1/maxtries0. No queryPIN extra/max7/cache hash o credencial.
Un estado viejo o metadata/contexto inválidos no publican login y cierran
reserva (ese cierre puede hacer reset/unlock). No inferir login de opts o
de un mensaje de éxito previo. Tras PSO/reset pasa a LOGGED_OUT. Firma sin
otro login falla antes de PSO; login nuevo explícito abre canal nuevo.

`iasecc_chipdoc_routing_remove(card,owner,report)` cierra reserva propia y
restaura clear original, deja cache0 y borra p15/key/PIN **antes** de liberar
objetos. Mantiene filtro y marcador de desvinculación de propiedad de tarjeta
hasta finish: ningún fallback a IAS en claro, metadata o nuevo install.
Idempotente; recovery persiste si reset/unlock/pérdida fallan. La falta de
propiedad impide reset/unlock ajenos. Reporte en memoria contiene solo enum,
resultado y metadata de canal/startup/cierre; snapshot privado disponible por
routing_last_report, sin buffers/PIN/longitud real/hash/digest/firma/claves.

Hook clear de PKCS15 ejecuta remove y luego original_clear una sola vez en
lifecycle normal. `sc_pkcs15_card_free` llama ese hook antes de objetos/app.
Finish de routing desvincula si aún hay owner, libera struct propia, restaura
tabla original exclusivamente para destruir y llama finish original. Propaga
fallo del cierre, aun si ese finish tiene éxito. Card/context/driver data
siguen el orden de vida OpenSC: PKCS15 owner debe liberarse antes de destruir
la tarjeta. No reemplazar tabla/driver data ni liberar contexto/objetos a
espaldas de esa asociación. close.complete se refiere a nuestra reserva;
PKCS15 libera su lock exterior posteriormente, como en la etapa previa.

`make test/iasecc-chipdoc-routing` pasó **seis grupos**. Aquí los callbacks
instalados son de producción, no el routing anterior del fixture. Transporte,
lector, metadata idle y RSA/PIN son sintéticos; funciones reales nativas
DER/PKCS15 y dispatcher/locks. Prueba ambas rutas con/sin AID, RSA verificado,
segunda firma bloqueada sin nuevo login y dos login/firma explícitos sobre
el mismo binding, GET_INFO local/live/logout, lectura idle/bloqueo activo,
10 fallos install/globalops/caps/algos/owner y no mutación, error real de
cada etapa/unVERIFY/nocache/output, reset/unlock/pérdida/recuperación, cambio
metadata/cache/provider/pertenencia y **sc_pkcs15_card_free real** con owner
en heap, clear original y cierre fallido. Callbacks tras free no acceden a
objetos liberados; finish libera estado de routing y drv_data originales.

ASan/UBSan6 detect_leaks0 pasó: driver/codec/serializer/routing/fixture
instrumentados, bibliotecas PKCS15/ASN1/provider/locks precompiladas no;
sin prueba de fugas. Stub de crypto ausente rechaza install/status sin IO/
mutación; no builds reales otros backends/FIPS/plataformas. Fallos iniciales
de pruebas fueron enlazar el stub sin crypto con funciones de lifecycle,
usar un helper de app no exportado y reiniciar peer con políticaLe172 por
default; se corrigieron exclusivamente en fixture/stub. Ningún resultado
de esos fallos corresponde a tarjeta/PIN ni justifica variar Le real.

Sin PCSC/hardware/PIN real/GUI/Idopte/capturas nuevas. Firma JnUUiy9x vigente.
Siguen configuración/init/binding optativos/capacidades justificadas,
algoritmos/objetos/mecanismos y sesiones PKCS11; luego prueba real autorizada
por el usuario. RSA3072/JCOP4 GUI/consecutivas/reinserción real pendientes.

Pasaron47 grupos previos y suiteIAS/JCOP3 completa:708líneasPASS (no708casos
distintos). OpenSC/móduloPKCS11 compilados, sin nuevos exports. Core47,
helpers76/helpers hardware/observador/flake/lock/capturas intactos y sus
resultados anteriores vigentes/no repetidos.

### Configuración optativa, inicialización y binding nativo

La opción `card_driver iasecc { chipdoc_profile = cr-rsa2048; }` está
comentada en el ejemplo de configuración. No hay activación por defecto ni
nueva entrada ATR automática. La selección de driver debe mantener `iasecc`;
el ATR/AID compartidos con eOI no distinguen por sí solos aplicaciones.
El match optativo reconoce solo un candidato; el binding exige los objetos
nativos y el bootstrap posterior comprueba D004 antes del primer PIN.

`iasecc-chipdoc-init.h` implementa la rama nueva de `iasecc_init`, separada
del init IAS convencional. Requiere ATR exacto, contexto OpenSSL3 con libctx,
debug0, sin otro wrapper SM, T=1, ATR del lector coincidente y límites
declarados suficientes. Inicialización sin APDU/reset/consulta PIN:
asigna una tabla de instancia con selección ISO y lectura pública, checkSW/FCI,
finish/reset-hook, PIN bloqueado y logout vacío. Firma/admin/RNG no disponibles
hasta activar el binding. No hereda EF.ATR, serial ni algoritmos amplios IAS.

Anuncia exactamente RSA2048/PAD_TYPE01/SHA256, sin generación ni exponente
supuesto. EXT y PIN_INFO permiten la operación extendida y el estado local.
Los techos de tarjeta139/291 son límites operativos justificados por GA y PSO
ya observados, no máximos físicos descubiertos ni soporte RSA3072. No aumenta
un techo explícito de tarjeta/lector menor. Las lecturas públicas se limitan
a256 por llamada ISO; el bucle normal continúa por los bytes devueltos.
No se habilita READ extendido por anunciar el tamaño de respuesta del PSO.

`sc_pkcs15_bind` detecta solo esta instancia optativa. Fuerza cachés de
archivo/PIN desactivadas y bind nativo, incluso con emulación-first configurada;
un error no vuelve a emulación IAS. La nueva unidad privada
`iasecc-chipdoc-bind.c` enumera RSA y PIN nativos con límite32, exige una única
llave candidata ref91 y delega la validación completa al instalador. Solo
entonces activa callbacks de producción. Metadata incompleta deja PIN/firma
bloqueados. Los demás perfiles/drivers conservan su ruta anterior.
La unidad separada evita arrastrar routing SM al emulador IAS convencional.

`make test/iasecc-chipdoc-init` pasó **cuatro grupos** con config real,
initializer de producción, serializer/dispatcher/locks y `sc_pkcs15_bind`
reales; solo lector, archivos DER/ISO, credencial y peer/RSA son sintéticos.
Casos negativos de configuración/ATR/debug/protocolo/límites sin mutación/IO;
init mínimo sin APDU y PIN bloqueado; bind nativo de TokenInfo/ODF/PrKDF/AODF,
activación automática sin installer del fixture, un login/firma RSA verificada,
cache/emulación-first sobreescritos, PIN ausente rechazado; READ500 mediante
256+244 cortos. No ejecuta `sc_connect_card` ni la búsqueda entre drivers,
C_Initialize/C_Login/C_Sign, hardware o GUI. ASan/UBSan con detect_leaks0 cubre
initializer/fixture/peer/serializer instrumentados; biblioteca OpenSC con
bind/parser/routing/SM/provider/locks precompilados sin instrumentar. No prueba
fugas. Errores iniciales: TokenInfo sintético sin flags/OID inicializados y
fixture devolviendo FCP sin buffer en SELECT caso3; corregidos en tests, no
rechazos de tarjeta. El primer enlace de regresiones retuvo callbacks nuevos
al incluir el emulador fuente; se resolvió separando la unidad de activación.

Falta comprobar objetos/certificados, mecanismos y estado entre sesiones de
PKCS11 antes de pedir una prueba real del perfil. El éxito con helper JnUUiy9x
sigue siendo la única firma real de esta secuencia; no se repite hardware.
RSA3072, GUI, consecutivas/reinserción y otros backends/plataformas pendientes.

OpenSC/módulo PKCS11 compilados y sin nuevos símbolos privados exportados.
La suite IAS/JCOP3 completa pasó 712 líneas PASS, incluyendo los cuatro
nuevos grupos; son líneas de reporte, no 712 casos independientes. Core47,
helpers76, helpers hardware/observador/flake/lock/capturas no se modificaron;
sus suites sin cambios no se repitieron.

### Módulo PKCS11 compilado con contexto y tarjeta sintéticos

`make test/iasecc-chipdoc-pkcs11` carga el módulo construido del proyecto,
comprueba la ruta de C_Initialize con dladdr y fija LD_LIBRARY_PATH local.
Sustituye únicamente las APIs de contexto/descubrimiento/conexión/cancelación;
sc_connect_card simulado llama el initializer real anterior. No crea un
contexto PCSC, busca lectores reales, abre GUI ni obtiene una credencial real.
C_Initialize/GetSlotList/binding/framework/operaciones/sesiones son del módulo
normal precompilado, con PKCS15/ASN1/locks/dispatcher de libOpenSC normal.
Archivos TokenInfo/ODF/PrKDF/AODF/CDF, certificado/RSA/PIN y peer CA/SM sintéticos.
El certificado se genera en memoria y solo se compara/verifica en memoria.
No dump de valores: salida PASS fija, o enums numéricos del fixture en fallo.
Lanzador sin core dumps/preload/audit/trace API heredados. No es prueba del
match entre drivers ni de PCSC/hilos/plataformas/GUI/cierre temporal real.

La prueba llevó a cambios **solo para iasecc_chipdoc_configured** en cuatro
fuentes PKCS11; JCOP3 y otros perfiles conservan la conducta anterior:

- Framework: rechaza atomic/lock_login antes del binding de metadatos. Este
  perfil tiene reserva propia y no usa la pila genérica que guarda PIN para
  restaurar logins. Proyección de llaves privadas restringida a RSA2048/ref91,
  candidata ya validada al bind; una RSA2048/ref90 adicional no aparece como
  llave privada firmable. Metadatos nativos y certificados públicos se mantienen.
- Token/mecanismos: token readonly aun sin bit READONLY de TokenInfo y sin
  PIN-pad aunque el lector simulado declare esa capacidad. RSA_PKCS y
  SHA256_RSA_PKCS para firma2048; verify/hash son operaciones de software.
  Sin CKF_DECRYPT/gen/derivación/wrap, otros hashes RSA/PSS/raw/EC. Los hashes
  genéricos de software son válidos, no algoritmos de hash ejecutados en tarjeta.
  RSA_PKCS tiene el subconjunto **DigestInfo SHA256**, no payload arbitrario.
- Login/estado: solo CKU_USER, software PIN6..14; PIN nulo no solicita PIN-pad,
  rolesSO/CONTEXT_SPECIFIC se rechazan sin credencial. GET_INFO local valida
  prueba de login; ausencia/error pasa a PUBLIC y borra login lógico, sin
  asumir que UNKNOWN conserva un login. SignInit exige prueba vigente.
  Tras PSO cerrado, reset_login_state habilita un nuevo C_Login explícito.
- Logout/cierre: este framework llama sc_logout para la reserva propia aun
  con SW_PIN_LOGOUT_ONLY general. Última sesión cierra también si otro flujo
  ya borró login_user (p.ej. GetSessionInfo con tarjeta cambiada). Propaga el
  error de cierre después de liberar operaciones/handles. El driver controla
  ownership/reset/unlock; no se resetea una reserva ajena. C_Finalize genérico
  puede retornar OK ignorando errores de teardown: **no acredita cierre remoto**.

**Nueve grupos** pasaron: identidad del módulo/Initialize/slot/bind nativo y
certificado/publickey/IDs/token; mecanismos/flags y exclusión de key adicional;
login/DigestInfo RSA verificada/size/buffer pequeño/atributos sensibles/roles;
nuevo login en segunda sesión y cierre de una conservando la otra; SHA256
simple y multipart con firma verificada; mecanismos/payload inválidos antes
PSO, MAC malos enSELECT/MSE/PSO y VERIFY9000 sin autenticación, sin output ni
reintento; cambio/remoción simulados invalidando ambas sesiones y lifecycle;
atomic/lock_login sin token/PIN/metadata; reset final fallido propagado por
CloseAllSessions, handles liberados, instancia en cuarentena y otro login
bloqueado antes de cualquier VERIFY. Se recrea peer/counters del fixture
entre logins; no se cambia el estado/binding de producción para simular éxito.
Un payload local inválido anterior a PSO puede conservar login vivo; logout
explícito sí cierra. Un fallo de canal/PIN/cierre invalida y no restaura login.

ASan/UBSan9 detect_leaks0 pasó sobre initializer/fixture/peer/serializer;
module/framework/PKCS15/ASN1/SM/routing/provider/locks precompilados sin
instrumentar. No prueba fugas. La primera ejecución cargó el módulo Nix
por su SONAME, aunque enlazaba libOpenSC local: no acredita el módulo de
nuestro build. Se corrigió el lanzador y los resultados finales exigen dladdr
local. Fallos posteriores fueron supuestos del fixture sobre discoveries,
logout/cambio y estado, además de dos ubicaciones de patch/headers/linking;
se contrastaron con fuente/APIs, sin atribuirlos a tarjeta ni contador PIN.

Queda preparar un diagnóstico público del perfil, sin PIN, para que el usuario
compruebe el binding y certificados/mecanismos reales. Después, una firma
PKCS11 acotada y verificable; GUI/consecutivas/reinserción reales y RSA3072
siguen pendientes. Helper JnUUiy9x es la firma real vigente; no nueva captura
ni PCSC/hardware/PIN real/GUI/Idopte desde esta sesión.

## Diagnóstico público acotado del módulo local

`make inspect/jcop4-pkcs11 READER=0` ejecuta una sola consulta pública, sin
stdin, C_Login, C_Sign, PIN, Idopte o GUI. El ejecutable exige la ruta absoluta
del módulo del build y comprueba su origen con dladdr. Interpone únicamente
siete APIs de contexto/lectores/conexión; las reenvía a libOpenSC y presenta
al módulo un solo lector elegido. Exige el driver PCSC e iasecc integrado,
depuración0, OpenSSL3, ATR conocido, T=1 y límites declarados139/291. Estos
límites no son máximos físicos ni prueban PIN-pad. El perfil optativo y las
cachés desactivadas quedan en una configuración privada del diagnóstico.

La creación real del contexto PCSC puede conectar temporalmente otros
lectores y consultar sus capacidades antes de instalar el filtro. No se
cuentan esas operaciones como conexiones del módulo: el diagnóstico limita
el namespace y los posteriores comandos de tarjeta, no reemplaza el backend
PCSC ni demuestra una única conexión física durante toda la inicialización.
Escape, PIN-pad y PACE quedan deshabilitados para el diagnóstico.

El filtro por instancia del lector admite únicamente SELECT corto en canal
básico de MF, EF.DIR absoluto2F00, el AID conocido y los FID públicos
5031/5032/7001/7002/7004/7005/B001/B002 dentro de esa aplicación. READ BINARY
corto queda limitado por offset, tamaño y presupuesto por archivo; READ RECORD
solo permite los cuatro primeros registros secuenciales de EF.DIR. Los techos
1024 de EF.DIR/ODF/TokenInfo son límites locales; los restantes corresponden a
los tamaños públicos observados300/2048/4096/2368/3000/3000. Presupuesto de
lectura doble por archivo y máximo128 invocaciones del backend; no es un
conteo de transmisiones físicas internas de PCSC. No hay VERIFY ni consulta
vacía de PIN, MSE, GA, SM, PSO, D004, cambios, escrituras, extended, SFI,
chaining, resets explícitos o fallback. Se fijan NO_RETRY_WL/NO_GET_RESP/NO_SM;
primer fallo de guard, transporte, tamaño o SW bloquea toda continuación.
Solo9000 y EOF6282 de lectura/6A83 de READ RECORD son aceptados.

El módulo enumera EF.DIR con las funciones reales, exige una aplicación
conocida por AID/DF_NAME y realiza binding PKCS15 nativo. Comprueba token
readonly, mecanismos RSA2048/SHA256 y digests software, sesión pública,
certificados DER RSA2048 y coincidencia de IDs con llaves públicas en memoria.
Las privadas permanecen ocultas sin login. Esa coincidencia no demuestra
identidad, confianza del certificado ni correspondencia criptográfica de
cada llave. Certificados, IDs, etiquetas, nombres de lector, respuestas y
secretos no se imprimen. Stdout/stderr de las bibliotecas se descartan; el
informe contiene solo resultados, conteos, etapas y primer fallo.

El lanzador vacía el entorno, fija las bibliotecas locales, desactiva core
dumps y usa timeout45s/TERM+5s/KILL sin repetir. Directorio0700, informe y
configuración0600. Cierra sesión/finaliza y comprueba por separado intentos y
resultados de desconexión de tarjeta/lector, liberación de contexto y balance
de locks; Finalize0 aislado no acredita cierre. Un resultado0 del backend
sigue siendo su resultado, no una confirmación física independiente. No se
establece canal autenticado que necesite reset final. Ante timeout o cierre
incompleto, cerrar clientes/reinsertar antes de otra operación; revisar el
informe antes de repetir o introducir PIN.

`make test/jcop4-pkcs11-public` pasó cinco grupos nativos con el ejecutable
de producción incluido, módulo local real, contexto/lector/archivos/DER y
credenciales de software: binding y objetos públicos; sc_enum_apps/EF.DIR
real y SELECT absoluto abreviado; ATR/protocolo/límites/debug/lector incorrectos
sin APDU; primer error/SW61/transporte/tamaño y fallos de cierre; comandos
prohibidos y bloqueo permanente. Dos grupos del lanzador verifican entorno,
permisos, timeout simulado, estado propagado y ausencia de reintentos. Los
nueve grupos previos del módulo se repitieron por compartir el fixture y
pasaron. ASan/UBSan detect_leaks0 pasó los cinco grupos del diagnóstico;
módulo/libOpenSC precompilados no instrumentados, sin prueba de fugas. La
primera expectativa de liberación del fixture usaba0: sc_free_apps realmente
marca app_count=-1; se corrigió el test, no el backend ni los permisos.
No PCSC/hardware/PIN real/GUI/Idopte por el agente. Suites no modificadas no
repetidas. La posterior ejecución pública real se documenta a continuación;
firma PKCS11/GUI/consecutivas/reinserción y RSA3072 siguen pendientes.

## Enumeración PKCS11 real y siguiente firma de prueba

Informe privado `local/jcop4-pkcs11-public-Z5agqf5n/report.txt`,
2026-10-14T22:40:33Z (16:40 CR), proyecto6a22c7a/OpenSC52057cc6a:
build/initialize/slots/token/mechanismos/sesión0, dos certificados y dos
llaves públicas RSA2048 con IDs coincidentes en memoria, privadas ocultas sin
login. Lector conocido/T1/límites139/291 validados;79 llamadas backend
(16 SELECT+63 READ), guard0, sin primer fallo. Nueve locks/unlocks balanceados,
una conexión/desconexión módulo-tarjeta/lector y creación/liberación contexto;
los tres resultados de cierre0, CloseSession/Finalize/helper0. Acredita la
enumeración pública con ese módulo/configuración/tarjeta, sin VERIFY/PIN/
MSE/GA/SM/firma; no identifica la llave privada ni valida confianza del
certificado. No repetir este diagnóstico por inercia.

`make sign/jcop4-pkcs11 READER=0` compila otro ejecutable que reutiliza el
filtro/contexto/metadata público y añade código de login solo en esta variante.
El binario público conserva sus permisos anteriores y no compila ese código.
Nueva instancia necesita binding y certificados públicos antes de la firma:
esas lecturas son preparación de la operación, no un inventario adicional.
Firma exclusivamente el texto ASCII fijo mostrado en la terminal:

```text
libjcop3p11: prueba tecnica PKCS11 JCOP4 RSA SHA-256.
```

Recoge una línea en /dev/tty, modo canónico y eco desactivado; admite ASCII
imprimible6..14 sin truncar/normalizar la credencial. Rechaza entradas largas
y vacía el input pendiente, no pasa PIN por argv/env/archivo/pipe ni lo imprime.
INT/TERM/HUP/QUIT interrumpen la espera; poll evita perder una cancelación justo
antes de read. Restaura atributos de terminal antes de C_Login y borra el
buffer en toda salida. No conserva PIN para otro login. El lanzador exige
terminal, guarda/restaura también sus atributos, incluido timeout/KILL.

C_Login una vez CKU_USER; el módulo valida el canal, MF y estado autenticado
antes del único VERIFY con credencial. El perfil exige el estado observado
63C7: no asumir un máximo persistente7 ni reinicio del contador; cualquier
estado diferente detiene sin credencial. Encuentra una sola llave privada
expuesta y un certificado cuyo ID coincida en memoria. C_SignInit y una única
C_Sign con CKM_SHA256_RSA_PKCS firman el texto fijo. No PDF/firma multipart/
queries de tamaño ni firma con otra llave. Verifica RSA2048/PKCS1v1.5/SHA256
y exige rechazo del mensaje con un bit cambiado, sin otra operación de tarjeta.
No guarda mensaje/firma/certificado ni datos personales en esta prueba: solo
metadatos fijos; la validación criptográfica ocurre en memoria.

El filtro autenticado limita encabezados/casos/longitudes/TLV y orden a doce
APDU: AID, control D004, READ137, MSE CA, GA sinLe, READ protegido, MF protegido,
consulta PIN, VERIFY cifrado, AID protegido, MSE firma y PSO extendido. Comprueba
reserva propia y fase del driver en cada envío; esas fases posteriores dependen
de respuestas autenticadas por el driver. Es un límite de transporte, no una
segunda verificación criptográfica independiente del canal. No copia claves,
SSC o plaintext PIN. Cuenta el VERIFY y PSO antes de llamar al backend; bloqueo
definitivo, sin GET RESPONSE/6C/chaining/variantes/relogin. Solo permite reset
inicial de la reserva y un reset final después de borrar canal/detachar reserva,
incluso si falló un intercambio o hubo cancelación. Sin reset automático fuera
de esta operación. Cierra explícitamente login/sesión y comprueba resultados
y conteos de reset, locks, tarjeta, lector y contexto; Finalize0 no basta.

Lanzador envvacío/liblocal/debug0/core0/stdinvacío/informe+config0600/dir0700,
una invocación, timeout --foreground120s TERM+5s KILL para permitir /dev/tty
interactivo. Un timeout PCSC puede impedir el cierre aunque se restaure el
eco: ante cierre incompleto/timeout, cerrar clientes y reinsertar antes de
otra operación; revisar informe sin repetir PIN. No cambiar la tarjeta ni
abrir otro cliente durante la prueba.

Pasaron cinco grupos nativos del nuevo make test/jcop4-pkcs11-sign con el
helper y módulo reales, transporte/certificados/RSA/PIN sintéticos y PTY real:
firma/verificación/negativo con PIN6/14; estados autenticados no disponibles,
PIN incorrecto simulado,9000 sinMAC, PSO MACmalo, reset inicial/final malo y
query61 sin segundo intento; entradas cortas/largas/noASCII/INT/TERM sin
VERIFY y eco/atributos restaurados/input pendiente vacío; reset fuera de
operación bloqueado; encabezados/fases/ownership/credencial repetida y comando
extra bloqueados antes del backend. Dos grupos del lanzador con herramientas
falsas/PTY real verifican terminal obligatoria, entorno, permisos, ejecución
única/timeout foreground, propagación de errores y restauración tras KILL
simulado. Cinco grupos ASan/UBSan detect_leaks0 pasaron; módulo/libOpenSC
precompilados no instrumentados, sin prueba de fugas. También pasaron de nuevo
los siete grupos del diagnóstico público al compartir el helper y fixture.
Fallos iniciales fueron fixtures (PIN sintético del padre sin inicializar y
expectativa de finish exitoso tras reset fallido) y compilación de macroEXT/
write; se corrigieron antes de intervenir la tarjeta. Submódulo, driver y
helpers autenticados previos intactos. No hardware/PIN real/GUI/Idopte por
el agente; firma PKCS11 real/firmador/PDF/consecutivas/reinserción/RSA3072
siguen pendientes.

## Firma PKCS11 real confirmada y lanzador GUI

`local/jcop4-pkcs11-sign-v0hqHrzR/report.txt`,2026-10-14T22:59:50Z (16:59CR),
principal82ab080/OpenSC52057cc6a: módulo local, fase pública válida, login/
SignInit/Sign/session-state0, RSA2048/SHA256 válida y mensaje conbit cambiado
rechazado. Un C_Login,12APDU autenticadas, un VERIFY con credencial y un PSO.
Total91 llamadas backend (79 públicas anteriores+12 autenticadas),guard0, sin
primer fallo; reset inicial/final intentados una vez y0,11locks/11unlocks,
tarjeta/lector/contexto desconectados/liberados una vez con0, sesión/finalize/
helper0. C_Logout257 (0x101,CKR_USER_NOT_LOGGED_IN) es esperado: el driver cerró
la reserva después de PSO y la sesión ya era pública, comprobado anteslogout.
No equivale al fallo de login CKR_USER_NOT_LOGGED_IN previo de JCOP3.

Esto acredita C_Login y una C_Sign CKM_SHA256_RSA_PKCS reales del módulo local
y la correspondencia criptográfica de esa firma con la llave pública del
certificado seleccionado enmemoria. No se guardaron firma/certificado/PIN,
no se valida confianza/revocación/identidad. No demuestra aún CKM_RSA_PKCS,
multipart/tamaño, consecutivas, reinserción, PDF o firmador reales. No repetir
esta firma ni el diagnóstico público como primer paso.

Nuevo `make firmador/jcop4`: lanzador GUI únicamente, sin CLI/PIN desde
shell/spy/Idopte/debug APDU. El wrapper instalado firmador2.0.0 respeta
LIBASEP11; su wrapper interior ejecuta Java21 sin modificar el entorno. Se
leyeron ambos scripts, sin ejecutar Java. LIBASEP11 señala el módulo local y
LD_LIBRARY_PATH fija libOpenSC local. Usa perfil cr-rsa2048/iasecc,
use_file_caching/use_pin_caching/try_emulation_first desactivados,
atomic/lock_login desactivados, un slot por tarjeta, PIN-pad/PACE desactivados
y políticas PCSC leave para no añadir resets al cerrar locks ordinarios.
La reserva/reset explícitos del driver permanecen activos. No cambios de
Java/firmador/driver/módulo ni perfil JCOP3.

Entorno vacío con lista de variables de escritorio/sesión/home/locale/trust
stores, PATH y rutas locales; elimina preloads/audit/PCSC delegates/debug/
experimentos JCOP3 y JAVA_TOOL_OPTIONS/_JAVA_OPTIONS/JDK_JAVA_OPTIONS heredados.
No captura stdout/stderr ni contenido del documento. OPENSC_API_TRACE_FILE
propio habilita el logger existente de enums y números: eventos, handles,
CKR, mecanismos y referencias/tamaños de llave; no PIN, longitud PIN, APDU,
atributos, documentos, certificados o hashes. Fuente api-trace.h y callsites
revisados. Config/api.log/report0600, directorio0700, umask077/core0. Informe
de fecha/HEAD/modo y exit del programa; exit0 no acredita firmado/cierre físico.
La GUI usa el módulo directamente, **sin el filtro de transporte, namespace
de un lector, cuota de intentos o timeout de los helpers**. Cada login del
perfil sigue el camino de producción probado y no conserva PIN para reintento;
las decisiones de la GUI/usuario pueden iniciar operaciones adicionales.

Cuatro grupos nuevos de software pasaron: configuración real mediante parser
OpenSC/load_pkcs11_parameters, entorno GUI preservado/diagnósticos descartados,
módulo/core locales y permisos; fakefirmador sin args/lectura PIN con exit17
propagado y sin asumir firma;11mutaciones de perfil/driver/cache/emulación/
atomic/lock_login/slots/PIN-pad/PACE/reconnect rechazadas por checker real;
args/módulo core ausente detienen antes GUI. El checker compartido añade modo
chipdoc conservando interfaz previa; once grupos JCOP3 anteriores repetidos
y pasaron. No context create/PCSC/Java/GUI/PIN/hardware reales por el agente;
resto de suites sin cambios no repetidas, submódulo intacto.

Siguiente intervención: mismaJCOP4, otrosclientes/firmador cerrados,
`nix develop -c make firmador/jcop4`. Abrir doc.pdf original, seleccionar
LlaveDeFirma, firmar una sola vez con PIN correcto y guardar
local/jcop4-firmador-first.pdf con nombre nuevo, cerrar GUI. Sin reinserción
ni segunda firma todavía. Ante error detenerse, conservar informe/API y no
repetir PIN porinercia. Luego revisar mecanismos/llave/login/firma/cierre
numéricos y verificar CMS/ByteRange del PDF offline sin mostrar identidad.
Consecutivas/reinserción/JCOP4RSA3072/otrasplataformas/upstream pendientes.

## Primer PDF del firmador verificado offline

Proyecto7bb9661/OpenSC52057cc6a, limpios al comenzar. Informe usuario
`local/jcop4-firmador-api-2VJ372cf/report.txt`,2026-10-14T23:10:42Z (17:10CR),
exitGUI0 y127eventos API. Una C_Login0, C_SignInit0/CKM_SHA256_RSA_PKCS0x40,
C_SignUpdate0, SIGN_KEY0/ref0x91/RSA2048, KEY_SELECT0/MSE0, KEY_COMPUTE0x100
(longitud256 positiva, no CKR), C_SignFinal0. Logout0x101 anteslogin fue sobre
sesiónpública/no autenticada. Tres GetSessionInfo0/USER antesfirma. No
C_GetSessionInfo después de firma ni evento C_Finalize. Ocho OpenSession y
cinco CloseSession0; no inferir fuga o limpieza completa de ese desfase: la
traza no registra todas las etapas del teardown. CARD_RESET no se registra
paraChipDoc porque el logger existente de sc_reset está limitado aAMOS.
No contar resets/VERIFY/PSO de esta GUI desde ausencia de eventos; el driver
entrega256 solo tras verificar SM y cierre de su reserva.

El archivo esperado local/jcop4-firmador-first.pdf no apareció; el resultado
nuevo fue doc-firmado.pdf en raíz, mtime2026-10-14T23:11:00Z,80454bytes. Se
conservó una copia privada junto al original6967bytes para no depender de
posteriores sobrescrituras. Informe offline:
`local/jcop4-pdf-verification-z4w35w5c/report.json`. Directorio0700/archivos0600,
originales intactos; no datos identificables en el resumen mostrado. Mutool
resolvió los dos diccionarios de firma activos desde AcroForm; sin NSS/pdfsig
ni módulos PKCS11/PCSC. OpenSSL ejecutado con entorno limpio/OPENSSL_CONF
/dev/null, CMS -binary -noverify, sin red/consulta de confianza/OCSP/lector/PIN.

| Objeto | Rangos ByteRange | Resultado |
| --- | --- | --- |
| CAdES tarjeta | [0,7343,33969,4430] | CMS/SHA256 válido, RSA2048, certificado DER idéntico al observado para JCOP4 |
| Sello documento RFC3161 | [0,60468,79414,1040] | CMS válido e imprint SHA512 coincide con esos rangos; revisión final80454 |

Cada hueco corresponde exactamente a Contents, longitud DER y relleno nulo
validados; rangos positivos/dentroarchivo/sin solaparse y contenidos activos
coinciden con los originales. La firma CAdES cubre revisión38399, no los
incrementos posteriores; el sello de documento cubre el archivo final.
Los6967bytes originales permanecen como prefijo y están dentro del primer
rango de ambas firmas. Alterar un bit del contenido CAdES causa rechazo de
CMS. Además se verificó el token RFC3161 de atributo unsigned
signatureTimeStampToken del CAdES: CMS válido/imprintSHA512 coincide con los
256bytes SignatureValue. Son dos sellos válidos matemáticamente, no validación
de confianza/revocación/identidad/tiempo jurídico. No se comparó render de
páginas; la conservación comprobada es de bytes originales cubiertos.

Esto acredita una firma PDF real y multipart SHA256_RSA_PKCS con ese módulo,
firmador2.0.0/Java21 y JCOP4. No prueba aún consecutivas/reinserción/otras
llaves/tarjetas/lectores/RSA3072. No cambios de código ni nuevas regresiones
software en esta revisión; pruebas anteriores vigentes. No PCSC/hardware/
PINreal/GUI/Idopte/red poragente, solo análisis de archivos y crypto offline.

Siguiente intervención: dos firmas en una misma instancia para comprobar
que Java y PKCS11 renuevan login después del cierre del primer PSO. Se
prepararon dos copias0600 idénticas del doc.pdf original en
`local/jcop4-consecutive-rwva61gm/primero.pdf` y `segundo.pdf`,directorios0700.
Esta preparación no accedió a tarjeta/lector ni modifica originales.

1. Clientescerrados/mismaJCOP4: `nix develop -c make firmador/jcop4`.
2. Abrir primero.pdf, seleccionar LlaveDeFirma, firmar y conservar
   primero-firmado.pdf en ese directorio.
3. Sin cerrar GUI ni retirar tarjeta, abrir segundo.pdf y firmar hacia
   segundo-firmado.pdf. Se firman originales distintos con los mismos bytes,
   no el resultado de la primera firma.
4. Cerrar GUI después; detenerse ante primer error sin repetir PIN.

Introducir PIN correcto solo cuando lo solicite; registrar si lo pide de nuevo.
No anticipar éxito/cantidad de logins del cliente: se revisará la traza y cada
CMS posteriormente. Reinserción será otra prueba después de estos resultados.

## Dos firmas consecutivas confirmadas en la GUI

Principal90ea080/OpenSC52057cc6a, limpios al comenzar. Ejecución
`local/jcop4-firmador-api-KKBZwTnU/report.txt`,2026-10-14T23:22:27Z (17:22CR),
exitGUI0/221eventos API. Dos C_Login0, separados por una firma completa; cada
una tiene C_SignInit0/0x40SHA256_RSA_PKCS, Update0, llave0x91/RSA2048,
KEY_SELECT/MSE0, KEY_COMPUTE256 y SignFinal0. Dos Logout0x101 antes sus logins
sobre sesiones públicas esperados.6GetSessionInfo0/USER antes operaciones,
31CARD_PRESENCE0x1 y ningún SLOT_TOKEN_REMOVED. La traza y ejecución registrada
son consistentes con los pasos de dos firmas sin retirar tarjeta en la misma
GUI. Dos logins no determinan cuántos diálogos PIN mostró Java; eso no se
registró. No contar VERIFY/reset/PSO físicos desde la traza limitada.

Los resultados están en local/jcop4-consecutive-rwva61gm/primero-firmado.pdf
y segundo-firmado.pdf. Ambos originales son6967bytes idénticos a doc.pdf.
Se verificaron offline los diccionarios activos, huecos Contents/rangos/
DER/relleno0, CMS SHA256 con certificado DER exactoJCOP4/RSA2048, rechazo de
un bit cambiado, tokenRFC3161 de signatureTimeStampToken e imprintSHA512
sobre SignatureValue, y DocTS RFC3161/imprintSHA512 sobre ByteRange final.
Ambos conservan/cubren todos los bytes originales. Copias+capturas+report
privados0700/0600, sin datos identificables en el resumen:

| Resultado | PDF final | CAdES/revisión | DocTS/rangos finales | Informe privado |
| --- | --- | --- | --- | --- |
| Primero |80454bytes|[0,7343,33969,4430]/38399|[0,60468,79414,1040]|local/jcop4-pdf-verification-kfjd3fcr/report.json|
| Segundo |80474bytes|[0,7343,33969,4430]/38399|[0,60488,79434,1040]|local/jcop4-pdf-verification-01rpnzzp/report.json|

Firma tarjeta cubre revisión38399; sello documento cubre todo PDF final en
cada caso. Confianza/revocación/identidad/render no evaluados. Sin NSS/red/
PCSC/PIN/GUI/Idopte del agente; Mutool/OpenSSL/envlimpio/OPENSSL_CONF/dev/null,
misma verificación offline anterior aplicada a cada archivo nuevo. No código
modificado ni regresiones repetidas; resultados software anteriores vigentes.
Se confirma que este recorrido del firmador puede renovar login después de
una firma y producir otra firma correcta con el perfil optativo observado.
No prueba reinserción ni otros lectores/modelos/plataformas/RSA3072.

Se prepararon dos copias idénticas0600 del original en
local/jcop4-reinsert-vnw9_xsr/antes.pdf y despues.pdf, directorio0700, sin
lector/PIN. Siguiente usuario: clientescerrados/mismaJCOP4, abrir
`nix develop -c make firmador/jcop4`, firmar antes.pdf/LlaveDeFirma y conservar
antes-firmado.pdf. Después de guardar, **mantener GUI/lector conectados**,
retirar solo tarjeta, esperar unos segundos y reinsertar la misma. Sin
intentar firmar durante ausencia, abrir despues.pdf, firmar una vez y
conservar despues-firmado.pdf. Cerrar GUI después. Ante primer error de GUI
o firma, detenerse sin reiniciar/repetir PIN; se revisarán eventos y resultados
antes de otra operación. La espera no demuestra detección en Java: la traza
posterior deberá mostrar cómo se recuperó el token. No inventario/estado/
D004 separado ni otras pruebas de concurrencia durante esa secuencia.
