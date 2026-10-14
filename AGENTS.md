# libjcop3p11: contexto para continuar

**Autorización vigente del usuario:** el agente ejecuta Git y trabajo local
sin lector/tarjeta/PIN/GUI. **Por instrucción nueva del usuario, todos los
comandos que accedan a PC/SC, al lector o a la tarjeta quedan para el usuario**,
incluidas enumeraciones para activar pcscd y diagnósticos sin PIN. El agente
prepara comandos, revisa informes privados y ejecuta compilación/pruebas con
transporte simulado. PIN/GUI también los ejecuta el usuario. Esto sustituye
la autorización anterior de hardware por el agente; conservar límites de
cada diagnóstico y privacidad. Git sigue autorizado pero .git solo lectura.

**Última revisión: login JCOP4 confirmado; recolector de respuesta larga probado offline.**
Principal `b8800b2`, OpenSC `3dfef6704`, limpios al comenzar. Informe usuario
`local/jcop4-channel-login-hSQwNDBj/report.txt`,2026-10-14T07:01:57Z
(14Oct01:01CR): canal/MF/query63C7 autenticados, login transporte0/decoder
accepted/MACverified/inner9000, attempted1/pin_verified_no_signature. Cierre
reset/unlock/disconnect0, guard0/11APDU/interrupted0/helper0. **Primer login
con credencial bajo SM acreditado, ninguna firma RSA aún.** Formato candidato
ASCII sin padding de credencial aceptado en este test; no longitud/valor
guardados. SM padding sigue framing. ContextoMF+SM difiere del fallo antiguo
6982: no aislar SM como única causa por este resultado. No repetir login/
estado/D004/canal ni pedir PIN por inercia, no máximo7/reset por firma probado.

Nuevo trabajo estático mismo libt_ias:36f540..36f57b arma PSO002A9E9A con
datos/Le256 en una rama, no prueba despacho runtime/MSE/entrada RSA actual.
SM24483c..2448a0 sigue exterior256 o Le+35 según flag. Transport22d950
ensambla61xx por GET RESPONSE antes del decoder SM240f21/2436a0:22dab4..
22db69 armaC0/00/00/LeSW2 (0=>256), acumula datos y reemplaza SW intermedio.
CLA segúncard+49:1->00,0->original(22dbe0 llama227760). Constructor22de3a..
22de45 inicializa49/4A a0; no asumir valor/cadena activos por constructor.
Rama6C reenvía en22dbf0..22dc56 y rama sinLe transforma61 a9000 en22dbb9:
**no copiar esos comportamientos ni probar CLA alternativas sobre hardware**.
No Idopte ejecutado ni nuevas capturas/autenticación. EF.ATR público anterior
DORk47gE dio0/0límites noobtenidos, no habilita extended por conjetura.

Nuevo tools/jcop4-response-collector.h, aislado sinPCSC/PIN/canal/driver:
recibe fragmento inicial yaenviado/callback, políticaCLA00/0C explícita sin
default/fallback, límites293/421bytes (candidatos DO87 cifrado/payload256/384),
máximo3GET adicionales. Solo61xx habilita cinco bytesCLA C00000Le; SW2=0
significa256. Capacidad antes de enviar, cada respuesta<=Le/258/progreso,
terminación9000, min14bytes cuerpo. Nunca reenvía PSO/6C, escribe/admin,
seal/SSC/VERIFY ni autentica fragmentos. Éxito **COMPLETE_UNAUTHENTICATED**:
devuelve bytes opacos, no firma/SW interno/éxitoSM. Dueño exige MAC/semántica
una vez tras ensamblado, conserva lock/conexión/estadoWAITING; error obliga
invalidarSM/cerrar. Salida intacta/written0 en fallo; temporales limpiados.
No conectado a helper real: guard actual sigue bloqueandoC0/PSO/MSEfirma.
421 solo límite sintético, no soporteRSA3072 ni DO97/Le384 implementados.

Nuevo make test/jcop4-response-collector pasó **5 grupos**: dos políticas/
293/421/data inicial+61/frame completo, errores/transporte/no progreso/6C/
truncación/oversize/budget/argumentos/capacidad sin fallback ni salida parcial.
Fixture293 independiente OpenSSL CLI (payload00..FF **no firma RSA**), recolecta
con dosGET sin avanzarSSC; codec final validaMAC/SSC2/plain256. Alterar cada
uno de291bytes de cuerpo rechaza antes de plaintext/SW. ASan/UBSan5pasaron
detect_leaks0; libcrypto precompilada sin instrumentar/no prueba de fugas.
Suitesanteriores sin cambios vigentes/no repetidas; bashsyntax/diffcheck0.
Solo Makefile/collector/tests/script/docs, helperC/driver/submódulo/observador/
flake/lock intactos. Ningún comando PCSC/lector/tarjeta/PIN real/GUI por agente.

**Siguiente trabajo del agente:** terminar ruta MSE/RSA/PSO y justificar
CLA de continuación; preparar adaptador acotado de firma con misma conexión/
lock/loginSSC8, retornoMSE/PSO conMAC, verificación RSA/SHA256 con certificado
público vinculado antes de persistir resultado. Hay certificados privados
en local/jcop4-signature-5Dpadr1N/certificate.der y otras pruebas anteriores;
solo se enumeraron rutas, no se volcó contenido/identidad ni compararon todavía.
No usar4FGWOtpI por error humano conocido. No transformar firma de256en éxito
sin verificar; noGET real/PIN solicitado aún. FirmaJCOP4/RSA3072/driverpendientes.
Git sololectura: commit docs3/Makefile y **nuevos3** collector.h/test.c/script;
no capturas/build ni eludir.git. No intervención de hardware necesaria ahora.

**Revisión anterior: timeout de login antes de PIN; corregido grupo de terminal del lanzador.**
Principal `3d91123`, OpenSC `3dfef6704`, limpios al comenzar. Informe usuario
`local/jcop4-channel-login-m6tjqsjj/report.txt`,2026-10-14T06:54:44Z
(14Oct00:54CR): canal/MF/query autenticados9000/63C7, luego input rechazado,
login_attempted0/not_confirmed_stop. Resetfinal/unlock/disconnect0, guard0,
command_count10/interrupted1/helper_exit_status124. **No VERIFY con credencial**
ni fallo de PIN/canal probado; consulta vacía previa sí se envió. No atribuir
consumo de intento ni6982. Cierre completo observado, no suponer sesión abierta.

Bug local del launcher: GNU timeout desde shell crea grupo fuera del foreground
TTY; helper queda suspendido al acceder a terminal antes del prompt/Ctrl+C no
le llega directamente. Reproducido solo con PTY y binario tests/pin-input que
no ejecuta su entrypoint de tarjeta: shell persistente -> timeout real anidado,
helper estadoT/grupo distintoTTY, prompt ausente/eco aún activo/status124.
Timeout directo como líder de sesión funciona: fixture anterior no reproducía
el control de trabajos del launcher. Timeout exec simulado también lo omitía.
No pruebaPCSC ni atribuir la suspensión a lector/tarjeta.

Se añade **--foreground solo en modo--login-once**, conserva TERM45s/KILL5s,
stdinhelper vacío/inputdevtty/privacidad/guard/APDU/unintento/cierre. Modi noPIN
conservan su política anterior. Foreground alcanza al helper; GNU documenta
que no aplica timeout a sus descendientes: helper no lanza subprocesos de
trabajo. Sin nueva firma/PIN por agente, no cambia formato ni aceptaciónSW.

Pasaron **13 grupos login** (5nativos+5bootstrap lector simulado+3PTY/wrapper)
y **22 probe** por launcher compartido. Nueva regresión timeout GNU real
anidado: baseline sinforeground reproduce124/prompt ausente; conforeground
acepta PIN ficticio oculto, Ctrl+C porTTY cancela antes de timeout, timeout
sin input sigue124 pero devuelve metadata y restaura eco. Wrapper exigeflag
foreground junto con límites previos. Bashsyntax/diffcheck pasan; suites
cripto/ASan previas vigentes, no repetidas por solo launcher/Python/docs.
Driver/submódulo/helperC/observador/flake/lock intactos. No PCSC/tarjeta/PIN real.

**Siguiente intervención:** cerrar otros clientes y reinsertar mismaJCOP4
antes de repetir, siguiendo regla previa tras timeout, aunque este informe
sí muestra cierre0. Luego una sola ejecución:
`nix develop -c make login/jcop4-channel READER=0`. PIN correcto solo si pide
helper; ahora debe verse prompt. No repeat si falla/PINincorrecto. Revisar
login_attempted/MAC/innerSW/result/helper/cierre antes de firmar. Login real/
firmaJCOP4/RSA3072/antiguo6982 pendientes. Git sololectura: commit AGENTS/
README/docs, scripts/probe-jcop4-channel.sh, tests/jcop4-login-terminal.py;
ningún archivo nuevo/captura/build, no eludir restricciónGit.

**Revisión anterior: canal/MF/estado PIN autenticados en tarjeta; login único preparado.**
Principal `3cdadc1`, OpenSC `3dfef6704`, limpios al comenzar. Informe usuario
`local/jcop4-channel-pin-context-H3dqRC8j/report.txt`,2026-10-14T06:35:03Z
(14Oct00:35CR): canal íntegro confirmado, MF9000/MAC válido, query16/status99/
exterior63C7, decoder accepted/MACverified/inner63C7/available_unverified,
pin_context_result=status_observed_no_pin_no_login. Finalreset/unlock/
disconnect0, guard0/10APDU/helper0. **Estado autenticado, no login ni firma**.
63C7 observado no acredita máximo7, reset por firma ni identidad de tarjeta.
No repetir diagnóstico de canal/MF/estado por inercia; buffers borrados.

Revisión estática de formato:flagperfil8 entra383cb0 y agrega límites de
objetos PIN por slots30/38, retorna al mismo dispatcher383c69; no cambia por
sí solo la rama raw2/383f39 ni acredita formato efectivo. Campo token379
inicial0 (3b0197, palabra378) puede activar longitud fija; escritura1 en
3b5a2f tras rama etiquetadaOrange/IN.pro.0183.01, no atribuirla a tarjetaCR.
Se conserva formato candidato del metadata PKCS15 ya observado: referencia
global1/UTF8/flags11/sin padding/bounds6..14/ruta vacía. No código Idopte
copiado ni ejecutado. Diagnóstico libre prueba ese candidato con SM, no afirma
que se haya resuelto cada clase/perfil/formateador de Idopte.

Nuevo **make login/jcop4-channel READER=0**: binario separado
jcop4-channel-login (--login-once), comparte bootstrap/cierre por include
de probe.c pero solo ese build compila colector de PIN/guard VERIFY. Probe
normal rechaza --login-once antes de conexión, no env activa credenciales.
Misma JCOP4 RSA2048, mismo lock/conexión; tras canal+D004 íntegro/MF9000/query
autenticado **exactamente63C7**, retieneSSC6 solo para login. Otros estados,
fallos o budget agotado detienen antes de pedir PIN;7 es precondición observada,
no máximo universal. Leer/dev/tty sin eco, no argv/env/pass/stdin ni informe;
acepta solo ASCII imprimible6..14 bytes (subconjuntoUTF8), no conversión/hash/
relleno de credencial. Otros caracteres/cancelación/longitudes detienen sin
VERIFY; no experimentar formatos alternativos sobre tarjeta.

Un único VERIFY interior00200001 con credencial; SSC7, CLA0C/cuerpo29/wire35,
DO871101+cipher16/DO8E8, sinDO97/Le interior, exteriorLe256. No PIN clear al
transporte: borrar input antes; método2 SM es framing, no paddingPIN. Guard
compara frame completo exacto y estado; no plaintext/ref3/cambio/reenvío.
RespuestaSSC8 exige MAC+coherencia exterior63Cx/DO99 o exterior9000; éxito
solo inner9000/datos0. Warning no login/no retry. login_attempted cuenta
despacho al backend; transport/decoder/MAC/innerSW validados solo metadatos,
sin credential/longitud/cipher/MAC/keys. Borra estado incluso éxito; reset
final/unlock/disconnect, sin firma/PSO/lectura posterior. No key privada fuera
de tarjeta. Binario normal/observador/driver/crypto/submódulo intactos.

Lanzador requiere terminalstdin/stdout antes de compilar, stdinhelper vacío;
PIN va por/dev/tty, logs0700/0600/configdebug0/tracesaisladas/core0 como antes.
SIGINT/TERM sinSA_RESTART restauran eco antes de cierre; timeout45sTERM+5sKILL.
Timeout/reset ausente/fallido exige cerrar clientes/reinsertar antes de otro uso.
**Un VERIFY fallido puede consumir intento**: usuario usa PIN correcto y no
repite automáticamente; no ejecutar oferta de PIN incorrecto ni asumirreset.

Pasaron **12 grupos login**(5nativos+5OpenSC bootstrap/reader simulado+2PTY/
wrapper), PIN exclusivamente ficticio: límites6..14/encryptionECB-CBC/CMAC
independientes/SSC7/8, failures/replay/warnings/guard/no retry/close, input sin
eco/restauraciónTERM y launcher una llamada/permisos/EOF/errores. ASan/UBSan
10nativo/bootstrap+PTY input pasó, wrapper simulado; detect_leaks0, libs
precompiladas sin instrumentar/no prueba de fugas. **22 grupos probe** pasaron
de nuevo por helper/lanzador/build compartidos; ambos binarios compilados.
Fallo inicial build fue write con resultado ignorado/Werror en salto de línea
del colector, corregido; no error de tarjeta/PIN. Suites crypto sin cambio no
repetidas; flake/lock/Idopte/driver/submódulo intactos. Sin PCSC/lector/tarjeta/
PIN real/GUI por agente; pruebasPTY no crean contexto de tarjeta.

**Siguiente intervención:** misma JCOP4 RSA2048, firmador/otros clientes
cerrados, `nix develop -c make login/jcop4-channel READER=0`, una sola vez.
Introducir PIN correcto únicamente si pide el helper, sin pegarlo en chat.
No usar viejos sign/jcop4-apdu/verify-first por inercia. Revisar login_attempted/
transport/decode/mac/response/sw/result y cierre antes de firma u otro intento.
Login real/Jcop4firma/RSA3072/causa6982 siguen pendientes. Git solo lectura:
commit Makefile/scripts2/toolsprobe/testsprobe/docs3 y **seis nuevos**:
tools/login.c/login.h, tests/login.c/pin-input.c/login-terminal.py y script
test-login.sh. No capturas/build ni eludir .git sololectura.

**Revisión anterior: respuesta PIN vacío tiene SW exterior63C7; regla9000 corregida solo para query.**
Principal `02b0d2c`, OpenSC `3dfef6704`, limpios al comenzar. Informe usuario
`local/jcop4-channel-pin-context-8H2TMoTn/report.txt`,2026-10-14T06:28:45Z
(14Oct00:28CR): canal íntegro confirmado, MF respuesta51/outer9000/87,
decode accepted/MAC verified/inner9000. Query transporte0/respuesta16,
outer63C7/leading status99, decoder outer_sw_not_9000/MAC not_checked.
Finalreset/unlock/disconnect0, guard0/10comandos/helper1. **Bloqueo local por
regla de SW exterior, no fallo MAC comprobado ni rechazo6982/PIN incorrecto.**
No prueba aún contenidoDO99/MAC/estado autenticado ni contador máximo7 o
efectos de reset; buffers borrados, no raw recuperable. Sin PIN suministrado.

Análisis estático mismo libt_ias/hash2380540c..542dc4 confirmado:22bfa0
obtiene los dos últimos bytes;243981..24399f y243abb..243ae2 comparan ambos
con DO99, sin exigir9000, antes de MAC243c50..243c69.3756d9..3756e3 clasifica
familia63Cx. Justifica contraste limitado, no atribuir rama runtime completa
ni regla ISO universal/Idopte ejecutado a esta revisión. No ampliar85.

Nuevo open_report_policy: política predeterminada OUTER_9000 intacta en
open/open_report/confirmation/session/MF. Solo el query vacío opta por
OUTER_9000_OR_MATCHING_63CX: exterior9000 o63Cx, TLV canónicos99/8E/MAC8,
SSC correcto y MAC obligatorio; si exterior63Cx exige DO99 exactamente igual
**después de verificar MAC y antes de descifrar/publicarSW/datos**. Mismatch
fijo authenticated_status_outer_mismatch/MACverified no publica innerSW.
No transformar63Cx en9000/login: helper sigue exigiendo datos0 y63Cx x>0;
63C0 bloquea, estados desconocidos/bareSW/cuerpo sinMAC fallan. Exterior6983/
6982/61xx/6Cxx siguen fuera de política. Sin nuevos comandos/fallback/retries/
variantesCLA/Le/ref/MF, guard/reset/límites/permisos/privacidad intactos.
Report añade response_policy por fase; estructura/exterior no autenticados.

Pasaron **12 codec**, **22 probe**(13nativos+3bootstrap real/OpenSC simulado+
6wrapper), **7confirmation** y **6session** por dependencia del codec. Vector
warning sintético independiente OpenSSL CLI para63C0/7/F, todos128bits alterados,
bare63C7/DO99 distinto conMACválido/missingMAC/SSC viejo/política inválida,
default rechaza warnings. Fixture bootstrap real prueba exterior63C7/MAC,
guard/helperMF siguen9000; query cero/datos/mismatch/MACbad detienen y cierran
sin extraAPDU. ASan/UBSan28grupos codec/nativo/bootstrap detect_leaks0,
sin prueba de fugas ni libs precompiladas instrumentadas. Helper compilado.
Driver/submódulo/observador/flake/lock intactos; otras suites no repetidas.

**Siguiente intervención:** misma JCOP4, firmador/otros clientes cerrados,
`nix develop -c make inspect/jcop4-channel-pin-context READER=0`, una sola vez,
sin introducir PIN. Repetición justificada por corregir aceptación de respuesta
observada, no ensayar variantes ni repetir canal por inercia. Revisar query
MAC/innerSW/response/result/cierre; no preparar credencial antes de comprobarlo.
JCOP4 firma/RSA3072/antiguoVERIFY6982 pendientes. Git sololectura: commit de
tools2/tests2/AGENTS/README/docs, sin archivo nuevo/captura/build. No comando
PCSC/lector/tarjeta/Idopte/PIN/GUI por agente.

**Revisión anterior: MF protegido confirmado; respuesta de PIN vacío aún sin validar.**
Principal `c3f2bf7`, OpenSC `3dfef6704`, limpios al comenzar. Informe del usuario
`local/jcop4-channel-pin-context-VoAbbMum/report.txt`,2026-10-14T06:19:03Z
(14Oct00:19CR): build/context/driver/connect/lock/reset inicial0, ATR/T1/límites
correctos; D004/control/READ y canal íntegro confirmado. SELECT MF cifrado
SSC3/4 devolvió respuesta autenticada9000. Es evidencia real de comandoDO87,
además del READ protegido anterior. Query vacío1 transporte0, response=
invalid_stop, pin_context_result=not_confirmed_stop; no SW interno/forma/
MAC/causa disponibles. Finalreset/unlock/disconnect0, guard_blocked0,
command_count10/helper1. **No atribuir6982, PIN incorrecto, decremento,
pérdida de canal ni rechazo de tarjeta a ese marcador agregado**. Sin PIN
suministrado; el viejo VERIFY6982 con credencial es evidencia distinta.

Instrumentación de diagnóstico únicamente: open_report en codec devuelve
enum de argumento/estado, tamaño, SW exterior, SSC, TLV87/99/8E, MAC,
descifrado, padding, capacidad o aceptación, y MAC not_checked/failed/verified.
API open anterior conserva aceptación/limpieza. MAC verificado no equivale
a respuesta aceptada si falla padding/capacidad. Helper agrega para MF/query
response_size<=258, outer_sw, leading_object (absent/encrypted87/unsupported85/
status99/other_uninterpreted), decode_result/mac_check. Exterior/estructura
**no autenticados**, no presentar outer_sw como estado interno. No valores
de tags desconocidos, DO99 sin validación, raw/plain/cipher/MAC/keys/hash.
Mantiene mismos APDU/CLA/Le/P1/P2/ref/fases/budget/reset/guard/permisos;
85 y respuesta plaintext siguen rechazados. Sin fallback ni nuevo login.

Revisión estática mismo libt_ias/hash:240f15 transporte seguido de slot40
en240f21; decoder2437db..2437e9 admite85/87 y exige99 en2437fe..243812.
No demuestra rama runtime ni forma de respuesta actual: **no habilitar85 ni
ignorar MAC por esa lectura estática**. Buffer real anterior fue borrado;
no se puede recuperar causa desde reporte ni justificar variante por intuición.

Pasaron **11 grupos codec**, **21 probe** (12 nativos+3 OpenSC bootstrap
simulado+6 wrapper), **7 confirmation** y **6 session** por dependencia del
codec modificado. Fixtures distinguen SW exterior6982/63C7 sin MAC, respuesta
corta9000,85, TLV malformado, MAC malo y padding malo con MAC válido; ningún
SW interno/plaintext en fallos, mismas paradas/cierre y cero APDU adicionales.
Fallo inicial de compilación fue advertencia de underflow en fixture al alterar
longitud MAC después de vaciar respuesta; corregido orden/assert, no hardware.
Helper real compilado. ASan/UBSan26 grupos codec/nativos/bootstrap pasaron
detect_leaks0; bibliotecas precompiladas sin instrumentar/no prueba de fugas.
Driver/submódulo/observador/flake/lock intactos; otras suites no repetidas.

**Siguiente intervención:** misma JCOP4, otros clientes/firmador cerrados,
`nix develop -c make inspect/jcop4-channel-pin-context READER=0`, una sola vez,
sin PIN. Repetición para recuperar metadatos omitidos de la respuesta fallida,
no para reconfirmar D004/MF ni ensayar variantes. Revisar query outer_sw/size/
leading_object/decode_result/mac_check/response y cierre antes de otro comando.
Login/firma JCOP4/RSA3072/VERIFY6982 siguen pendientes. Git solo lectura:
commit de tools2/tests2/AGENTS/README/docs, ningún archivo nuevo/captura/build.
Ningún comando PCSC/hardware/Idopte/PIN/GUI por el agente.

**Revisión anterior: consulta MF/PIN vacío bajo canal preparada, aún sin hardware.**
Principal `81d68ae`, OpenSC `3dfef6704`, limpios al comenzar. Última evidencia
de tarjeta sigue27daTsCp/READ137 íntegro/cierre0. No repetir ese diagnóstico.
Sin PCSC/hardware/GUI/PIN/Idopte ejecutado por el agente; análisis estático y
software. Tipo/despacho exactos de Idopte siguen pendientes: categoría fábrica
cache+48 (1->constructorCP71 en3fc469) NO es descriptor perfil+8/token+380.
La rama de shift3bd69d..3bd727 parte de tipo4 y crea8 según objetos; no prueba
tipo de esta JCOP4 ni que su login retireSM. No usar esos números como JCOP.

Nuevo **make inspect/jcop4-channel-pin-context READER=0**, flag explícito
--pin-context en helper/lanzador; no se activa por env, target antiguo intacto.
Diagnóstico alternativo libre, no réplica de rama interna Idopte. Precedentes
reales en esta misma tarjeta: SELECT MF00A40000/data3F00/caso4/Le2569000 en
captura pública post-auth-mf, VERIFY vacío1/63C7 y metadatos referencia global1.
Tras canal confirmado/D004137 idéntico, conserva conexión/lock/keysSSC2 y
envía solo SELECT MF cifrado (SSC3/4), luego VERIFY1 **sin credencial/DO87/DO97**
(SSC5/6) si la respuesta MF está autenticada y SW interno9000. Esta combinación
bajoSM aún no está probada: es la duda nueva, no se infiere por el precedente
plaintext ni por tipo Idopte. No elegir/barrer P2 ni reabrir canal/fallback.

SELECT inner00A40000/data3F00/Le256 ->CLA0C/cuerpo32/wire38/outerLe256;
query inner00200001/caso1 ->CLA0C/solo8E08MAC8/wire16/outerLe256.
Guard exige flag/fase/ATR/T1/límites/budget24 y comparación exacta del frame
local (también ciphertext/MAC), forma/tags/refs fijos. MAC/padding antes de
SW/control, MF non9000/61xx/6Cxx/transporte/MAC/error detienen antes de query.
Control MF opaco/no registrado. Query debe devolver cero datos y SW9000 o
63Cx con x>0;63C0/6983/otros detienen.9000 se registra como status_9000_no_pin_
supplied, no prueba de login efectuado por helper. No PIN real/cambio/reenvío,
otrosSELECT/reads/objetos/firma ni downgrade aplain. No imprime contadores como
máximos ni afirma reset por firma. Fin siempre borra estado y reset/unlock/
disconnect; timeout/reset ausente/fallido requiere reinsertar antes de otro uso.

Reporte privado **local/jcop4-channel-pin-context-*/report.txt**0700/0600:
metadata actual más context_select_transport/response/sw/result,
pin_query_transport/response/sw/result y pin_context_result. Sin control,
ciphertext/plaintext/MAC/keys/identificadores, stdin vacío/debug0/trace aislada,
timeout45sTERM+5s kill/core0. Channel confirmado por sí solo no basta para
éxito de contexto: exigir pin_context_result positivo/helper0 y cierre0.

Pasaron **20 grupos probe**: nativos11+OpenSCreal bootstrap/transmit3+wrapper6,
software peer con CBC/ECB/CMAC independiente, SSC3/4/5/6/orden, gatingD004,
fallos/status/datos inesperados y guard antes de backend. Fixtures usan11
APDU nativas/10 realbootstrap; máximo24 sigue, no exigir ese número en tarjeta.
ASan/UBSan14 pasaron detect_leaks0; libs precompiladas no instrumentadas/no
prueba de fugas. Helper real compilado; componentes cripto/session y suites
sin cambios vigentes/no repetidas. Driver/submódulo/observador/flake/lock intactos.

**Siguiente intervención:** misma JCOP4, firmador/otros clientes cerrados:
`nix develop -c make inspect/jcop4-channel-pin-context READER=0`, una sola vez,
**sin introducir PIN**. No ejecutar el target previo de canal por inercia.
Revisar MF/query autenticados/SW/cierre antes de ofrecer login real. Consulta
vacía no autentica ni determina formato PIN/causa6982. JCOP4/RSA3072 pendientes.
Git solo lectura: commit Makefile/helper/lanzador/tests2/AGENTS/README/docs;
ningún archivo nuevo/captura/build/submódulo. No hardware por el agente.

**Última revisión: retención del canal confirmado probada solo en software.**
Principal `75e3ec5`, OpenSC `3dfef6704`, limpios al comenzar. Última tarjeta
sigue27daTsCp/05:40:01Z: canal confirmado/READ137 idéntico/cierre0. No repetir
canal/D004/estado ni pedir PIN. Sin PCSC/hardware/GUI/Idopte ejecutado por agente.

Nuevo jcop4_ca_confirm_d004_session en confirmation.h: salida opcional de estado
SM solo tras MAC/padding/SW9000/tamaño137/comparación completa. Destino debe
estar inicializado INVALID; éxito IDLE/SSC2 y keys del acuerdo de esa conexión.
Fallo deja destino nuevo borrado/INVALID; si ya está IDLE o WAITING, se rechaza
antes de I/O y se preserva para que su dueño lo cierre (no sobrescribir sesión).
Dueño conserva misma conexión/lock, invalida tras transporte/reset/reconnect,
borra keys/SSC y termina sesión remota. Componente cripto no puede imponer
identidad de conexión. Wrapper anterior confirm_d004 pasaNULL: helper hardware
sigue sin devolver estado, mismasAPDU/guard/reset; ningún login/VERIFY añadido.

Nuevo make test/jcop4-channel-session: **6 grupos**, peer RFC/software, handoff
solo confirmado, claves comparadas en memoria con peer independiente, SSC2,
SELECT cifrado **solo simulado** con ECB/CBC/CMAC independienteSSC3/4,
interleaving/replay rechazados, fallos setup/transporte/MAC/SW/contenido/IV,
entrada/nulos/borrado y destinos activos IDLE/WAITING sin I/O, wrapper intacto.
No determina P1/P2/contexto real del PIN; SELECT del fixture no es comando de
hardware autorizado/preparado. confirmation7/probe12 pasaron de nuevo por
refactor, helper real compilado; ASan/UBSan6 pasaron detect_leaks0 (biblioteca
OpenSSL no instrumentada, no prueba de fugas). Resto de suites sin cambios
no repetidas, driver/submódulo/observador/flake/lock intactos.

Revisión estática adicional misma libt_ias.so/hash2380540...:
3a02f0 lee descriptor token+380/campo+8, mismo selector que385530/3fddd0
(tipos1/2/8, no generaciones JCOP). Ruta3c1bf0/3c1e0c..3c1e46 abre anónimo
por590 si385530, consulta598=369870 y pasa3c1840->3c11b0. Esta lee referencia,
para bit80 hace3ac420 antes de consultar5e8=3755c0;375606 llama368b90 ANTES
de decidir estado cacheado/queryvacío. Rama global puede pasar39cd30->37d690
retirandoSM por450=384c60. Sin omisión del estado acreditada en esta rama;
no declarar que la tarjeta la ejecuta ni resolver contradicción aparente por
conjetura. Faltan tipo/runtime/despacho/ref global/local/contexto efectivo.
39ce80/39cf08 aplica flag a primerobjeto si tipo8,383b70 usa metadatos;
tabla4b98e8 índice2->383f39 copia bytes, pero límites/padding/rama de tipo8
siguen pendientes: no adaptar PIN ni equiparar automáticamente enumsOpenSC.
Reenvío3c1690..3c16d4 confirmado, no trasladar ni probar PIN fallidos.

**Siguiente trabajo local:** resolver perfil/despacho/contexto/formato y definir
integración que retengaSSC en la misma conexión, primero probar selección/
contexto sin PIN si hay precedente aplicable. No nuevo comando hardware aún;
no reutilizar la sesión del diagnóstico ya cerrada, no ofrecer VERIFY por
inercia. FirmaJCOP4/RSA3072/causa6982 pendientes. Git solo lectura: commit
incluye nuevos tests/jcop4-channel-session.c y scripts/test-jcop4-channel-session.sh,
confirmation.h/Makefile/documentos; sin capturas/binarios/build/submódulo.

**Último resultado: canal JCOP4 confirmado mediante READ protegido íntegro.**
Principal `d462900`, OpenSC `3dfef6704`. Tres documentos pendientes de la
revisión interrumpida del intento sin tarjeta; sin cambios de código del usuario.
Informe nuevo `local/jcop4-channel-27daTsCp/report.txt`, 2026-10-14T05:40:01Z
(13Oct CR): build/context/driver/connect/lock/reset inicial0, ATR/política
correctos, directory0/AID anunciado1/select aplicación0, SELECT/READ D0040/9000.
stage4/outcome8(READ_CONFIRMED), transporte0/SW interno9000/MAC verificado1,
**content_size137/size_equal1/first_block_equal1/tail_equal1**,
channel_result=protected_d004_read_confirmed_no_login. Ocho APDU, guard0,
reset final/unlock/disconnect0, interrupted0/helper0. Confirmación completa
del primer intercambio en esa sesión; no identidad certificada/PIN/login/firma.
El comando READ no tiene datos cifrados: DO87 en comandos aún solo probado en
software, respuesta cifrada sí comprobada con igualdad completa en tarjeta.
No repetir canal/D004/estado por inercia; la sesión ya fue reseteada y sus
claves/SSC borrados, no reutilizarla desde otro comando/proceso.

Usuario aclaró que CkRwMyGb se ejecutó accidentalmente sin tarjeta y volvió a
ejecutar el diagnóstico. Conservar ese informe como no ejecución del protocolo;
no hace falta pcsc_scan ni investigar servicio por ese intento ahora superado.

**Hallazgo estático para el próximo paso:** misma libt_ias.so/hash2380540...
368b90->39cd30 para rama global ordinaria;39cd47 llama CP71 slot4d8.
Relocación5d1140=37d690; esta rutina va a3a8370 sin quitar SM para tipos0/7/9,
pero para los otros llama slot450 primero. Relocación5d10b8=384c60: retira
pipe228b60, destruye objeto token+340 y lo pone0. Es **retirada de SM local**,
no prueba de reset/cierre remoto ni de intento PIN. No copiar SELECT raíz
genérico antes de VERIFY: puede descartar la capa en esa rama.3a8370 también
elige P2 de SELECT MF según slot538 (62->04,64->08,6F/otros->00 en rama
examinada), aún sin selector runtime. No atribuir esos números a generación
JCOP/ATR ni elegir/barrer P2. No declarar ejecutada esta ruta en esta tarjeta.
Tipo efectivo, referencia global/local, selección/cache/contexto y formato PIN
siguen pendientes para login. El éxito de D004 no resuelve por sí solo VERIFY6982.

**Siguiente trabajo local:** resolver la rama de contexto/login y diseñar
retención de un canal confirmado en la misma conexión/lock, con pruebas
sintéticas de SSC y selección. No pedir otro PIN, repetir hardware ni añadir
SM/SELECT/VERIFY al diagnóstico actual por inercia. Driver/submódulo/observador
intactos; suites codec10/confirmation7/probe12 y ASan25 vigentes/no repetidas.
Solo documentación/revisión estática esta vez; JCOP4/RSA3072/firma pendientes.
Git solo lectura: commit documental AGENTS.md/README.md/docs/jcop4.md, sin
capturas/binarios/build/submódulo. Todos los comandos de tarjeta los hace usuario.

**Intento anterior sin tarjeta: lector no enumerado, sin ejecución del protocolo.**
Principal `d462900`, limpio al comenzar. Informe del usuario
`local/jcop4-channel-CkRwMyGb/report.txt`, 2026-10-14T05:37:16Z (13Oct CR):
build/context/driver0, **reader_result=not_found**, guard_blocked0,
command_count0, interrupted0, helper1. No connect, reset ni APDU/MSE/GA/SM/
PIN/VERIFY. No rechazo de tarjeta ni evidencia a favor/en contra del IV nuevo.
Context0 es contexto OpenSC creado, no prueba de servicio PCSC/lector disponible.
No determinar desconexión física/pcscd inactivo/socket/índice desde este resumen.
Última evidencia de MAC real sigue siendo NY1kGaJQ, con mismatch anterior;
corrección AES(Kenc,SSC) probada en software pero todavía no en tarjeta.

Se propuso enumerar lectores, pero el usuario aclaró el accidente sin tarjeta
y el informe posterior27daTsCp confirmó el canal. **Esa intervención ya no es
necesaria**. No repetir canal por este informe ni modificar protocolo/Le/CLA.
No enumeración/activación PCSC por el agente.

**Última revisión: primer MAC real verificado; corregido IV derivado del SSC.**
Principal `6ab901f`, OpenSC `3dfef6704`, limpios al comenzar. Informe del usuario
`local/jcop4-channel-NY1kGaJQ/report.txt`, 2026-10-14T05:23:31Z (13Oct CR):
build/context/driver/connect/lock/reset inicial0, ATR/política correctos,
directory0/AID anunciado1/select aplicación0, SELECT y READ D0040/9000.
channel_stage4(READ), outcome7(CONTENT_MISMATCH), transporte0, SW interno9000,
**authenticated_response1**, guard_blocked0, command_count8, reset final/
unlock/disconnect0, helper1. MSE/GA pasaron sin payload inesperado y se verificó
MAC/padding de la respuesta protegida, pero tamaño o contenido no coincidió.
**El informe no distingue tamaño/contenido ni localiza diferencias**, no
afirmar137 recibidos, solo primer bloque cambiado o D004 mutable. No PIN/VERIFY/
firma. Evidencia de primer MAC, no confirmación completa/identidad/login.

Revisión estática del mismo /usr/lib/SCMiddleware/libt_ias.so/hash2380540...
corrige interpretación anterior **IV0**: respuesta243c9b..243cde cifra SSC
actual por245720 con IV cero, obtiene16 bytes;243cf5..243d0e usa ese resultado
como IV de2456a0/23ed60 para datos.243130 instala la misma Kenc en+58/+70;
245720->23ec10 cifra,2456a0->23ed60 descifra. Comando244009..2440a9 cifra
SSC||pad(datos) con cero;2440b6..2440e6 descarta primer bloque, equivalente
a **IV de datos=AES-ECB(Kenc,SSC actual)**. SSC incrementado antes de cada
mensaje. El cero visto era IV de esa operación previa, no el IV final del dato.
No ejecutar funciones del binario; no nuevo protocolo ni fallback.

Codec ahora deriva IV en comando cifrado/respuesta, después de verificar MAC
en respuesta, limpia IV temporal. READ actual no tiene datos en comando:
su cabecera/DO97/MAC/Le no cambian por esta corrección; MSE/GA/guard intactos.
Comparación completa137 sigue obligatoria; no ignorar header/primer bloque.
Report añade solo tamaño/size_equal y dos booleanos de comparación fija
primer16/resto, solo tras MAC/padding/SW interno9000, con comparación de regiones
solo para tamaño137. Sin bytes/hashes/offsets variables; no dato no verificado.

Pasaron codec10/confirmation7/probe12 grupos. Nuevos vectores CLI ECB/CBC/CMAC
reproducibles en **tests/jcop4-channel-vectors.py**, sintéticos, sin código
producción/PCSC/Idopte. Peer usa ECB EVP independiente para IV de respuesta.
Peer con IV cero/MAC válido se rechaza y muestra primer bloque distinto/resto
igual en fixture137; esa es una simulación, no evidencia del patrón real previo.
ASan/UBSan pasaron25 grupos nativos (10/7/6/2), detect_leaks0; bibliotecas
OpenSC/OpenSSL precompiladas no instrumentadas, no prueba de fugas. Helper real
compilado. KDF/primitivas/acuerdo/helper público/observador/driver/submódulo/
flake/lock intactos, suites sin cambios no repetidas. No hardware/PIN/GUI/Idopte
ejecutado por el agente. JCOP4/RSA3072/VERIFY6982 pendientes.

**Siguiente intervención:** misma JCOP4, firmador/otros clientes cerrados,
`nix develop -c make inspect/jcop4-channel READER=0`, una sola vez, sin PIN.
Justificada por IV de descifrado corregido y campos antes omitidos; misma
secuencia, no variar CLA/Le ni relajar comparación. Revisar contenido/etapa/
cierre antes de continuar. Git solo lectura: dar commit concreto, incluyendo
nuevo generador de vectores; no capturas/build/Idopte/submódulo a Git.

**Última revisión: corregida forma absoluta de SELECT EF.DIR del diagnóstico.**
Principal `278c54f`, OpenSC `3dfef6704`, limpios al comenzar. Informe nuevo
`local/jcop4-channel-6sS5ottZ/report.txt`, 2026-10-14T05:17:53Z (13Oct CR):
build/context/driver/connect0, ATR conocido, política T=1/límites superada,
lock/reset inicial0, directory_result=-1209, guard_blocked1, command_count0,
reset final/unlock/disconnect0, helper1. **Bloqueo local antes de transporte**,
no rechazo de tarjeta ni MSE/GA/SM/PIN/VERIFY. Hubo resets de sesión PCSC,
no APDU. Informe anterior ewlMTV5g/05:17:31Z build1 sin ejecución del helper;
compilación0 en el informe posterior, no atribuir causa del primero sin evidencia.

Fuente local iso7816.c:685..696 elimina MF3F00 de SC_PATH_TYPE_PATH;
sc_enum_apps(dir.c) solicita3F002F00 y el backend recibe P1=08/data2F00.
Guard aceptaba solo ruta completa3F002F00/P1=08, por eso negó la primera APDU.
Se añade exclusivamente SELECT absoluto2F00/P1=08, no selección relativa
sin MF ni archivos nuevos. Resto de límites/etapas/cierre/permisos intactos.

Pasaron **12 grupos** de make test/jcop4-channel-probe: seis nativos previos,
dos nuevos con sc_enum_apps/sc_select_file/read/FCI/ASN.1/format/transmit reales
de la biblioteca local y lector/contexto simulados, cuatro del lanzador.
El fixture real reproduce la ruta abreviada, llega a confirmación y comprueba
exclusión de otros FID/P1/canal/longitud/P2. Fallo inicial de ese fixture fue
mantener sc_transmit_apdu simulado sin normalizar CASE_2 a corto; al usar la
función real pasó, sin ampliar permisos de producción. Helper real compilado;
ASan/UBSan pasaron los ocho grupos nativos/integración, detect_leaks0; no
prueba de fugas ni instrumentación de la biblioteca OpenSC precompilada.
no PCSC/hardware/Idopte/PIN/GUI por el agente. Helper público/cripto/observador/
driver/submódulo/flake/lock intactos, suites sin cambios no repetidas.

**Siguiente intervención:** misma JCOP4 y otros clientes/firmador cerrados,
`nix develop -c make inspect/jcop4-channel READER=0`, una sola vez desde
terminal del usuario, sin PIN. Repetición justificada por corregir el bloqueo
anterior con0 APDU, no variante MSE/GA/Le ni reintento autenticado. Revisar
etapa/primer fallo/cierre antes de otro comando. Canal real/JCOP4/RSA3072/
VERIFY6982 pendientes. Commit del principal con AGENTS.md/README.md/docs,
tools/jcop4-channel-probe.c, tests/jcop4-channel-probe.c y su script; ningún
archivo nuevo/captura/build/submódulo. No eludir Git solo lectura.

**Última revisión: diagnóstico real de canal preparado, sin APDU desde esta sesión.**
Principal `3d4a0ba`, OpenSC `3dfef6704`, limpios al comenzar. Nuevo
`make inspect/jcop4-channel READER=0`: helper separado, driver default,
debug0, sin Idopte/binding/PIN/VERIFY/login/firma. Observador y driver intactos.
Exige ATR conocido en lector/tarjeta, T=1 y límites declarados envío>=139,
recepción>=256; son límites OpenSC, no prueba empírica/PIN-pad. Política propia
corta T=1 Le exterior256, DO97137, sin alternar172/256/copiar ReaderNames.
Lock, reset inicial, MF/EF.DIR/AID conocidos, SELECT D004/control/READ137 en
memoria, MSE/GA CLA10 sinLe y un READ protegido. Nueva lectura tiene propósito
de obtener punto efímero: no raw real conservado. No repetir D004 aislado.
Guard de transmisión por fase/ATR/protocolo, máximo24 APDU; bloquea antes del
backend PIN/cambios/escrituras/desconocidos/chaining/extended/offsets/SFI.
NO_RETRY_WL/NO_GET_RESP/NO_SM en todos los comandos. Primer bloqueo definitivo,
sin fallback. Exige MAC antes de descifrar, SW interno9000 y mismo contenido.
No devuelve sesión/claves; limpieza y reset final bajo lock, unlock/disconnect.
Confirmación solo válida con cierre completo/helper0; no prueba identidad,
permiso de PIN ni capacidad de firmar. No efecto probado sobre intentos PIN.

Lanzador una invocación, stdin vacío, sin debug/trace/preload/delegates heredados,
core dumps desactivados; timeout45s TERM+5s kill. Bloqueo PCSC puede impedir
cierre: ante timeout/reset final ausente o fallido, cerrar clientes y reinsertar
tarjeta antes de otra operación; no repetir ni PIN por inercia. Report/config
0600, directorio0700, solo metadatos; no datos/punto/claves/inventario personal.

**Única ejecución real intentada:** `local/jcop4-channel-6LvDBFn7/report.txt`,
2026-10-14T05:03:26Z (13Oct CR), build/context/driver0,
reader_result=missing_or_unsupported, guard_blocked0, command_count0, helper1.
No connect/APDU/reset/PIN; no rechazo de tarjeta/canal. Ese resultado agrupa
ausencia de lector/backend; futuras versiones separan not_found/unsupported.
Policy inicial es intención, no prueba de que T=1/límites pasaron.
Usuario sugirió enumeración para activar socket: pcsc_scan -r acotado devolvió
Service not available; socket existe pero conexión directa errno1/Operation
not permitted, sin PCSCLITE_CSOCK_NAME. Bus systemd también bloqueado. No
determina estado del servicio ni activación desde terminal del usuario. No
alterar sistema/eludir restricción/repetir canal desde esta sesión por inercia.

Pasaron **10 grupos adaptador/lanzador** (6 nativos+4 wrapper), transporte
simulado/software peer, guard/cierre/errores/timeout/permisos; timeout simulado,
no prueba temporal PCSC. ASan/UBSan6 detect_leaks0, no prueba de fugas.
Helper real compilado. Validador control D004 extraído a dh-control.h compartido;
las **92 comprobaciones helper público** pasaron de nuevo. Suites anteriores
sin cambios/no repetidas; submódulo/driver/observador/flake/lock intactos.

**Siguiente intervención:** misma JCOP4, otros clientes/firmador cerrados,
`nix develop -c make inspect/jcop4-channel READER=0` desde terminal del usuario,
una sola vez, sin PIN. Revisar primer fallo/etapa y cierre antes de decidir
otros comandos. Canal real aún no confirmado; JCOP4/RSA3072/VERIFY6982 pendientes.
Git autorizado pero .git solo lectura: no eludir; dar comando de commit con
los siete archivos nuevos (dh-control.h, channel-probe.c, tres scripts y dos
tests), Makefile/dh-info.h y documentos. No capturas/build a Git.

**Última revisión: secuencia de confirmación probada con transporte sintético.**
Principal `e9951da`, OpenSC `3dfef6704`, limpios al comenzar. Sin hardware,
Idopte ejecutado, PIN, GUI ni nuevas capturas. D004 real LbbFD51N sigue vigente;
no repetir inventario/estado/D004 por inercia. No dato o secreto real en tests.

368190 llama acuerdo slot460 antes de instalar SM slot448/384820. Si token+340
ya tiene SM retorna sin reinstalar;384c60 lo retira/borra el estado local.
No identificar esos campos con contadores persistentes de tarjeta.
240e30 clona comando, prepara porslot38, transmite y verifica respuesta por40.
**Nuevo hallazgo: Le exterior del READ protegido no es siempre256.**
24483c..2448a0: descriptor+0C=0 ->256;=1 ->Le original+(27+MACsize), salvo65536.
Para READ137/MAC8 es172. DO97 sigue137 en ambos; respuesta sintética cifrada164.
240960 guarda r8 endescriptor+0C.36828f..3682bc calcula ese booleano a partir
de FEATURE_VERIFY_PIN_DIRECT tag06 (consulta2266e0) y bit1 de compatibilidad
(226260).2266e0 consulta capacidades vacías42000D48, respetando
SecuritySuite/DisablePinPad; /etc/idoss.conf declara DisablePinPad0, pero esto
**no prueba presencia/capacidad de PIN-pad**.226260/226070 usan una tabla de
configuración **ReaderNames**, no inferir sus flags desdeATR/nombre de lector.
No se ejecutó control lector ni PIN-pad. Disponibilidad tag06/flags actuales
y política efectiva de Le pendientes; no alternar172/256 sobre hardware.

Nuevo **tools/jcop4-channel-confirmation.h**: secuencia candidata por callback,
sin PC/SC/Idopte/PIN. Requiere mismo contexto exclusivo ADF/D004, control y
lectura137 ya validados por futuro llamador; no descubre ni lee inicial por sí.
Genera acuerdo efímero, MSE una vez, GA una vez y READ protegido una vez.
Política exterior172/256 explícita, sin default/fallback; READ caso4 corto19.
MSE exigeSW9000 sinpayload; GA admite sinpayload o plantilla vacía7C00, **no
observación real de GA**. Nonce/token/otro payload detiene, nunca se ignora.
No retries/GET RESPONSE/6C/PIN-status/VERIFY/firma/seguimiento. Solo devuelve
éxito con MAC/padding válido, SW interno9000 y contenido137 idéntico en memoria.
MAC válido con SW6982 o contenido distinto sigue fallo. Reporte solo enums,
transporte/SW/booleano; sin buffers/claves/datos. Limpia keys/SSC/temporales
en toda salida, no devuelve sesión reutilizable. Futuro llamador debe cerrar/
resetear también sesión de tarjeta: limpieza local no lo hace.

Nuevo **make test/jcop4-channel-confirmation** pasó **6 grupos**: peer RFC5903
de software/efímero local, armado independiente CBC/MAC/TLV de respuesta,
orden exacto3 comandos y Le explícito/GA vacío; errores transporte/buffer en
cada etapa; setup69xx/61xx/6Cxx y payload;9000 solo/no MAC,64bitsMACalterados,
padding malo conMAC válido; SW interno error/datos distintos/cortos/ausentes;
entrada/curva/longitud/política/nulos con0 intercambios y reporte limpio.
ASan/UBSan pasaron (detect_leaks0 por ptrace, no prueba de fugas); lanzador
desactiva core dumps. Fallo inicial del fixture se corrigió: plantilla GA de
éxito sobreescribía el SW fallido simulado. No error de tarjeta ni de PIN.
KDF5/primitivas7/codec9/acuerdo8/helper92/lanzador95/observador162/firma6/
operación12 intactos/no repetidos. Driver/submódulo/helper/observador/flake/lock
sin cambios; se mantienen los bloqueos de observación previos.

**Siguiente trabajo:** justificar Le mediante capacidades/compatibilidad del
lector actual, preparar adaptador real acotado con control de ATR/AID/D004,
debug0, lock y cierre/reset de sesión. Aún no hay make inspect de canal ni
se envió MSE/GA/SM real: no dar por confirmado canal/identidad/PIN/firmaJCOP4.
No repetir estados/lecturas sin propósito nuevo; RSA3072/VERIFY6982 pendientes.
Git autorizado pero .git solo lectura impide index.lock, no eludirlo. Commit
incluye tres archivos nuevos de confirmation.h, tests y lanzador.

**Última revisión: acuerdo efímero P-521 y MSE/GA candidatos probados offline.**
Principal `458007f`, OpenSC `3dfef6704`, limpios al comenzar. Última tarjeta:
LbbFD51N/02:22:26Z, punto D004 válido; no repetir diagnósticos D004/estado/
inventario por inercia. Sin hardware, Idopte ejecutado, PIN, GUI o captura.
Hash actual libt_ias.so coincide:2380540cd85316306233aa2baf640019a848606ce72bb5692cbebbb111542dc4.

Revisión estática cierra una ruta de transporte **condicionada al pipe activo**:
2a8a20 crea adaptador22fec0/vtable5cb640; slot10 ->22ffe0 ->22cd40
->CCard slot10 (vtable5cb5a0/22d950).22d950 llama serializador227d80;
22d8a0 entrega ese buffer al backend slot10/22afc0 ->SCardTransmit.
El serializador copia los cuatro bytes de cabecera sin normalizar CLA.
Constructor227600 pone Le0; append228530 agrega cuerpo sin cambiarlo:
en la rama GA372740..37283e **Le ausente**, no Le256. Lc139 cabe en ruta
corta y en fragmentador2a09a0 si capacidad>=139; entonces CLA10 se conserva.
**No sustituir por CLA00, añadir Le ni probar ambas por inercia.** Falta confirmar
el perfil/cadena activos en esta tarjeta; no captura GA ni prueba de éxito.
Referencias completas y condiciones en docs/jcop4.md.

Nuevo **tools/jcop4-channel-agreement.h** sin PC/SC/Idopte/PIN: valida solo
SEC1 P-521 no comprimido133 bytes, coordenadas canónicas<p/punto en curva/no
infinito. Entrada normal genera clave efímera con EVP_EC_gen; importación y
ECDH mediante EVP, validación completa del peer y par local. Se solicita
sin KDF del provider/cofactor0/padding1, exige X compartida66 y aplica KDF
candidata existente. Solo devuelve punto público del terminal y claves
**pendientes**, no secreto X ni escalar privado; limpia temporales/libera key.
Error borra toda la salida; sin fallback/reuso/escalares fijos en entrada normal.
Implementación interna con key inyectada permite vectores solo en pruebas.

Constructores locales candidatos: MSE17/caso3 con OID CA/AES128; GA144/caso3,
CLA10/Lc139/7C8188/808185/punto133, **sin Le**. No están conectados al hardware
ni al driver. Preparación/9000 de MSE/GA no prueban canal: exigir posteriormente
respuesta protegida con MAC correcto antes de habilitar PIN. Aún no hay ese
orquestador/verificación en tarjeta; identidad de D004 y ruta C_Login pendientes.
Observador y sus bloqueos anteriores intactos, también READ4141/PIN/MSE/GA/SM.

Nuevo **make test/jcop4-channel-agreement** pasó **8 grupos**: vector ECDH
RFC5903§8.3 y KDF esperada independiente hashlib, simetría/SSC0, secreto X
con cero inicial, claves efímeras distintas/acuerdo con peer de software,
todos los bits de punto alterados/tamaños/encodings/nulos, coordenadas fuera
de campo y alias x+p/y+p, clave local pública/inconsistente/0/otra curva/tipo,
provider no disponible, formato/tamaños/capacidad/borrado de MSE/GA locales.
ASan/UBSan pasaron con detect_leaks=0 (ptrace impide LeakSanitizer, no prueba
de fugas). KDF5/primitivas7/codec9/helper92/lanzador95/observador162/firma6/
operación12 intactos/no repetidos. Flake/lock/helper hardware/driver/submódulo
sin cambios. Fuentes primarias OpenSSL/RFC enlazadas; no atribuir perfilCR alRFC.

**Siguiente trabajo:** comprobar qué pipes/contexto usa el establecimiento
anónimo y diseñar el diagnóstico sin PIN que valide el primer MAC con una
consulta conocida. No enviar todavía MSE/GA/SM ni pedir PIN. No variar CLA/Le,
ampliar observador o repetir tarjeta por inercia. JCOP4/RSA3072/VERIFY6982 siguen
pendientes. Git autorizado, pero .git montado solo lectura no deja index.lock;
no eludirlo. Commit debe incluir los tres nuevos archivos agreement.h,
tests/jcop4-channel-agreement.c y scripts/test-jcop4-channel-agreement.sh.

**Última revisión: codec candidato de mensajes protegidos probado offline.**
Principal `98b4f86`, OpenSC `3dfef6704`, limpios al comenzar. Última tarjeta:
LbbFD51N/02:22:26Z, punto D004 válido. No repetir D004, estados ni inventario.
Esta revisión no ejecutó hardware, Idopte, PIN, GUI ni captura autenticada.

Nuevo `tools/jcop4-channel-messages.h`, separado del helper y del driver:
contexto local con claves/SSC y estados INVALID/IDLE/WAITING; constructor de
cabecera y cuerpo protegido, sin serializar Lc/Le de transporte. Subconjunto
conservador: CLA básica00 ->0C, datos87/01, Le97 de un byte, MAC8 en8E;
respuesta87 opcional,99/2,8E/8 y SW físico9000. No acepta85 ni otras variantes.
CBC usa IV0 de la rama revisada; MAC de comando sobre SSC||pad(cabecera)
y cuerpo87/97, con padding adicional solo si hay cuerpo. Respuesta:
pad(SSC||87 opcional||99). SSC se incrementa antes de cada mensaje.
**Verifica MAC antes de descifrar y antes de publicar datos/SW**; un SW interno
autenticado6982 se devuelve como error, no se transforma en9000.

Toda respuesta inválida borra claves y descarta el contexto. Overflow nunca
reinicia SSC. Un error local de construcción conserva estado sin enviar;
no se pueden intercalar comandos mientras se espera respuesta. El futuro
llamador debe serializar acceso e invalidar ante error de transporte; el codec
no hace PC/SC ni locks ni permite asumir que init establece un canal.
Límite3584/TLV81/82 es local, no soporte APDU/RSA3072 ni autorización de tarjeta.

Nuevo `make test/jcop4-channel-messages`: **9 grupos pasaron**, fixtures
sintéticos independientes Python/OpenSSL CLI y hashes hashlib; comandos con
datos/Le/sin cuerpo, tamaños128/384/3584, respuestas válidas/in-place/error
interno, todos los bits alterados, padding inválido con MAC válido, TLV
truncados/no canónicos/duplicados/reordenados, replay/estados/capacidades/nulos,
overflow y borrado. ASan/UBSan también pasaron con detect_leaks=0: LeakSanitizer
falló por ptrace del entorno; **no se verificaron fugas**. KDF5/primitivas7/
helper92/lanzador95/observador162/firma6/operación12 intactos, no repetidos.
Flake/lock/observador/helper hardware/driver/submódulo sin cambios.

**Siguiente trabajo:** terminar la serialización final CLA/Le de GA y el acuerdo
efímero local; definir verificación del primer intercambio protegido antes de
intentar tarjeta. No nuevo D004/PIN/MSE/GA/SM real todavía. Identidad de D004,
perfil/ruta C_Login y causa VERIFY6982 siguen pendientes; no firma JCOP4 ni
RSA3072 acreditada. Git autorizado, pero .git montado solo lectura impide
crear index.lock; no eludirlo. Incluir explícitamente los nuevos archivos
messages.h, tests/jcop4-channel-messages.c y su lanzador en el commit.

**Última revisión: primitivas AES/CMAC/padding/SSC probadas offline.**
Principal `e6eaee7`, OpenSC `3dfef6704`, limpios al comenzar. Última tarjeta:
LbbFD51N/02:22:26Z, punto D004 válido; **no repetir D004 ni estados/inventario**.
Esta revisión sin hardware, Idopte ejecutado, GUI, PIN o captura autenticada.

Mismo libt_ias.so/hash, revisión estática:384820 instala AES por240ae0/2435b0
con pipe228aa0. VtableAES preparación243e60/respuesta2436a0: SSC16 incrementado
BE antes de cada mensaje. ComandoCLA|0C y87/97/8E; respuesta85/87,99/8E en ruta
revisada, no decoder/política universal. **244083 IV16ceros ->245720 ->23ec10**,
CBC sin transformación visible SSC->IV en esta rama. MAC246370/245900 usa
subclaves/desplazamiento/Rb87 (CMAC), configuraciónMAC8; padding24719080+ceros.
Entrada MAC/respuesta/protocolo runtime aún sin completar, no asumir variante
moderna AES(SSC) como IV. Fragmentador2a09a0 delega intacto si cabeLc, o marca
intermediosCLA|10 y últimoCLAoriginal. **No prueba normalizar GA CLA10 a00**;
pipeactivo/serialización/Le final pendientes. No mandar GA por copiar cabecera.

Nuevo **tools/jcop4-channel-primitives.h**, sin PC/SC/Idopte/PIN/APDU/canal:
SSC128 next con overflow rechazado sinreset, método2padding/unpad, AES128CBC
IVexplícito/sin padding OpenSSL, CMAC16 y verificaciónprefijo8 CRYPTO_memcmp.
CBCsize1..4096 múltiplo16/capacidadcomprobada, resultado solo tras éxito completo;
fallo deja output intacto (debe descartarse). CMAC borra output en fallo.
**CBC/unpad no autentican**: futura capa debe verificarMAC sobreTLV/SSC correctos
antes de plaintext/SW, no está conectada todavía. No otras curvas/algoritmos,
no clave/session/PIN real. Helper hardware/observador/driver/submódulo intactos.

Nuevo **make test/jcop4-channel-primitives** pasó **7 grupos**, CBC/CMAC
vectores primarios NIST, IVestándar vs0 primerbloqueECB, in-place/negativebuffers,
64bitsMACalterados/mensajealterado, frontera4096 válida paraCBC/CMAC,
SSCcarry/overflow y padding0..4095/incrustado/
malformado. Límite4096 local no soporte APDU/RSA3072 probado. libcrypto ya enflake,
sin lock/inputs/sistema. KDF5/helper92/lanzador95/observador162/firma6/operación12
intactos, suites vigentes/no repetidas. Fuentes y límites en docs/jcop4.md.

**Siguiente trabajo:** constructor/verificador de mensajes protegidos y respuesta
MACcomprobada antes de descifrado/aceptaciónSW; terminar serializaciónGA final.
No D004 repetido ni nuevo PIN/MSE/GA/SM de tarjeta todavía. Identidad/política/
ruta C_Login y tipo interno siguen pendientes; JCOP4/RSA3072/VERIFY6982 no resueltos.
Git autorizado: el intento de añadir los archivos falló por .git montado
solo lectura; no eludirlo. **Agregar explícitamente los nuevos3 archivos de
primitivas/pruebas/lanzador**, git add -u no los incluye.

**Último resultado: punto D004 validado; KDF candidata CA/AES128 comprobada offline.**
Principal `c85aa16`, OpenSC `3dfef6704`, limpios al comenzar. Informe usuario
`local/jcop4-dh-parameters-LbbFD51N/report.txt`, **2026-10-14T02:22:26Z (13 CR)**:
build/inventario/conexión/ADF correctos, SELECT/READ0/9000,137/discriminador18.
**affine_xy_at_offset5/secp521r1/valid_non_infinity/recognized_point_only_no_channel**,
unlock0/proceso0. Validación matemática confirmada; no autenticidad/variante
runtime. Helper sin PIN/VERIFY/MSE/GA/SM/firma; no coords/header/raw/hash guardados.
**No repetir D004, inventario o estados para confirmar este resultado.**

Continuación estática en mismo libt_ias.so/hash: **371ca0 no es solo lector**,
su rama ECC incluye cálculo/MSE/GA/derivación. **2409a0 copia descriptor/SSC/
claves** para instalar después; no deriva. EC_POINT_mul, X compartida serializada
unsignedBE66 (235cf0/2357e0 ceros iniciales). ConstructorMSE37253b:
002241A4 y800A04007F00070202030202, OID CA/ECDH/AES128. ConstructorGA372740
CLA10/INS86/P100/P200, punto local no comprimido en80/7C; **codificación final
CLA/Le/fragmentación pendiente**, no copiar esa CLA ni la00 del ZIP.2278c0 ->
CP71slot10/3c3f00 ->2a9170 ->objeto transporte. MSE/GA exigen9000; esta rama
no consume nonce/token de GA para la derivación. SHA1 mediante EVP funciones,
Kenc=SHA1(Z66||00000001)[:16], Kmac=SHA1(Z66||00000002)[:16], SSC16ceros;
368190 configura AES1/MAC8 e instala descriptor. OpenSC sm-eac.c:1431 espera
nonce/token: integración no automática. Fuentes primarias OpenPACE/límites en
docs/jcop4.md; BSI PDF403 no recuperado, no atribuir texto no leído.
**No prueba ruta runtime de esta tarjeta ni versiónCA formal/PACE/CAN**;
C_Login intermedio/tipo interno aún pendientes. Sin ejecutar funciones del
módulo, login, captura autenticada, hardware, GUI o PIN por el agente.

Nueva pieza **tools/jcop4-channel-kdf.h**, aislada, sin PC/SC/Idopte/PIN/APDU/
ECDH/SM/firma. Exige66, SHA1/contadoresBE1/2 y primeros16, SSC0; commit conjunto
de las claves, salida borrada en fallo, temporales borrados/sin fallback.
No integrado al helper ni driver. Nuevos tests/jcop4-channel-kdf.c y
scripts/test-jcop4-channel-kdf.sh; **make test/jcop4-channel-kdf** pasó5 grupos:
3 vectores sintéticos independientes hashlib, ceros iniciales/contadores/
truncamiento, input intacto/SSC0, tamaños0..67 salvo66/nulos y salida borrada.
No datos reales/secretos de sesión; salida soloPASS. OpenSSL ya enflake,
sin lock/inputs/instalaciones. Helper92/lanzador95/observador162/firma6/operación12
intactos, suites vigentes/no repetidas. Driver/submódulo sin cambios.

**Siguiente trabajo del agente:** resolver serialización final de GA y revisar
AES/IV/MAC/SSC/padding/respuesta protegida; pruebas locales de acuerdo/ensobrado
con datos sintéticos y MAC comprobado antes de canal. **No nuevo comando de
hardware ni PIN solicitado todavía**, no repetir D004 por inercia. Observador
conserva todos los bloqueos previos. JCOP4/RSA3072/VERIFY6982 pendientes.
Git autorizado: intento de git add falló por .git montado solo lectura;
no eludirlo. **Agregar explícitamente los 3 archivos nuevos de KDF/pruebas/
lanzador**, git add -u no los incluye.

**Último resultado: discriminador P-521 coincide; prefijo04 era supuesto incorrecto.**
Principal `ab46551`, OpenSC `3dfef6704`, limpios al comenzar. Informe del usuario
`local/jcop4-dh-parameters-0lUpuwV0/report.txt`, **2026-10-14T02:17:16Z (13 CR)**.
Build/inventario/conexión/ADF correctos, SELECT y READ D0040/9000,137 bytes.
**dh_curve_selector=matches_p521_candidate; dh_point_prefix=different_uninterpreted**.
Punto no comprobado/unrecognized_layout_stop, unlock0/proceso1. No valor byte4
registrado ni coordenadas guardadas. Helper sin PIN/VERIFY/MSE/GA/SM/firma.

La condición byte4=04 era una suposición incorrecta de SEC1 desdeposición4,
no una regla confirmada del perfil. Se revisó rama18 de Idopte371ca0:
3720e0 ->371fd8;372060 toma bytesdesde5 y BN_bin2bn separa `(longitud-5)/2`,
236860 llama EC_POINT_set_affine_coordinates. **No usa byte4 como prefijo**
al leer coordenadas; no implica irrelevancia para el protocolo/canal completo.

Decoder corregido en tools/jcop4-dh-info.h: exige137/discriminador18, X66bytes
5..70/Y66bytes71..136 unsigned big-endian. BN_bin2bn, rangoX,Y<primoP521
explícito, puntoafín/oncurve/noinfinito. Otros campos sin interpretar/registrar;
no otra curva/offset/algoritmo. Se elimina dh_point_prefix; en éxito añade
**dh_parameters_layout=affine_xy_at_offset5**, curva/point/result fijos anteriores.
No bytes/coords/header/hash/raw/captura; buffer/BIGNUM/punto borrados/liberados.
Punto válido no demuestra identidad, protocolo/KDF/MAC o causaVERIFY6982.

Pasaron **92 comprobaciones helper**, sintéticas: todos los valores de los
cuatro bytes de cabecera restantes no alteran validación de un punto válido,
rango de ambas coordenadas, selector desconocido, punto inválido/truncamiento,
privacidad. Helper real compilado en ambos builds. Lanzador95/observador162/
firma6/operación12 intactos, suites vigentes. **APDU/permisos sin cambios**:
control D004 transparente137 antes de único00B0000089,9000/137 obligatorios,
sin continuación/reintento/GET RESPONSE/canal/PIN. Driver/submódulo intactos.
Sin hardware/GUI/PIN por agente; no repetir desde sesión antes sin lector.

**Siguiente intervención:** mismaJCOP4/otros clientes cerrados,
`nix develop -c make inspect/jcop4-dh-parameters READER=0`, sin PIN. Se necesita
única lectura para aplicar validación corregida: buffer anterior borrado,
comparaciones no permiten validación offline. Revisar layout/curve/point/result,
sin informes completos. No pedir PIN ni MSE/GA/SM antes de identificar variante
si el punto valida. JCOP4/RSA3072 aún pendientes. Git autorizado: git add -u
falló por .git montado solo lectura. No eludirlo; dar comandos de commit de
los archivos seguidos.

**Último resultado: SELECT/READ D0049000; rechazo local de formato.**
Principal `6465cb0`, OpenSC `3dfef6704`, limpios al comenzar. Informe del usuario
`local/jcop4-dh-parameters-gd7lajTW/report.txt`, **2026-10-14T02:12:31Z (13 CR)**.
Inventario/build/conexión/ADF correctos; SELECT D0040/9000, identidad/estructura/
tamaño confirmados. READ00B0000089 ->0/9000, **exactamente137 bytes** (el parser
solo se llama tras esa comprobación). Resultado unrecognized_layout_stop,
unlock0/proceso1. No PIN/VERIFY/MSE/GA/SM/firma en el helper. No rechazo de
la tarjeta: byte2!=18 o byte4!=04 o ambos; informe anterior no distingue cuál.
No concluir otra curva/punto inválido: **no se ejecutó validación del punto**.
Buffer borrado, no cabecera/coordenadas/raw/hashes guardados; no se pueden
recuperar las comparaciones del informe. Lectura aceptada solo en ese contexto,
no prueba política universal ni SM/identidad/causa6982.

Mejora exclusiva del parser en tools/jcop4-dh-info.h: nuevas etiquetas fijas
**dh_layout_size**, **dh_curve_selector**, **dh_point_prefix** y point=
not_checked_due_to_layout. Size expected_137/unexpected_or_missing_stop;
selector matches_p521_candidate/different_uninterpreted; prefijo
matches_uncompressed_candidate/different_uninterpreted. Nulo/tamaño distinto
no clasifican bytes. Sin valores/otros selectores/coords/hash ni cambios de
aceptación: solo137, byte2=18, byte4=04 y punto P-521 válido no infinito.
Sin nueva curva/formato/APDU, permisos, repetición/continuación/fallback ni
cambios del lanzador, constructor, observador, firma, driver o submódulo.

Pasaron **90 comprobaciones del helper**, fallos individuales/combinados,
privacidad y nulos/truncados; real compilado contra ambos builds. Lanzador95,
observador162/firma6/operación12 intactos, suites previas vigentes. No hardware,
PIN o GUI por el agente, no repetir prueba desde sesión que no detectaba lector.
**Siguiente intervención:** misma JCOP4, otros clientes cerrados,
`nix develop -c make inspect/jcop4-dh-parameters READER=0`, sin PIN. Se requiere
otra única READ137 porque el resumen anterior omitió qué condición falló;
no repetir por error de tarjeta ni nueva variante. Revisar nuevos campos antes
de cambiar formato/curva o intentar canal/PIN. Datos D004/perfil efectivo/
VERIFY6982/JCOP4/RSA3072 aún pendientes. Git autorizado: git add -u falló
por .git montado solo lectura; no eludirlo. Dar comandos de commit, todos
estos archivos ya seguidos.

**Último resultado: D004 existe; lectura aislada de parámetros preparada.**
Principal `fa9c8f2`, OpenSC `3dfef6704`, limpios al comenzar. Informe del usuario
`local/jcop4-filed004-qSMBuWC3/report.txt`, **2026-10-14T01:38:49Z (13 CR)**:
inventario/build/conexión/ADF correctos, SELECT D0040/9000, transparente/
**137 bytes**, seguridad directa86 no interpretada; unlock/proceso0. No READ,
PIN, firma ni autenticación. No repetir SELECT-only ni interpretar86 como ACL.

Revisión estática de `371ca0` en libt_ias.so instalado/hash previo: byte2 es
índice de tabla4b9868, valor18 ->3720e0 ->NID716 (OpenSSL secp521r1).
EC_GROUP_new_by_curve_name/61c10, coordenadas desde5 divididas en dos mitades,
BN_bin2bn/63800 y236860 -> EC_POINT_set_affine_coordinates/63330, símbolosPLT
reales. Con137 serían66 bytes por coordenada. **Contenido aún no leído**;
no probar tipo runtime ni canal obligatorio por esta tabla. ZIP coincide con
SELECT/READ137 pero llama CardAccess al archivo: no adoptar esa etiqueta ni
su mezcla3DES/AES. El parser instalado y SELECT actual fundamentan una nueva
lectura acotada; no se deriva permiso de atributos propietarios.

Nuevo **make inspect/jcop4-dh-parameters / --dh-parameters**, helper default,
ATR/AID conocidos/anunciado. ADF y SELECT D004 de control, exige FCI/FCP íntegro,
83=D004,82 transparente y80=137, únicos. Ausentes/duplicados/diferentes/SW o
transporte malos detienen sin READ; 6A82 falla este modo. Un solo00B0000089,
canal básico, sin SFI/input; buffer137/9000/longitud137 obligatorios. Sin GET
RESPONSE/reintento/continuación/fallback, binding/objetos/certificados/VERIFY
incluso vacío/challenge/MSE/GA/SM/PIN/firma/administración. Filed004 conserva
SELECT-only; observador/driver/submódulo intactos. No afecta intentos mediante
VERIFY ni ejecuta la oferta de firmas fallidas del usuario.

Decoder `tools/jcop4-dh-info.h` reconoce solo137, byte2=18 y byte4=04; valida
punto P-521 no infinito localmente con OpenSSL. Solo etiquetas fijas/transporte/
SW, sin header/coordenadas/hash/serial/contenido ni captura; buffer borrado.
Punto válido no autentica identidad, perfil de canal/KDF/MAC ni vínculo con
RSA2048 de firma. No confundir estos parámetros EC con algoritmo de firma.
Se enlaza libcrypto explícitamente; ya presente en flake, sin cambiar lock.
**88 comprobaciones helper y95 lanzador** pasaron, transporte/punto sintéticos;
helper real compilado en ambos builds. Observador162/firma6/operación12 intactos,
suites vigentes. Logs privados habituales, sin repetir suites sin cambios.

Intento del agente timeout35s make inspect/jcop4-dh-parameters READER=0:
`local/jcop4-dh-parameters-D0xMmCEq/report.txt`, **2026-10-14T02:00:05Z (13 CR)**.
Inventario sin lectores/conexión fallida antes del helper: **sin SELECT/READ**
ni SW/PIN. Report0600/directorio0700. No atribuir rechazo6982 ni invalidar
SELECT9000 del usuario. **Siguiente intervención:** mismaJCOP4/lector accesible,
otros clientes cerrados, `nix develop -c make inspect/jcop4-dh-parameters READER=0`,
sin PIN. Revisar dh_control_result/read_transport/read_sw/parameters_result/
curve/point sin volcar informes completos. No ampliar otra curva/archivo/canal
ni solicitar PIN por inercia. Si valida, investigar acuerdo de claves/variante;
si falla, detener. Tipo runtime/VERIFY6982/JCOP4/RSA3072 siguen pendientes.
Git autorizado: se intentó añadir estos archivos y falló index.lock por .git
montado solo lectura; no eludirlo. **Incluir el nuevo tools/jcop4-dh-info.h
al hacer commit**, git add -u solo lo omitiría.

**Última revisión: login estático y candidato D004; consulta de tarjeta pendiente.**
Principal `1cfe6fd`, OpenSC `3dfef6704`, limpios al comenzar. Revisión estática
sin ejecutar C_Login/T_Login, sin PIN, GUI ni firma. T_Login (`285ca0`) pasa
por CP71 slot200 / `3c1bf0`: la rama de usuario condiciona la apertura de canal
(slot590 / `368190`) a `385530` / IsAnonymousDHRequired antes del login
`3c1840` / `3c11b0`. El descriptor interno en token+380 habilita esa decisión
para tipos1/2/8; tipo7 consulta EF.DH. **El tipo efectivo está pendiente**:
no equivale a generación JCOP, auth_method8, PACE ni CAN. C_Login de libidop11
(`1b4c00`) envuelve la credencial y llama `1ae380`; cadena intermedia hasta
T_Login aún sin resolver. No declarar esa rama ejecutada en esta tarjeta.

La consulta de estado PIN (slot5e8 / `3755c0`) prepara el contexto mediante
`368b90`: referencia global ordinaria pasa por `39cd30` y raíz `3a8432`;
hay ramas locales y de perfiles7/10/12 diferentes. El helper libre mantiene
ADF para PIN1: diferencia candidata, no causa6982 probada. Slot618 / `2d1740`
es RET/vacío, no selección. Constructor `39ce80` / `383b70` depende de
metadatos/flags; formato interno2 / `383f39` copia bytes sin hash en esa rama.
**No igualar el enum a OpenSC UTF8 ni dar por resuelto el formato completo**.
Perfil8 modifica un flag previo. La rama `3c1690` reenvía VERIFY tras ciertos
SW: no portarla ni atribuir reset sin resolverla.

`371ca0`, parámetros del canal desde `368190` slot460, forma D0+04 mediante
`367a80` (appendbyte verificado). Ruta D004 -> `381ca0` -> `3acb60`, bajo la
primera aplicación obtenida por `39cdd0`. Hay ramas1/7 alternativas; no prueba
ubicación efectiva ni que el AID conocido sea la primera aplicación de Idopte.
Precedente concreto para SELECT-only D004, no permiso READ/SM/PIN. Hashes,
VMA y límites en docs/jcop4.md; disassembly privado en
`local/jcop4-idopte-static-u8qg_p48/`, no copiar propietario/IDs a Git/chat.

Nuevo `make inspect/jcop4-filed004` / --filed004: helper default, ATR/AID
conocidos y AID anunciado en EF.DIR, ADF y único 00A4020002D00400, control<=256.
Sin READ, binding, PIN, challenge, MSE, GA, SM, firma, GET RESPONSE, reintentos
ni variantes. Parser existente sin interpretar86 como permiso. Pasaron **82
del helper y93 del lanzador**, transporte ficticio; helper real compilado
contra OpenSC offline/habitual. Observador162, firma6 y operación12 intactos,
suites previas vigentes. Nix falló por caché solo lectura/socket del daemon;
se usaron compilador/dependencias de sesión, sin flake/lock/HOME/sistema.
Driver/submódulo y permisos del observador intactos.

Intento `timeout 35s make inspect/jcop4-filed004 READER=0`: informe privado
`local/jcop4-filed004-yJF43Rcq/report.txt`, **2026-10-14T01:35:10Z (13 CR)**.
Inventario sin lectores, conexión fallida antes del helper: **no SELECT D004
ni SW**. Informe0600/directorio0700. No asumir rechazo6982/PIN ni causa física
frente a entorno. **Siguiente intervención:** misma JCOP4/lector accesibles,
otros clientes cerrados: `nix develop -c make inspect/jcop4-filed004 READER=0`,
sin PIN. Revisar control/SW antes de READ; no repetir inventario Idopte/estado,
otros FID/MF, login o PIN por inercia. No se solicitó la oferta de firmas
fallidas. Estados7/15 no prueban máximos/reinicios. JCOP4/RSA3072 pendientes.
Git autorizado: `git add -u` falló porque .git está montado solo lectura.
No eludirlo; dar comandos de commit al usuario.

**Último resultado: inventario público Idopte completo, sin PIN.**
Principal `e095ffa`, OpenSC `3dfef6704` limpios al comenzar. Nuevo modo separado
inspect/jcop4-idopte-post-auth-mf/--idopte-post-auth-mf,
JCOP4_DIAGNOSTIC_POST_AUTH_MF=1 depende de AUTH_STATUS y todos sus padres.
Después del estado externo válido9000/63Cx no cero, permite **máximo4 SELECT
MF** exactos00A40000023F00, caso3 o caso4/Le00, por el mismo handle de ese
estado; ATR fresco del emisor en cada selección. Mantiene stopped1 y contextos
normales0; no READ, otros SELECT, challenge, autenticación con datos, MSE/GA/
PSO/SM/escrituras/reintentos. Presupuestos/global128 sin reinicio; contexto/
reset/control/SetAttrib o primera operación inesperada cancelan oportunidad.
Buffer mínimo capacidad cliente/258, SW9000/tamaño válido únicamente, marker
privado previo; fallos/otrosSW detienen sin copiar/GET RESPONSE/fallback.
63C0 no activa excepción. Modos previos intactos; inventario/firma limpian flag.

Consultas que fundamentaron el límite: P8Shlru3 (00:52:48Z) permitió1 MF9000,
luego otro MF bloqueado. iuXYHcIZ (00:55:56Z) permitió2 MF9000, luego tercero
bloqueado. X0xq6QeQ (00:59:35Z), mismos permisos2, recuperó **origen de llamadas
antes omitido**, no reintento por error/PIN. Solo normaliza direcciones de retorno
ELF VMA de nombres exactos libt_ias.so/libidop11.so, máximo24frames, fases primera
selección permitida y primer MF bloqueado. No stack/argumentos/datos, direcciones
absolutas/ASLR, rutas, nombres de otros módulos o símbolos. Coincide por basename,
no certifica identidad de módulo. Siempre privado. La traza distingue rutas
T_GetTokenInfo (283e5c/3bb367) y T_GetVSlotInfo (281d3e/3a07d2), ambas pasando
por37560b ->39cd4d ->3a8432/selección raíz al preparar consulta de atributos PIN.
No demuestra loop de reintentos ni ejecución C_Login. Interpretar con binario
instalado/hash documentado, no símbolos vecinos ficticios de objdump.

Máximo4 deja margen acotado para esas consultas repetidas de metadatos, no cambia
la clase de APDU ni permisos restantes. Tests finales **162 observador/constructor/
decoder,91 lanzador público,6 firma:259**. Transporte ficticio/redirector real,
normalización en módulo sintético llamado libt_ias.so contra rangos ELF ejecutables,
exclusión de módulos/argumentos/rutas, máximo4 sin reponer, ATR/buffers/SW/log/
transporte fallidos y aislamiento flags. Biblioteca compilada. Helper79/operación12
intactos, suites previas vigentes; fixture privacidad creció4K->8K por registro
mayor, no cambio en política de log. Firma6 previa de este cambio sigue vigente.

**Última consulta:** local/jcop4-idopte-post-auth-mf-cn4D9B89/report.txt,
**2026-10-14T01:03:08Z (13 de octubre CR)**:
nix develop --offline -c make inspect/jcop4-idopte-post-auth-mf READER=0.
Build/inventario/ATR correctos; primera MF9000, estado externo63CF y **3 SELECT
MF posteriores0/9000**, same_as_status_sender/case4_le256/known_jcop4.
**token_info_result=returned, module_exit_status=0, observer_result=events_recorded;
diagnóstico0.** No operación posterior bloqueada ni permiso4 agotado.
108APDU:106SW9000, un63C7 PIN1 y un63CF ref03. La marca inicial
blocked=read4141_connection_boundary permanece porque la barrera global sigue
activa: **no indica fallo del inventario final**. No PIN/credencial/C_Login/firma,
MSE/GA/challenge/SM/escrituras ni VERIFY6982 nuevos. Estados7/15 no máximos ni
reinicio/decremento medido; no se pidió la oferta de firmas fallidas del usuario.

**Próximo paso:** analizar estáticamente C_Login/T_Login y la construcción de
VERIFY/ref/contexto/formato PIN en el perfil observado, comparando con diagnóstico
libre6982. Inicialización pública exitosa no prueba PIN/signado libre ni ausencia
de requisito de canal para firma. No repetir inventario/auth-status ni ensayar
PIN fallidos por inercia. Driver/submódulo intactos; JCOP4/RSA3072 pendientes.
Git autorizado, .git solo lectura si persiste; no eludirlo, dar comando de commit.

**Resultado anterior: estado externo03 devuelve63CF; PIN1 devuelve63C7.**
Principal `54b9cc8`, OpenSC `3dfef6704` limpios al comenzar. Usuario reiteró
verificar sus hipótesis: siete disponibles no acredita máximo7 ni reinicio tras
PIN/firma correctos. Autorizó acciones que no bloqueen tarjeta; ofreció firmas
fallidas controladas, **no se solicitó/ejecutó ninguna**. No ensayar credenciales
erróneas ni bloqueo/reset de contadores por inercia.

Análisis estático de libt_ias.so instalado, SHA256
2380540cd85316306233aa2baf640019a848606ce72bb5692cbebbb111542dc4:
RTTI CP71Token, vtable5d0c68/slot660 ->36e4f0, cadena de error
GetAuthRemainingAttempts. Construye CLA00/INS82/P100/P2 de objeto auth2c8,
sin datos/Le añadidos; convierte63Cx a x&0x0f. Slots640/648 son cache local,
no decremento/reset físico. T_GetVSlotInfo usa slot1b8 ->3a0710, que llama
slot660 al llenar su salida. Constructor227600 inicializa cabecera/campos vacíos,
transmisión2278c0, extracciónSW22c120. Direcciones VMA de este binario, **no
nombres fiables de símbolos vecinos de objdump**. Rama fábrica imprime P71
Token detected y usa368470. No demuestra CPU/modelo de esta tarjeta ni selección
runtime de clase; combinación con APDU observada/respuesta respalda intención
de consulta de estado, no auth con criptograma. Disassembly privado
local/jcop4-idopte-static-u8qg_p48/, no copiar a Git/chat.

Nuevo modo separado inspect/jcop4-idopte-auth-status/--idopte-auth-status,
JCOP4_DIAGNOSTIC_AUTH_STATUS=1 depende de SECOND_MF y padres, exclusivamente
inventario Idopte/stdin vacío. Modos previos conservan bloqueo82; inventario/
firma eliminan flag externo. Tras MF comprobado/9000, permite una sola
APDU exacta00820003/4bytes por el mismo handle. Primera operación inesperada
consume oportunidad; conexión/reset/control/SetAttrib la eliminan. Presupuestos
originales/global128 sin reinicio, stopped permanece1; ningún READ/PIN con
datos/challenge/MSE/GA/SM/PSO/escritura/reintento habilitado. Backend buffer2;
solo respuesta exacta2/SW9000 o63Cx se devuelve, incluso63C0 no habilita otro
intento. Marker privado previo obligatorio; fallos/otrosSW no fallback. Getter
PC/SC de atributos preexistente no se amplió. Solo transporte/cabecera/SW en log.
Pasaron150 observador/constructor/decoder,89 lanzador y6 firma: **245**, transporte
ficticio/redirector real, biblioteca compilada. Helper79/operación12 intactos.

Una consulta autorizada sin PIN/GUI tras tests:
nix develop --offline -c make inspect/jcop4-idopte-auth-status READER=0.
Informe local/jcop4-idopte-auth-status-dXi6peTi/report.txt,
**2026-10-14T00:27:33Z (13 de octubre CR)**. Build/inventario/ATR correctos;
MF por first_handle_value/case4_le256 ->0/9000, ATR del emisor conocido.
**00820003 -> transporte0/SW63CF, returned_still_stopped**, exactamente una.
105 APDU:103SW9000, un63C7 PIN y un63CF estado externo. **Siguiente bloqueado
00A40000/known_master_file_select**, no transmitido/sinSW. Módulo0,
token_info_result=CKR_DEVICE_ERROR/diagnóstico1/token_info_unavailable,
events_recorded. No credencial/login/firma/MSE/PSO/SM/escritura ni VERIFY6982 nuevo.
63CF encaja con15 disponibles para ref03; **no máximo15 ni cambio/reset medido**.
No equipararlo al PIN1/7 intentos ni probar vínculo Card Admin Key por ese SW.

**Próximo contraste:** caracterizar la siguiente selección MF/propósito dentro
del inventario y evaluar una excepción SELECT-only independiente, sin reabrir
transporte ni ensayar fallos PIN. No repetir auth-status ni otra referencia por
inercia. C_Login/VERIFY6982/canal pendiente, driver/submódulo intactos; JCOP4/
RSA3072 pendientes. Git autorizado pero .git sigue solo lectura; no eludirlo.

**Investigación anterior: precedente ISO para EXTERNAL AUTHENTICATE vacío.**
Principal `3e44bb6`, OpenSC `3dfef6704`, ambos limpios al comenzar.
Solo revisión documental/estática, sin hardware, PIN, GUI, Idopte ejecutado ni
cambios de protocolo/permisos. ISO/IEC7816-4:2005,7.5.4 y tabla68, contempla
EXTERNAL AUTHENTICATE sin datos para consultar intentos63Cx o necesidad de
verificación9000;7.5.1/tabla65 describe P1=00 sin algoritmo explícito y P2=03
como referencia global3 **en esa codificación estándar**. Original ISO alojado
por USPTO enlazado en docs/jcop4.md; texto revisado, screenshot no disponible.
Esto aporta una hipótesis fundada para la solicitud Idopte ya observada, **no
confirma su semántica ChipDoc/BCCR, identidad de clave ni efectos de contador**.
No decir que es autenticación efectiva, licencia, Card Admin Key o causa6982.

El ejemplo local epass2003 get_external_key_retries usa INS82 vacío, pero
P1=01/P2=80|kid y challenge previo: otro perfil, no copiarlo. API oficial
Idopte Token.externalAuthenticate describe criptogramas/challenge (MSE en IAS
ECC), no el estado vacío ni ref03. Búsquedas públicas no aportaron definición
ChipDoc aplicable; no prueba inexistencia. Inspección estática limitada de
símbolos/cadenas del módulo instalado encontró nombres
Control2ExternalAuthenticate/T_ExternalAuthenticate y variantes unblock/
changeSOPin; **no vincula esos nombres con esta APDU ni autoriza esas rutas**.
No ejecutar comandos administrativos ni extraer secretos/licencias.

**Próximo paso:** establecer ese vínculo mediante documentación ChipDoc678510/
perfil BCCR o análisis estático del constructor/manejador de la consulta vacía
en el módulo instalado. Mantener00820003 bloqueado; no repetir second-mf ni
pedir PIN para resolverlo. No solicitar otra vez los documentos ya preguntados
por inercia. Última evidencia hardware sigueY4Vg2V5R/00:03:32Z, sin SW para82.
Sin cambios de código, suites142/87/6 y79/12 previas vigentes, no repetidas.
JCOP4/VERIFY6982/RSA3072 pendientes. Git autorizado; .git de solo lectura
puede impedir el commit documental, no eludirlo.

**Resultado anterior: MF9000 por handle original; 00820003 caso1 bloqueado.**
Principal `0dfca5f`, OpenSC `3dfef6704` limpios al comenzar. Nuevo modo separado
`inspect/jcop4-idopte-second-mf` / --idopte-second-mf, opt-in
`JCOP4_DIAGNOSTIC_SECOND_MF=1` depende de B002 y padres, solo inventario Idopte/
stdin vacío. Modos anteriores mantienen parada total; inventario/otros modos/
firma eliminan variable. No reinicia presupuestos ni estado seleccionado.
Tras lectura B002 y primera conexión posterior a la barrera, exige mismo nombre
de lector guardado, nuevo handle, SHARED/T1 y ATR conocido en ese nuevo handle.
Permite un solo SELECT MF exacto00A40000023F00 (caso3 o caso4/Le00) después:
por el handle nuevo comprobado o **por el original igual al snapshot y trace**,
con ATR nuevamente comprobado sobre el emisor real. Otros handles/formas/control
consumen la oportunidad sin enviar; no reintentos/rearme por conexión/reset.
Global stopped sigue1; selected_mf/app/file y comparison_atr siguen0. Buffer
mínimo de capacidad del cliente/258, al menos2; SW9000 y tamaño válido únicamente
se devuelven a Idopte, datos nunca en log. Marker privado previo obligatorio;
eventos límite128 incluye SELECT; fallos detienen sin GET RESPONSE. Solo se
emiten SW/transporte y clases fijas de handle/forma/ATR; no valores brutos.

Se corrigió una hipótesis de emisor con evidencia, sin cambiar APDU: primera
consulta `local/jcop4-idopte-second-mf-wAVg1O4r/report.txt`,23:53:15UTC:
ATR del nuevo handle conocido, MF no transmitido, motivo aún sin distinguir.
Nueva clasificación, `...-qLdINu1m/report.txt`,23:55:56UTC:
**first_handle_value + case4_le256**, no MF transmitido. No era Le255 ni error
de tarjeta. Corrección con ATR del emisor original y tests, `...-3keXM1Gb/
report.txt`,23:59:20UTC: **MF0/9000**, siguiente **00820003 bloqueado**, forma
aún omitida. No afirmar que MF se transmitió por el handle nuevo.

Última consulta con clasificación de forma, tras pruebas:
`nix develop --offline -c make inspect/jcop4-idopte-second-mf READER=0`.
Informe `local/jcop4-idopte-second-mf-Y4Vg2V5R/report.txt`,
**2026-10-14T00:03:32Z (13 de octubre en Costa Rica)**. Build/inventario correctos;
ATR de handle nuevo y emisor original conocidos, MF solicitado por
first_handle_value/case4_le256 ->0/9000, selected_still_stopped. **104 APDU:
103SW9000 y un estado vacío63C7**. Luego **boundary_first_operation=apdu,
boundary_blocked_header=00820003,
boundary_command=header_00820003_case1_no_data**: APDU exactamente4bytes,
**sin datos, no transmitida, sin SW**, permanece bloqueada. No nuevas lecturas
tras MF, PIN, login, MSE/PSO/SM/escritura/firma. No atribuir efectos de contador,
licencia, Card Admin Key, PACE/CAN o causa de6982 a la mera cabecera/forma.
Módulo0/token_info_result=CKR_TOKEN_NOT_PRESENT/diagnóstico1/token_info_unavailable,
events_recorded. No rechazo nuevo de tarjeta.

Pasaron **142 observador/constructor/decoder,87 lanzador,6 lanzador firma
(235)**, fake transporte/redirector real; biblioteca compilada. Helper79 y
operación12 intactos, suites anteriores vigentes. Las repeticiones recuperaron
información antes omitida/corrigieron la hipótesis del handle, no fueron reintentos
de PIN ni variantes APDU. Cambios sin driver/submódulo ni protocolos de firma.

**Próximo contraste:** investigar la semántica de **00 82 00 03 sin datos** en
ChipDoc de este perfil antes de permitirlo. Biblioteca local iasecc-sm.c define
external authentication mediante MSE y challenge/SM, no esta consulta vacía;
no copiar esa ruta ni política de otro perfil. ZIP fuentes/textos no contiene
external-auth/00820003 según búsqueda estática, no ejecutar ZIP. No asumir que
vacío es consulta inocua/retry-counter o no consume intentos: requiere precedente
aplicable. Mantenerlo bloqueado, no pedir PIN ni repetir la captura idéntica.
JCOP4/VERIFY6982/RSA3072 pendientes. Capturas0700/0600local/ fuera de Git/chat.
Git autorizado, .git de solo lectura aún impide commits si persiste; no eludirlo.

**Resultado anterior: segunda conexión shared/T1; primer bloqueado SELECT MF.**
Usuario registró `2b7f401`, principal y OpenSC `3dfef6704` limpios al comenzar.
Instrumentación nueva únicamente en modo B002 ya optativo: conserva primera
conexión exitosa en memoria (nombre limitado a256 bytes, contexto y handle),
emite máximo4 resúmenes de igualdad/clases, nunca nombres/valores/hashes.
Igualdad de valores API no prueba recurso lógico ni identidad de tarjeta.
Registra una sola operación omitida por la barrera como apdu/reader_control/
reader_attribute; APDU solo cabecera4 bytes y clasificación de constantes
públicas conocidas, nunca datos/longitudes/credenciales. Control solo código y
feature_request_empty/unsupported/unknown; sin buffers/respuestas. Log omitido
fuera de B002 o si la primera causa de parada fue otra. Fallos de log no habilitan
transporte. **Sin nuevos APDU/permisos/GET ATR ni cambios de presupuestos**.

Pasaron **129 comprobaciones observador/constructor/decoder** con transporte
ficticio/redirector real: comparación privada, nombres largos/ausentes/fallos,
límite4, primera operación única, APDU/PIN/datos omitidos, controles sin enviar,
causa de parada y log fallido. Integración fake conecta segundo contexto/handle
direct y bloquea control de capacidades; ese direct es caso sintético, no dato
real. Biblioteca real compilada. Lanzador85/firma6/helper79/operación12 previos
intactos; no repetir sin cambios. Agente hizo una consulta nueva para obtener
información antes omitida, no repetición por error:
`nix develop --offline -c make inspect/jcop4-idopte-b002 READER=0`.
Informe `local/jcop4-idopte-b002-xkHMNglY/report.txt`, **23:16:08 UTC**:
compilación/inventario correctos, primera conexión shared/T1/ATR conocido.
**103 APDU:102SW9000 y un estado vacío63C7**, B001/B00214READ9000 cada uno,
dos controles42000D48 ->0. Segunda EstablishContext0/Connect0 con
**same_name_as_first, same_value_as_first(context), different_value_from_first
(handle), shared, t1, stopped**. **boundary_first_operation=apdu,
boundary_blocked_header=00A40000, boundary_command=known_master_file_select**.
SELECT MF identificado por forma/payload público fijo3F00, **no transmitido**,
sin SW. No lector/handle/contexto bruto guardado; no ATR nuevo comprobado en
segunda conexión ni prueba de misma tarjeta/recurso. No SCARD_SHARE_DIRECT real.
Módulo0/token_info_result=CKR_TOKEN_NOT_PRESENT/diagnóstico1/token_info_unavailable,
events_recorded. Sin PIN/login/MSE/PSO/firma nuevos, solo estado vacío ya permitido.

**Próximo contraste:** evaluar un modo separado que compruebe ATR y relación de
lector de la segunda conexión y permita exclusivamente el SELECT MF observado
una vez, conservando la parada para READ/control/auth y todos los presupuestos
originales; clasificar el siguiente bloqueado sin enviarlo. No rehabilitar el
transporte global ni reiniciar lecturas; no inferir la ruta de C_Login/VERIFY
por esta consulta sin login. SELECT MF sigue bloqueado en el modo actual; no
repetir idéntica observación ni pedir PIN por inercia. Driver/submódulo intactos;
firma JCOP4/VERIFY6982/RSA3072 pendientes. Informes privados local/0700/0600
fuera de Git/chat. Git autorizado, .git de solo lectura aún impide commits si
persiste; no eludirlo.

**Resultado anterior: Idopte READ B002 aceptado; segunda conexión bloqueada.**
Usuario registró `a1b7092`, principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo `inspect/jcop4-idopte-b002` / --idopte-b002, opt-in
`JCOP4_DIAGNOSTIC_CERT_B002=1` depende de CERT_B001 y padres anteriores, solo
en inventario Idopte/stdin vacío. Modos anteriores bloquean B002; inventario
previo/otros modos/firma eliminan variable externa. Exige ATR/ADF conocidos,
estado vacío MF válido y una lectura B001 aceptada; un SELECT B002 básico
exacto A4/02/00/Lc2/7u8 bytes. SELECT no9000 detiene sin follow-up. B0 caso2
básico corto/extendido, sin SFI/SM, Le<=256, cursor contiguo de bytes recibidos,
**presupuesto propio 3000 bytes solicitados/16 solicitudes**, sin reposición
por SELECT/reconexión. Buffer Le+2/SW9000/6282, datos solo al cliente; marker
privado obligatorio antes de SELECT/READ. Fallos detienen sin GET RESPONSE/
reintentos/fallback. PIN con datos/MSE/GA/SM/PSO/escrituras siguen bloqueados.

Pasaron **122 observador/constructor/decoder, 85 lanzador y 6 lanzador firma
(213)**, transporte ficticio/redirector real; biblioteca real compilada. Helper79
/operación12 intactos, suites previas vigentes, no repetir sin cambios. Agente
hizo una consulta sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-idopte-b002 READER=0`.
Informe privado `local/jcop4-idopte-b002-Q0vWkui7/report.txt`, **23:04:02 UTC**:
build/inventario correctos, ATR de primera conexión conocido. **103 APDU:102
SW9000 y un63C7 de estado vacío**; SELECT B002 y **14 READ B002 ->0/9000**,
última cabecera00B00B53/offset2899. B001 también14READ9000. Dos controles
42000D48 ->0, sin respuestas de capacidades registradas. Después de B002 y
segundo control: SCardEstablishContext0 -> **blocked=read4141_connection_boundary**
-> SCardConnect0. No SCardReconnect ni nueva cabecera APDU bloqueada. No
atribuir un archivo/PIN/comando al bloqueo ni afirmar mismo lector/tarjeta en
segunda conexión: el guard detenido no volvió a comprobar su ATR/identidad.
Módulo0/token_info_result=CKR_TOKEN_NOT_PRESENT/diagnóstico1/token_info_unavailable,
events_recorded. No rechazo nuevo APDU ni VERIFY6982; límite128 no alcanzado.
No credencial/login/firma nuevos, solo el estado VERIFY sin datos ya permitido.
Datos/longitudes de respuestas omitidos; no prueba DER válido/completo/uso.

**Próximo contraste:** caracterizar esa segunda conexión y el primer comando
que el guard omite cuando ya está detenido, con clasificación fija sin nombres,
handles, payloads ni transmisión adicional. Revisar si se trata de nuevo
lector/contexto y qué operación se solicita antes de decidir cualquier
excepción. No relajar la barrera global ni reiniciar presupuestos/contadores
por inercia; no repetir el mismo inventario sin instrumentación nueva. Los
SELECT/READ de certificados ya respondieron9000; no pedir otro PIN por ello.
Driver/submódulo intactos; JCOP4/VERIFY6982/RSA3072 pendientes. Informes
local/0700/0600 fuera de Git/chat. Git autorizado; .git de solo lectura aún
impide commits si persiste, no eludirlo.

**Resultado anterior: SELECT-only B002 aceptado, transparente/3000 bytes.**
Usuario registró `e48d508`; principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo `make inspect/jcop4-fileb002 READER=0` / --fileb002: reutiliza helper
SELECT-only con default driver, ATR/AID conocidos y anunciados en EF.DIR.
Tras seleccionar el ADF conocido, envía únicamente 00 A4 02 00 02 B0 02 00,
respuesta de control de hasta 256 bytes. Sin READ, binding, PIN, VERIFY, MSE,
PSO, escritura o Idopte. Valida FID si está presente; resume estructura/tamaño/
formas de seguridad, omite valores propietarios/raw/nombres/ACL. Errores/61xx/
respuesta inválida detienen sin GET RESPONSE/reintentos; 6A82 solo contextual.

Pasaron **79 comprobaciones del helper y 83 del lanzador (162)**, APIs falsas;
helper real compilado offline. Observador110/lanzador firma6/operación12 previos
intactos, no repetir sin cambios. Agente ejecutó una vez sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-fileb002 READER=0`.
Informe privado `local/jcop4-fileb002-0bSnKT0a/report.txt`, **22:59:33 UTC**:
inventario/build/conexión/aplicación/unlock/proceso 0. **SELECT B002 ->0/9000,
transparente/3000 bytes**, security_direct_tags=86,
security_format=profile_uninterpreted, standard_compact_read=uninterpreted.
No READ ni autenticación; CDF previo declara B002 como X.509/private=0,
pero no prueba DER real/completo, longitud DER, identidad, uso de firma o ACL.
Coincide el tamaño de EF con B001; no inferir igualdad de contenido/política.

**Próximo contraste:** modo Idopte separado para SELECT B002 observado y READ
acotada a los 3000 bytes medidos, después de lectura B001 aceptada/ADF/ATR y
padres comprobados. Conservar APDU, presupuestos/rangos limitados y contenido
solo al cliente, sin logs. PIN con datos/MSE/PSO/SM/escrituras/otros archivos
siguen bloqueados. No interpretar tag86/private=0 como ACL ni reutilizar
presupuesto B001. Guard actual todavía bloquea SELECT/READ B002. No repetir
SELECT/helper o inventario Idopte idénticos sin nueva información. Driver/
submódulo intactos; firma JCOP4/VERIFY6982/RSA3072 pendientes. Informes privados
local/0700/0600 fuera de Git/chat. Git autorizado, .git de solo lectura todavía
impide commits si persiste; no eludirlo.

**Resultado anterior: Idopte READ B001 aceptado; SELECT B002 bloqueado.**
Usuario registró `afda47f`, principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo modo separado `inspect/jcop4-idopte-b001` / --idopte-b001, opt-in
`JCOP4_DIAGNOSTIC_CERT_B001=1` depende de READ_DF0F y todos sus padres, solo
dentro de inventario Idopte con stdin vacío. Conserva bloqueo B001 en modos
anteriores; inventario previo/otros modos/firma eliminan variable externa.
Permite un SELECT B001 exacto básico A4/02/00/Lc2/total7u8, ATR/ADF conocidos,
estado vacío MF válido y READ DF0F previa. SELECT distinto de9000 detiene sin
GET RESPONSE/reintentos. Después permite B0 caso2 básico sin SFI/SM, Le<=256,
contiguo con cursor de bytes devueltos, **3000 bytes solicitados como máximo y
16 solicitudes**, presupuesto global por proceso sin reposición por SELECT o
reconexión. Buffer temporal Le+2 y SW9000/6282; respuesta solo al cliente,
contenido nunca en logs. Marker privado obligatorio antes de SELECT/READ;
fallos de log/buffer/transporte/SW/tamaño detienen. PIN con datos/MSE/GA/SM/
PSO/escrituras/otros certificados siguen bloqueados. No interpretar 86 como ACL.

Pasaron **110 comprobaciones observador/constructor/decoder, 81 lanzador y 6
lanzador firma (197)**, fake transporte/redirector real; biblioteca real
compilada. Helper77/operación12 anteriores intactos, no repetir sin cambios.
Agente ejecutó una vez, sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-idopte-b001 READER=0`.
Informe privado `local/jcop4-idopte-b001-y0qYQHxM/report.txt`, **22:46:41 UTC**:
build/inventario correctos, ATR conectado conocido; **88 APDU:87 SW9000 y un
63C7 de estado vacío**. SELECT B001 ->0/9000; **14 READ B001 ->0/9000**, última
cabecera00B00B53 (offset2899). No contenido/longitudes de respuestas registrados;
no afirmar DER completo/válido o longitud DER. Luego **SELECT EF B002 bloqueado
antes de transmitir**, sin SW/control/contenido de B002. Módulo0,
token_info_result=CKR_TOKEN_NOT_PRESENT, diagnóstico1/token_info_unavailable,
observer_result=events_recorded. El fallo corresponde al guard antes de B002,
no rechazo READ B001 ni nuevo VERIFY6982. No credencial/login/firma nuevos;
hubo la única consulta de estado VERIFY sin datos ya permitida, no autenticación.

**Próximo contraste:** SELECT-only B002 bajo el AID conocido, por su referencia
X.509/private=0 en CDF y solicitud efectiva Idopte. Obtener tamaño/estructura/
control antes de ampliar permisos. B002 todavía bloqueado; no asumir tamaño
igual a B001/ACL por private=0 ni autorizar lecturas por tag86/9000. No repetir
inventario Idopte o B001/PIN idénticos por inercia. Driver/submódulo intactos;
JCOP4/RSA3072 pendientes. Informes local/0700/0600 fuera de Git/chat.
Git autorizado, .git de solo lectura todavía impide commits si persiste; no eludir.

**Resultado anterior: SELECT-only B001 aceptado, transparente/3000 bytes.**
Usuario registró `2cff02e`; principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo `make inspect/jcop4-fileb001 READER=0` / --fileb001: reutiliza helper
SELECT-only con default driver, ATR/AID conocidos y anunciados en EF.DIR.
Tras seleccionar ADF conocido, envía únicamente 00 A4 02 00 02 B0 01 00,
respuesta de control de hasta 256 bytes. Sin READ, binding, PIN, VERIFY, MSE,
PSO, escritura o Idopte. Resume FID validado/estructura/tamaño/formas de
seguridad, omite valores propietarios/raw/nombres/ACL. Errores/61xx/tamaño
inválido detienen sin GET RESPONSE/reintentos; 6A82 solo ausencia en contexto.

Pasaron **77 comprobaciones del helper y 79 del lanzador (156)**, APIs falsas;
helper real compilado offline. Observador 99/lanzador firma 6/operación 12
previos intactos, no repetir sin cambios. Agente ejecutó una vez sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-fileb001 READER=0`.
Informe privado `local/jcop4-fileb001-8OxZ9U7P/report.txt`, **22:39:39 UTC**:
inventario/build/conexión/aplicación/unlock/proceso 0. **SELECT B001 -> 0/9000,
transparente/3000 bytes**, security_direct_tags=86,
security_format=profile_uninterpreted, standard_compact_read=uninterpreted.
No READ ni autenticación; CDF previo declara B001/B002 como X.509/private=0,
pero no identifica DER real, longitud DER, certificado de firma o ACL.

**Próximo contraste:** preparar un modo Idopte separado que permita el SELECT
B001 observado y una lectura acotada por su tamaño de EF (3000 bytes), con
contexto ATR/ADF/padres comprobados, APDU sin modificar, presupuestos/rangos
limitados, respuesta solo al cliente y sin log de contenido. No interpretar 86
ni private=0 como política; no habilitar PIN/auth/MSE/PSO ni otros certificados
por inercia. Guard actual todavía bloquea SELECT/READ B001. No repetir este
SELECT/helper o inventario idéntico sin nueva información. Driver/submódulo
intactos; JCOP4/RSA3072 pendientes. Informes local/0700/0600 fuera de Git/chat.
Git autorizado, .git de solo lectura aún impide commits si persiste; no eludirlo.

**Resultado anterior: CDF7005 declara certificados en B001 y B002.**
Usuario registró `2a30496`, principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo `make inspect/jcop4-certificate-paths READER=0` / --certificate-paths:
helper default driver con ATR/AID conocidos y anunciados, binding PKCS#15
nativo sin cachés/emuladores/PIN, parse explícito exclusivamente del CDF7005
anunciado en ODF. No usar get_objects para ocultar errores o buscar CDFs
trusted/useful. Valida directorio único, rutas/tipos/AID, objetos/rangos y límites
antes de emitir inventario; ciclos/metadata inválida/fallos detienen. Imprime
solo rutas, offset/count, marca privado y almacenamiento path/inline (contenido
omitido). Sin IDs/nombres/DER, lectura de certificados referenciados, PIN,
VERIFY, MSE, PSO, escritura o Idopte. Valores inline no se imprimen; rutas no
se siguen. Mantiene todos los permisos del observador sin modificaciones.

Pasaron **75 comprobaciones del helper y 77 del lanzador (152)**, APIs falsas;
helper real compilado offline. Observador 99, lanzador firma 6 y operación 12
previos intactos, no repetir sin cambios. Agente ejecutó una vez sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-certificate-paths READER=0`.
Informe privado `local/jcop4-certificate-paths-HN4CjVzr/report.txt`,
**22:27:46 UTC**. Inventario/build/binding nativo/parse CDF/unlock/proceso 0.
**Dos objetos X.509: rutas B001 y B002, ambas private=0, offset=0, count=-1**
(sin rango explícito). No se siguieron rutas ni se leyeron certificados.
Esto identifica la referencia B001 en el CDF de este perfil, no certifica aún
contenido DER/ACL, firma frente a autenticación o relación con llave/PIN6982.
Sin nombres/IDs/seriales al chat/Git; informes0700/0600 bajo local/.

**Próximo contraste:** SELECT-only B001 bajo el mismo AID para conocer control,
FID/tamaño/estructura/seguridad, sin READ/PIN; después decidir observación
acotada de la lectura solicitada por Idopte según evidencia. No asumir ACL por
private=0 ni copiar el PIN/AID/llave del driver uruguayo. Guard Idopte sigue
bloqueando SELECT/READ B001; no repetir inventario Idopte o CDF idénticos por
inercia. Driver/submódulo intactos; firma JCOP4/RSA3072 siguen pendientes.
Git autorizado, .git de solo lectura aún impide commits si persiste; no eludirlo.

**Resultado anterior: Idopte READ DF0F aceptado; SELECT B001 bloqueado.**
Usuario registró `442c70e`, principal y OpenSC `3dfef6704` limpios al comenzar.
Nuevo modo separado `inspect/jcop4-idopte-read-df0f`, flag
`JCOP4_DIAGNOSTIC_READ_DF0F=1` dependiente de SELECT_DF0F y sus padres, solo
para inventario Idopte con stdin vacío. Conserva permisos de modos anteriores.
Tras SELECT DF0F exacto/9000 y ATR/ADF/estado vacío MF comprobados, permite
**una sola READ BINARY caso 2 básica, offset 0, Le exactamente 150** (corta o
extendida). No reescribe APDU ni permite Le mayor/otro offset/SFI/SM/canal.
Buffer temporal Le+2, únicamente SW9000/6282; datos solo al cliente, no a logs.
Errores/oversize/log fallido detienen antes de cualquier fallback; sin GET
RESPONSE/reintentos. Nueva flag eliminada del inventario previo, otros modos y
firma. PIN con datos, MSE/GA/SM/PSO/escrituras siguen bloqueados.

Pasaron **99 comprobaciones del observador/constructor/decoder, 75 del lanzador
público y 6 del lanzador de firma (180)**, transporte ficticio/redirector real;
biblioteca real compilada. Helper 68 y operación de firma 12 intactos, pruebas
previas vigentes. Agente ejecutó una vez, sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-idopte-read-df0f READER=0`.
Informe `local/jcop4-idopte-read-df0f-BeXtNUJ3/report.txt`, **22:21:37 UTC**:
build/inventario correctos, ATR conectado conocido. **73 APDU: 72 SW9000 y un
63C7 de estado vacío**, transporte correcto. SELECT DF0F y READ offset0 ->
0/9000; luego SELECT MF y ADF conocidos -> 0/9000; **SELECT EF B001 bloqueado
antes de transmitir**, sin respuesta/control/contenido de B001. Módulo 0,
token_info_result=CKR_TOKEN_NOT_PRESENT, diagnóstico 1/token_info_unavailable,
observer_result=events_recorded. No confundir bloqueo local con rechazo de la
tarjeta ni con VERIFY6982 autenticado anterior. No PIN/login/firma nuevos.

DF0F leído exitosamente por el proveedor sin autenticación solicitada en este
proceso; contenido omitido, propósito/política siguen sin identificar. No
atribuirle licencia/CA/PACE ni copiarlo al driver por inercia. B001 aparece como
certificado de firma en `vendor/opensc/src/libopensc/pkcs15-cedulauy.c`, pero ese
perfil uruguayo usa otro AID/PIN/llave: **no prueba qué contiene B001 en CR**.
Búsqueda estática de fuentes/textos del ZIP no encontró B001; no ejecutar ZIP.
**Próximo contraste:** confirmar si CDF7005 de este AID referencia B001 como
certificado, con un resumen de rutas de metadatos sin IDs/nombres/contenidos,
antes de ampliar SELECT/READ. B001 sigue bloqueado. No repetir captura idéntica
ni pedir PIN por inercia. JCOP4/RSA3072 pendientes; driver/submódulo intactos.
Capturas privadas local/0700/0600 fuera de Git/chat. Git autorizado; montaje
.git de solo lectura aún puede impedir el commit; no eludirlo.

**Resultado anterior: Idopte SELECT DF0F -> READ bloqueado, 22:03:13 UTC.**
Usuario registró `3fbd854`, OpenSC `3dfef6704`, ambos limpios antes de preparar
cambios. Turno interrumpido tras editar cuatro archivos; retomado con pruebas,
no había hardware de esta versión antes de verificarlas. Agente ejecutó una vez
`nix develop --offline -c make inspect/jcop4-idopte-df0f READER=0`, sin PIN/GUI.
Informe `local/jcop4-idopte-df0f-OCE6tqH3/report.txt`,**22:03:13 UTC**: build/
inventario correctos, ATR conectado conocido;70APDU,69SW9000 y estado vacío63C7.
Después MF/status/ADF conocidos, **SELECT DF0F0/9000**, luego **READ BINARY
00B00000 bloqueado antes de transmitir**. Sin SW/contenido de ese READ.
Módulo0/C_GetTokenInfo CKR_TOKEN_NOT_PRESENT/diagnóstico1/token_info_unavailable,
events_recorded: guard interrumpió acceso, no rechazo de tarjeta ni VERIFY6982.
Ningún PIN/login/MSE/GA/SM/PSO/escritura/firma. DF0F150bytes/proprietary86 sigue
sin propósito/política conocidos; no asumir licencia/CA/PACE por FID/tamaño.

Nuevo --idopte-df0f/target inspect/jcop4-idopte-df0f, opt-in
JCOP4_DIAGNOSTIC_SELECT_DF0F=1 requiere modo padre PIN_STATUS y todos los
anteriores solo dentro de Idopte inventario/slots/empty stdin. Modos anteriores
siguen bloqueando DF0F; inventario/firma limpian variable. Exige ATR/ADF y
resultado real válido de estado vacío enMF (2bytes/SW9000o63Cx), no contador
antes de transmitir; estado descarta en cambio de conexión. Un SELECT DF0F
básico exacto A4/02/00/Lc2/7u8bytes por proceso, marker privado obligatorio
antes de transmitir. No READ de DF0F ni permiso heredado; cualquier SW!=9000
paraSELECT detiene después, sin GET RESPONSE/reintento. PIN con datos bloqueado.
Pasaron **90 observador/constructor/decoder,73 lanzador y6 lanzadorfirma**,
fake transporte/redirector real; biblioteca compilada. Helper68/operación12
previos vigentes, driver/submódulo intactos, no repetir sin cambios.
Próximo contraste posible: excepción READ DF0F separada/acotada a150bytes y a
su forma solicitada, sin registrar contenido/cambiar APDU; o definición primaria
del perfil. No ampliar por 9000/tag86 ni repetir captura idéntica/PIN porinercia.
Informes privados0700/0600local/, no agregar aGit. JCOP4/RSA3072 pendientes.
Git autorizado, montaje.gitreadonly aún impide commits si persiste.

**Resultado anterior: SELECT-only DF0F, 21:43:59 UTC.**
Usuario registró `d07b7c2`, principal limpio al comenzar; OpenSC `3dfef6704`
intacto. Agente ejecutó `nix develop --offline -c make inspect/jcop4-filedf0f
READER=0`, sin PIN/GUI. Informe `local/jcop4-filedf0f-6FBH76f8/report.txt`:
SELECTtransporte0/**9000**, transparente/**150bytes**, security_direct_tags=86,
security_format=profile_uninterpreted, standard_compact_read=uninterpreted.
Inventario/build/conexión/lock/EF.ATR/EF.DIR/unlock/proceso0. No READ deDF0F,
binding/PIN/VERIFY/MSE/GA/PSO/escritura. Solo ADF conocido anunciado y
00 A4 02 00 02 DF 0F 00; no reproduce toda la inicialización deIdopte ni estado.

Nuevo target inspect/jcop4-filedf0f/--filedf0f, parser compartido con FID83
validado y valores/nombres/ACL/raw omitidos. No sondeo arbitrario ni follow-up
tras6A82/errores/61xx. Pasaron **68 helper y71 lanzador**, fake APIs, helper real
compilado. Observador/permisos y driver intactos; suites83observador/6firma/
12operación previas vigentes, no repetir sin cambios. Capturas0700/0600local/.
**El guard Idopte aún bloquea SELECT/READ DF0F**; no nueva captura Idopte.
150bytes/tag86/selección aceptada no identifican licencia, CAN/PACE/CA, permiso
público o causa de6982. Página oficial de licencias anterior no identificaFID.
Próximo contraste: SELECT-only DF0F del proveedor tras la secuencia ya observada,
con READ bloqueado, para identificar su próximo comando; no repetir este
SELECT/helper o consulta estado idénticos ni pedir PIN porinercia. JCOP4/RSA3072
pendientes. Git autorizado pero montaje.gitreadonly impide commits si persiste.

**Resultado anterior: estado vacío63C7 -> SELECT DF0F bloqueado, 21:38:03 UTC.**
Usuario registró `387d233`, OpenSC `3dfef6704`, limpios al comenzar. Agente hizo
sin PIN/GUI: SELECT-only7005 (`local/jcop4-file7005-eK0x57Vx/report.txt`,21:26:20),
0/9000/transparente2368bytes/tag86nointerpretado, inventario/build/conexión/
EF.ATR/EF.DIR/unlock/proceso0, sin READ. CDF Idopte nuevo
`local/jcop4-idopte-cdf-x3Np9eLh/report.txt`,21:29:08:67APDU0/9000,11READ7005,
últimooffset08B6, finalSELECT00A40000/9000 y VERIFY00200001 bloqueado. **Módulo0
pero C_GetTokenInfo CKR_TOKEN_NOT_PRESENT**: no inventario Cryptoki satisfactorio.
Se repitió una vez para nueva clasificación antes omitida, no autenticación:
`local/jcop4-idopte-cdf-WJMGxt0U/report.txt`,21:31:50, mismas67APDU0/9000,
SELECT MF3F00/9000 -> blocked_verify_form=case1_no_data. Ningún PIN/VERIFY real.

Nuevo modo independiente inspect/jcop4-idopte-pin-status (sin PIN/login/GUI),
opt-in JCOP4_DIAGNOSTIC_PIN_STATUS=1 junto a CDF permite **una sola00 20 00 01
caso1 sin datos** bajo MF confirmado/ATR conocido/despuésREAD7005/sin pending.
Buffer temporal2, soloSW1/SW2; errores/tamaño/logfallido detienen. Cualquier
credencial/ref/canal/caso/repetición bloqueado. No autentica ni consume un intento
con PIN. Los modos anteriores siguen bloqueando incluso VERIFY vacío.
Agente ejecutó una vez, informe
`local/jcop4-idopte-pin-status-1qsDoPci/report.txt`,**21:38:03 UTC**:69APDU,
68SW9000 y estado vacío0/**63C7**. Después SELECT ADF ChipDoc conocido0/9000,
**SELECT EF DF0F bloqueado antes de transmitir**, sin SW/FCP/contenido deDF0F.
Módulo0, token_info_result=CKR_TOKEN_NOT_PRESENT, events_recorded y diagnóstico1/
token_info_unavailable. No PIN/MSE/GA/SM/PSO/escrituras/firmas. VERIFY6982
anterior sigue pendiente. Estado63C7 no prueba PIN correcto ni contador antes/
después de credencial; no repetir estado ni pedir otro PIN por inercia.

Nuevo SELECT-only--file7005 y modo --idopte-cdf (opt-in JCOP4_DIAGNOSTIC_CDF=1
junto aPUKDF/etc dentro de Idopte inventario/empty stdin): SELECT7005exacto/9000,
ADF/ATR/READ7004previo, READ2368bytes/11calls/hasta256por solicitud, continuidad
sin reposición porSELECT/reconexión, bufferLe+2/SW9000/6282, fallos detienen sin
GET RESPONSE/reintentos. Contenidos solo al cliente,0700/0600local/. Nuevas
etiquetas MF y VERIFY conocido distinguen formas fijas sin datos/longitudes.
Launcher inventario/firma limpia opt-ins. Launcher registra C_GetTokenInfo por
mensajes fijos: fallaTokenInfo+proceso0 termina1, preserva módulo0, no IDs alchat.
Pasaron **66 helper,83 observador/constructor/decoder,69 lanzador y6 lanzador
firma**,fakeAPIs/redirectorreal, biblioteca/helper compilados. Driver/operación
firma intactos, suite12previa vigente, no repetir sin cambios.
No referenciaDF0F en ODF/código/ZIP revisados. Fuente primaria
https://www.idopte.fr/licensesys/ confirma licencias middleware en archivo de
tarjeta, pero **no identificaDF0F** ni prueba causa6982; no etiquetarlo licencia/
CA/PACE. No herramientas de licencias/administración/generación/carga/escrituras.
Próximo paso: SELECT-only DF0F bajo el AID observado, o definición primaria del
perfil; no READdesconocido/repetirconsultas porinercia. JCOP4/RSA3072 pendientes.
Git autorizado; montaje.gitreadonly sigue impidiendo commits si persiste.

**Resultado anterior: READ7004 -> SELECT7005 bloqueado, 21:16:07 UTC.**
Usuario registró `adb68f9`, OpenSC `3dfef6704`, ambos limpios al comenzar.
Agente preparó/ejecutó modo nuevo sin PIN/GUI:
`nix develop --offline -c make inspect/jcop4-idopte-pukdf READER=0`.
Informe `local/jcop4-idopte-pukdf-rI9D42no/report.txt`, **21:16:07 UTC**.
Compilación/inventario correctos, ATR conectado conocido; **54 APDU0/9000**,
SELECT7004 y19 READ aceptados, últimooffset0FAE. Contenido/Le/longitud final
omitidos, no afirmar lectura completa por cabeceras. **SELECT7005 bloqueado
antes de transmitir**, sin SW de tarjeta a ese comando. Módulo1/events_recorded
por guard, no rechazo de READ ni nuevo VERIFY6982. Sin PIN/VERIFY/MSE/GA/SM/PSO/
escritura/firma. ODF7005=certificates, con precedente de inventario nativo de
certificados públicos sin login. Secuencia sigue siendo inventario; no identifica
canal de C_Login. No descartar CA/PACE ni atribuirlo a4141/4109 por orden.

Nuevo --idopte-pukdf/target inspect/jcop4-idopte-pukdf, opt-in
JCOP4_DIAGNOSTIC_PUKDF=1 requiere PRKDF/AODF/READ4109/READ4141 dentro de Idopte
inventario/slots/empty stdin. Modos anteriores mantienen SELECT/READ7004
bloqueados. Exige ATR/ADF confirmados y READ7002 previo para SELECT7004 exacto
básico/9000. READ caso2 básico corto/extendido, hasta256/solicitud,
presupuesto4096 y19calls por proceso, offset0+continuidad sin reposición por
SELECT ni reconexión. BufferLe+2 máx258, SW9000/6282; fallos detienen, sin GET
RESPONSE/reintentos. Contenido solo al cliente, trazas privadas0700/0600.
No nuevas lecturas de rutas de objetos. Inventario/firmador limpian opt-in.
Pasaron **70 observador/constructor/decoder,61 lanzador y6 lanzadorfirma**,
fake transporte/redirector real; biblioteca compilada. Helper público65/
operación12 previos vigentes intactos, no repetir suites sin cambios.
Submódulo/driver intactos. No repetir consulta idéntica ni pedir PIN. Próximo
paso: control/acceso CDF7005 del mismo ODF con precedente de certificados
públicos ya inventariados, sin habilitar otros FID por inercia. JCOP4/RSA3072
pendientes. Git autorizado pero montaje.git readonly aún impide commits.

**Resultado anterior: PuKDF7004 contiene dos RSA2048, 20:48:52 UTC.**
Usuario registró `76c99c9`, OpenSC `3dfef6704`, ambos limpios al comenzar.
Agente ejecutó --file7004 SELECT-only sin PIN/GUI, informe privado
`local/jcop4-file7004-JRw8iNNl/report.txt`, **20:48:34 UTC**: SELECT0/9000,
transparente/**4096 bytes**, tag86/profile_uninterpreted/read_uninterpreted;
inventario/compilación/contexto/EF.ATR/EF.DIR/unlock/proceso correctos. Sin READ.
Después `nix develop --offline -c make inspect/jcop4-public-keys READER=0`:
`local/jcop4-public-keys-Qfl75Od0/report.txt`, **20:48:52 UTC**. Nativebind0,
PuKDFparse0, count2, RSA2048/ref144(0x90)/native1/usage208 y
RSA2048/ref145(0x91)/native1/usage192. Ningún objeto EC en ese PuKDF analizado.
Referencias coinciden con privadas previas; no comparación matemática de claves
con certificados ni lectura de material de llaves. No descartar claves CA/ECC
fuera de PuKDF ni trasladar punto P521 del ZIP a esta tarjeta. VERIFY6982 sigue
pendiente; no otra prueba de PIN. Todos los pasos terminaron0, sin autenticación.

Nuevo SELECT-only --file7004/target inspect/jcop4-file7004 reutiliza parser con
FID83 validado. --public-keys/target inspect/jcop4-public-keys usa nativebinding
sin cachés/emuladores, exige ADF/ATR conocidos/EF.DIR, un PuKDF anunciado7004
o3F00/7004 con AID compatible, no arbitrario/duplicado. Llama sc_pkcs15_parse_df
explícitamente y propaga errores: get_objects puede omitir error de DF y reportar
count0, no usar ese0 como prueba de vacío. No visita PuKDFTrusted ni otros DF.
Filtra objetos por DF, valida clase/data, menos32objetos y128nodos; resume
familias/referencias/native/usage/tamaños. ECbits0 sería desconocido, no curva0.
No IDs/etiquetas/rutas/raw/SPKI ni lectura de llaves referenciadas/certificados.
No PIN/VERIFY/MSE/GA/PSO/escrituras. Informes privados0700/0600 bajo local/.
Pasaron **65 helper y59 lanzador**, fake APIs; helper real compilado. RSA3072/
EC521 en fixtures no son observaciones de hardware. Observador64/lanzadorfirma6/
operación12 previos vigentes sin cambios; no repetir suites sin cambios.
Driver/submódulo intactos. El guard Idopte aún bloquea SELECT/READ7004; no se
repitió captura Idopte. Siguiente paso: si se sigue enumeración, lectura acotada
PuKDF7004 del mismo AID/tamaño4096 sin registrar contenido para observar próximo
comando; no suponer que inventario explique C_Login/canal. JCOP4/RSA3072 pendientes.
Git autorizado, pero montaje.git readonly sigue impidiendo commits si persiste.

**Resultado anterior: READ7002 -> SELECT7004 bloqueado, 20:33:01 UTC.**
Usuario hizo commit `c61fdd7`, OpenSC `3dfef6704`, limpios al comenzar.
Agente ejecutó SELECT-only7002, `local/jcop4-file7002-832gXk0r/report.txt`,
**20:30:16 UTC**: SELECT0/9000, transparente/**2048 bytes**, tag86/
profile_uninterpreted/read_uninterpreted; compilación/inventario/contexto/
EF.ATR/EF.DIR/unlock/proceso correctos, sin READ/binding/PIN. Ese tamaño es
bytes del directorio, no bits de RSA. FID7002 confirmado en ODF=private_keys;
inventario nativo previo obtuvo referencias RSA native=1, no extracción de clave.

Después nuevo `make inspect/jcop4-idopte-prkdf READER=0`, agente sin PIN/GUI:
`local/jcop4-idopte-prkdf-2LKKNfZB/report.txt`, **20:33:01 UTC**. Compilación/
inventario correctos, ATR conectado conocido; 34 APDU transmitidas0/9000.
SELECT7002 y diez READ aceptados, offsets0000/00DF/01BE/029D/037C/045B/053A/
0619/06F8/07D7. Contenido/Le/longitud final omitidos, no afirmar lectura completa
por cabeceras. **SELECT EF7004 bloqueado antes de transmitir**, sin SW de
tarjeta, módulo1/events_recorded por guard. Sin PIN/VERIFY/MSE/GA/SM/PSO/firma.
ODF7004=public_keys; siguiente contraste útil es su inventario de algoritmos/
referencias, no asumir clave CA/Chip Authentication por nombre ni trasladar ZIP.
VERIFY6982 y canal de autenticación siguen pendientes.

Nuevo --file7002/target SELECT-only fijo, parser compartido valida83 y omite
valores. --idopte-prkdf/target nuevo, opt-in JCOP4_DIAGNOSTIC_PRKDF=1 requiere
AODF/READ4109/READ4141 dentro de Idopte inventario/slots/empty stdin. Modos
anteriores mantienen SELECT/READ7002 bloqueados. Exige ADF/ATR/READ7001 previo,
SELECT7002 básico exacto/9000. READ caso2 corto/extendido básico sin SFI ni datos,
hasta256bytes/solicitud, presupuesto2048 y máximo10calls por proceso,
continuidad con offset real y sin reposición por SELECT. BufferLe+2 máximo258,
SW9000/6282, fallos/continuación/transporte/tamaño/log detienen; no GET RESPONSE
ni reintentos. Solo contenidos al cliente. Lanzadores inventario/firma limpian
opt-in externo. Registros privados0700/0600, no agregar local/ a Git.
Pasaron **60 helper, 64 observador/constructor/decoder, 55 lanzador y 6 lanzador
firma**, fake transporte/redirector real; helper/biblioteca compilados. Driver
y operación firma intactos, pruebas12 previas vigentes, no repetir sin cambios.
No repetir consulta idéntica ni pedir PIN; estudiar PuKDF7004 del mismo AID
con precedente ODF. JCOP4/RSA3072 pendientes. Git autorizado pero montaje.git
solo lectura sigue impidiendo commits si persiste.

**Resultado anterior: READ7001 -> SELECT7002 bloqueado, 20:18:15 UTC.**
Usuario registró `8dbd7da`, OpenSC `3dfef6704`, ambos limpios al comenzar.
Agente ejecutó dos diagnósticos sin PIN/GUI. Primero SELECT-only7001:
`local/jcop4-file7001-3dyC89fA/report.txt`, **20:15:53 UTC**, SELECT0/9000,
transparente/**300 bytes**, tag86/profile_uninterpreted/read_uninterpreted;
inventario/compilación/contexto/EF.ATR/EF.DIR/unlock/proceso correctos, sin READ.
Segundo `nix develop --offline -c make inspect/jcop4-idopte-aodf READER=0`:
`local/jcop4-idopte-aodf-CUeTL3Na/report.txt`, **20:18:15 UTC**, ATR conectado
conocido, 23 APDU transmitidas0/9000. SELECT7001 y dos READ BINARY (00B00000,
luego00B000DF) correctos; contenido/longitudes omitidos, no afirmar lectura
completa por cabeceras. **SELECT EF7002 bloqueado antes de transmitir**, sin SW
de tarjeta a ese comando. Módulo1/events_recorded por guard, no rechazo de READ
ni nuevo VERIFY6982. Sin PIN/VERIFY/MSE/GA/SM/PSO/escritura/firmas.

ODF del mismo AID ya identifica 7001=auth_objects, 7002=private_keys. Inventario
nativo previo obtuvo metadatos PIN y referencias RSA nativas sin login. PrKDF
no significa exportar llaves; lo observado es enumeración, todavía no canal de
autenticación. No atribuir función a 4141/4109 ni política a86 por estos pasos.
Nuevo SELECT-only --file7001/target inspect/jcop4-file7001; parser compartido,
valida FID83 concreto, no arbitrario. Nuevo modo --idopte-aodf/target
inspect/jcop4-idopte-aodf, opt-in JCOP4_DIAGNOSTIC_AODF=1 requiere READ4109 y
READ4141 dentro del proceso Idopte inventario/slots/empty stdin. Previo
READ4109 y ADF/ATR confirmados permiten SELECT7001 exacto básico y READ7001
hasta300bytes/2solicitudes contiguas. No reposición por SELECT, no relecturas,
GET RESPONSE/reintento/continuación/otras formas. SW9000/6282, bufferLe+2;
fallo de log/transporte/tamaño/SW detiene. Contenidos solo al cliente.
Modos anteriores bloquean7001. Launcher inventario/firma limpia nuevo opt-in.
Pasaron **59 helper, 57 observador/constructor/decoder, 51 lanzador y 6 lanzador
firma**, fake transporte/redirector real; biblioteca/helper compilados. Driver
y operación firma intactos, suite12 previa vigente, no repetir por documentos.
Siguiente paso: comparar acceso PrKDF7002 con metadatos nativos ya obtenidos,
sin escanear archivos ajenos ni pedir PIN. Consulta actual ya respondió el
siguiente comando; no repetir idéntica. JCOP4/RSA3072 pendientes. Git autorizado
pero montaje.git solo lectura sigue impidiendo crear commits si persiste.

**Resultado anterior: READ4109 -> SELECT7001 bloqueado, 20:12:16 UTC.**
Usuario hizo commit `e37cff1`, principal limpio al comenzar; OpenSC `3dfef6704`
intacto. Agente preparó/ejecutó modo independiente sin PIN/GUI
`nix develop --offline -c make inspect/jcop4-idopte-read4109 READER=0`.
Informe `local/jcop4-idopte-read4109-4qFZ03Qz/report.txt`: inventario/compilación
correctos, comparison_atr=known_jcop4, 20 APDU transmitidas 0/9000.
SELECT4109 -> 0/9000, READ4109 con Le17/offset0 -> 0/9000; contenidos omitidos,
no longitud devuelta en traza. Después SELECT/READ bootstrap sin FID registrado
(no atribuir archivo), luego **SELECT EF7001 bloqueado antes de transmitir**.
module_exit_status=1/observer_result=events_recorded por guard, sin respuesta
de tarjeta a SELECT7001. No PIN/VERIFY/MSE/GA/PSO/escritura ni firma.
ODF anterior identifica 7001=auth_objects: directorio de metadatos de
autenticación de esta tarjeta, no una credencial ni una operación VERIFY.
4141/4109 siguen de propósito desconocido; no asumir CA/PACE/no canal por orden.

Nuevo --idopte-read4109/target inspect/jcop4-idopte-read4109, opt-in separado
JCOP4_DIAGNOSTIC_READ4109=1 solo junto al READ4141 dentro de Idopte inventario.
Default/compare/spy mantienen READ4109 bloqueado. Solicitud caso-2 básico corto
o extendido, Le exactamente17/offset0, una llamada por proceso sin reposición.
No cambiar Le del proveedor. Buffer temporal19, tamaño2..19 y SW9000/6282;
otro SW/continuación/transporte/tamaño/logfallido detiene, sin GET RESPONSE,
reintento ni copia de respuesta al log. Opt-in inválido/sin padre/mixed-spy
bloquea todo. Launcher inventario/otros modos y firma limpian variable externa.
Verificación ATR/contexto/secuencia, límite READ4141 y prohibición PIN/firma
intactos. Informes privados 0700/0600. No incorporar local/ a Git.
Pasaron **52 observador/decoder/constructor, 47 lanzador y 6 lanzador firma**,
transporte ficticio/redirector real; biblioteca compilada. Helper público y
operación de firma sin cambios, suites58/12 previas vigentes, no repetir.
Siguiente trabajo: estudiar acceso AODF7001 del mismo AID, con precedente
ODF/PKCS#15 actual; no repetir esta consulta idéntica ni pedir PIN por inercia.
JCOP4/RSA3072 pendientes. Agente autorizado a Git/no PIN/no GUI; montaje.git
solo lectura sigue impidiendo commits en este entorno si no cambia.

**Resultado anterior: Idopte SELECT 4109 -> READ bloqueado, 20:04:49 UTC.**
Principal `40decb2`, OpenSC `3dfef6704`, ambos limpios al comenzar. Usuario hizo
commit anterior. Agente ejecutó el modo compare actualizado sin PIN/GUI:
`local/jcop4-idopte-compare-BCIdSIvC/report.txt`. Compilación/inventario correctos,
comparison_atr=known_jcop4; 17 APDU transmitidas 0/9000, incluidas dos READ
acotadas de 4141. **SELECT 4109 known_4109_metadata_only -> 0/9000**; siguiente
**READ BINARY 00B00000 bloqueado antes de transmitir**. No SW de tarjeta a ese
READ, ni contenido; module_exit_status=1/observer_result=events_recorded por
bloqueo propio, no rechazo de tarjeta ni VERIFY6982. No PIN, MSE/GA/VERIFY/PSO.

Modo compare permite SELECT4109 exacto únicamente con ATR de conexión validado,
ADF conocido/9000, 4141 seleccionado y lectura acotada previa; no READ4109.
SELECT revoca permiso de lectura anterior; warning/61xx/fallo no habilitan
lectura ni conservan ADF. Default discovery/spy siguen bloqueando SELECT4109.
comparison_scope=read4141_max400_select4109_only_no_recording_no_authentication.
No nueva variable, respuestas o campos identificables registrados.
Pasaron **44 pruebas observador/decoder y 45 lanzador**, fake transporte y
redirector real; biblioteca real compilada. Helper/firmador/driver sin cambios;
58 helper y 6 lanzador firma previas vigentes, no repetir por documentación.

Siguiente investigación: Idopte necesita leer 4109 durante enumeración; propósito
desconocido, no probar PIN nuevamente. Si se prepara una excepción, separar
READ4109 del default, limitarlo a los 17 bytes conocidos y a su forma solicitada,
sin registrar contenido, sin autenticación/firma/escritura. No habilitar por
SELECT9000/tag86 ni copiar protocolo ZIP. La consulta idéntica ya respondió qué
sigue; no repetirla sin nuevo diagnóstico. JCOP4/RSA3072 siguen pendientes.
Git continúa limitado por montaje de `.git` de solo lectura: autorización del
usuario existe, commits deben ejecutarse fuera de ese montaje si persiste.

**Resultado anterior: SELECT 4109, 13 de octubre de 2026, 20:00:55 UTC.**
El usuario creó `32616a2` para el diagnóstico anterior, Git limpio al comenzar;
OpenSC `3dfef6704` intacto. Agente ejecutó
`nix develop --offline -c make inspect/jcop4-file4109 READER=0`.
Informe `local/jcop4-file4109-LKNy75nY/report.txt`: compilación/inventario,
conexión/contexto/lock/EF.ATR/EF.DIR correctos. SELECT 4109 -> transporte
0/**9000**, transparente/**17 bytes**, security_direct_tags=86,
security_format=profile_uninterpreted, standard_compact_read=uninterpreted.
Unlock/proceso 0. No READ 4109 ni binding/PIN/VERIFY/MSE/GA/PSO.
El helper selecciona ADF conocido anunciado y 4109; no relee 4141 ni reproduce
el estado interno entero de Idopte. Existencia/control confirmados en ese
contexto; propósito y codificación 86 desconocidos. No inferir archivo público,
CAN/PACE/CA/clave ni causa de 6982 por tamaño/tag/orden. Valores propietarios,
identificadores personales y respuestas brutas omitidos.

Nueva opción --file4109 y target inspect/jcop4-file4109; analizador compartido
con --file4141, valida 83 contra el FID concreto y rechaza otros FID. No sondeo
arbitrario, reintento/GET RESPONSE ni nueva lectura. Pasaron **58 comprobaciones
del helper y 45 del lanzador**; compilación real. Observador/permisos y firma
intactos; no repetir suites sin cambios. No captura Idopte adicional.
Siguiente paso: decidir observación del proveedor después del SELECT 4109,
con eventual READ separado/acotado a 17 bytes sin registrar contenido, si es
necesario para una hipótesis concreta; no ampliar READ automáticamente por 9000.
El guard Idopte todavía bloquea SELECT/READ 4109. No pedir PIN por esta consulta.
JCOP4/RSA3072 y firma PKCS#11/PDF pendientes. Ver docs/jcop4.md.

**Comparación ejecutada por el agente: 13 de octubre, 19:55:02 UTC.**
Informe `local/jcop4-idopte-compare-ReO2RheL/report.txt`, principal `3e24829`
con cambios del diagnóstico pendientes de commit; OpenSC `3dfef6704`, intacto.
Compilación/inventario correctos, comparison_atr=known_jcop4. SELECT 4141 y
dos READ BINARY básicos (`00B00000`, luego `00B000DF`) -> transporte 0/SW
9000. Contenidos omitidos; no afirmar que se leyeron los 400 bytes completos
porque la traza no registra longitudes. Después Idopte intentó SELECT EF
**4109**, bloqueado antes de transmitir: sin respuesta de tarjeta ni metadatos.
module_exit_status=1/observer_result=events_recorded corresponde a ese bloqueo,
no a rechazo de READ 4141. No VERIFY, MSE, GA, PSO ni PIN. No atribuir 4141/4109
a CA/PACE/llave pública por su orden; la consulta solo enumera slots.
No referencias de 4109 en fuentes locales revisadas/ZIP; búsqueda pública sin
definición aplicable. No repetir la consulta sin cambiar una hipótesis concreta.
Siguiente paso técnico: preparar SELECT-only 4109 bajo el contexto observado
para obtener metadatos antes de decidir READ, sin habilitar lecturas generales.

El agente intentó Git tras autorización: creación de `.git/index.lock` falló
con **Read-only file system**. El sandbox permite leer `.git`, no escribirlo;
no hay mecanismo de aprobación de sandbox disponible. No se creó commit.
Documentación y código sí escritos; `git diff --check` correcto. Si continúa
este montaje, el usuario debe ejecutar el commit preparado o habilitar escritura
de `.git` en el entorno. No confundir este límite técnico con falta de permiso.

**Comparación experimental Idopte/APDU preparada, posterior a 6982.**
Principal `3e24829` / OpenSC `3dfef6704` al comenzar. El usuario pidió comparar
APDU reales de libidop11.so con el ZIP y estudiar necesidad de pasos/variantes
aunque los bytes no estén completamente documentados. Esta instrucción permite
preparar una nueva observación acotada; **la falta de manual no debe bloquear
toda investigación empírica**. El agente puede ejecutar el diagnóstico sin PIN
y crear sus commits, conforme a la autorización vigente. No firma JCOP4 nueva.

Comparación en docs/jcop4.md: Idopte seleccionó 4141 y su READ fue bloqueado
por nuestro guard, ZIP SM busca D004 (137 bytes supuestos), helper firma omite
ambos. No tenemos aún una traza de preparación autenticada de Idopte ni
evidencia para asignar 4141 a CA. El orden VERIFY/MSE ya falló en ambas variantes.
No repetir PIN por esa hipótesis ni usar 4FGWOtpI.

Nuevo **make inspect/jcop4-idopte-compare READER=0**, --idopte-compare: usa
exclusivamente Idopte --show-info --list-slots, stdin vacío y guard con opt-in
**JCOP4_DIAGNOSTIC_READ4141=1**, dentro de ese proceso. Default y modo spy
conservan bloqueo READ 4141. **Excepción nueva de lectura para comparación,
no deducción de permiso público por 86=00FFFF**: Idopte recibe el contenido,
observador no lo registra. Solo después de SELECT básico exacto 4141 en ADF
conocido/9000. En SCardConnect/Reconnect verifica **ATR de la conexión real**
con SCardGetAttrib(ATR_STRING), sin registrar bytes; ausente/distinto detiene
antes de APDU. --list-slots enumera todos los lectores, READER solo selecciona
inventario previo; no atribuir por índice. READ BINARY básico, sin SFI ni datos de entrada, corto o extendido;
offset inicial 0 y siguiente contiguo a bytes devueltos, **máximo dos lecturas
y 400 bytes solicitados en todo el proceso**. SELECT no repone offset/budget ni
permite releer; reconexión después de READ detiene definitivamente, otro handle
también. Fallo de escritura del registro bloquea antes de READ. Respuesta temporal <= Le+2, capacidad cliente adecuada,
9000/6282 exclusivamente; otro SW/continuación/transporte/tamaño detiene, sin
GET RESPONSE de esa lectura. Ningún READ RECORD, otro archivo nuevo, escritura,
VERIFY incluso vacío, MSE/GA/SM/PSO ni control de PIN llega al backend.

Raw spy y modo de lectura son incompatibles; opt-in inválido bloquea. No raw
ni captura estándar pcsc-spy/FIFO. Si se bloquea MSE exactamente reconocible,
registra solo blocked_mse_profile=ca_ecdh_aes128/ca_ecdh_3des y caso corto 3/4,
sin volcar buffers ni transmitirlo. No reconoce otros valores por analogía.
Lanzadores de inventario y firma limpian la nueva variable externa. Registros
local/jcop4-idopte-compare-*/ privados: report.txt y public-headers.txt, con
cabeceras/SW y etiquetas fijas, **sin respuestas de READ**. Inventario Idopte
puede incluir identificadores; no publicar informes completos ni agregarlos a Git.
Error de módulo por siguiente bloqueo sigue esperable: distinguir SW real de
read4141_sw y rechazo del guard. No ampliar siguientes permisos automáticamente.

Pasaron **41 comprobaciones del observador/decoder, 43 del lanzador y 6 del
lanzador de firma**, fake transport/redirector real, ninguna tarjeta/Idopte real.
Biblioteca de diagnóstico compilada. Helper público (54 previas), operación de
firma (12 previas) y driver intactos; no repetir esas suites sin cambios. Primera
ejecución de integración falló por fixture que solo permitía D003; actualizado
para esta excepción y luego suites completas correctas, no error de hardware.
Siguiente ejecución por el agente: misma JCOP4 y otros clientes cerrados,
**nix develop -c make inspect/jcop4-idopte-compare READER=0**. No introducir
PIN, no usar guard con firmador. Revisar READ/SW/siguiente bloqueo y comparar
antes de elegir una sola variante de protocolo. Ver plan en docs/jcop4.md.

**Resultado VERIFY antes de MSE: 13 de octubre de 2026, 19:00:58 UTC.**
Registro anterior; próxima investigación experimental actual descrita arriba.
Informe **local/jcop4-signature-6m61lek4/report.txt**, principal `3acd6b9`,
OpenSC `3dfef6704`; Git principal/driver limpios al comenzar salvo PDF del usuario
sin seguimiento. mode=experimental_apdu_sha256_rsa_verify_first, lector índice
0 (no inferir modelo del lector de ese índice). profile_metadata=validated,
certificate_rsa_bits=2048, authentication_order=verify_before_mse.
Estado sin credencial -> transporte 0/**63C7**; único VERIFY real -> transporte
0/**6982**. **No MSE ni PSO**, reset/unlock 0, helper/exit 1. El cambio de orden
no resolvió el rechazo; no atribuirlo al MSE anterior ni al formato PSO.
63C7 es anterior al intento, no medición posterior del contador. No demuestra
PIN incorrecto/correcto ni que una condición de canal específica sea obligatoria.
No repetir este diagnóstico ni solicitar otra variante de PIN por inercia.

Revisión estática adicional del ZIP sin importar/ejecutar/extractar: en
chipdoc/sm.py build_sm_apdu calcula mac y **no lo incorpora** al APDU; transmite
01+ciphertext sin DO87/DO8E. parse_sm_response retorna 9000 fijo sin decodificar
DO99. establish_secure_channel en driverfd/signer.py devuelve True después de
inicializar claves locales, sin intercambio de confirmación. SELECT bajo SM
es constante 0C A4 00 00 00 00 21, sin MAC ni claves de sesión; no prueba canal.
GENERAL AUTHENTICATE lleva CLA10 (fragmento encadenado ISO) sin comando final
en esas funciones; no equiparar un posible 9000 con ejecución completa.
kdf_bsi usa contadores de un byte y claves 3DES24/SSC derivado; hay alternativa
de contador de cuatro bytes, pero no corrige el resto ni contradicción OID AES.
Contraste primario OpenPACE eac_kdf.c/eac.c/eac_ca.c y OpenSC src/sm/sm-iso.c,
src/libopensc/apdu.c. **No trasladar ni reparar a ciegas para enviar PIN**.

Inspección estática limitada del módulo instalado libidop11.so (sin cargarlo):
no literales exactos OID CA/AES128, CA/3DES, MSE del ZIP o SELECT D004/4141;
sí nombres PACE/ECDH. No identifica ruta usada ni demuestra ausencia de soporte:
puede construir bytes en tiempo de ejecución. No copiar código propietario.
Búsqueda pública adicional solo recuperó referencias a manuales, no definición
de protocolo aplicable. Falta corroborar preparación del canal/perfil con guía
o registro histórico saneado (sin PIN/claves/datos personales). El usuario
confirmó que **solo conserva el ZIP**, sin registro externo. No volver a
preguntar por ese registro/comando: candidato identificado por AST, ejecución
histórica y preparación no pueden corroborarse mediante lo conservado.
Nombres estáticos del módulo instalado incluyen CA/ECDH AES128/192/256 y 3DES,
PACE y PK-ECDH; es soporte genérico, no selección de protocolo de esta tarjeta.

Solo documentación en esta revisión, driver/helper/guard sin cambios; las
12+6 comprobaciones de firma anteriores siguen vigentes, no repetirlas.
JCOP4 sin firma válida medida, PDF/PKCS#11/RSA3072 pendientes. Próximo paso
depende de evidencia de preparación; **no nuevos READ 4141/D004, GA/SM o PIN**
por los mensajes del prototipo. Mantener privado local/, nunca usar 4FGWOtpI,
no hardware ni Git por el agente. Dar comandos de commit documental principal.

**Revisión APDU del ZIP y diagnóstico VERIFY antes de MSE preparado.**
Registro anterior; diagnóstico ya ejecutado, no repetir como próximo paso.
Principal `1f4d9ae` / OpenSC `3dfef6704` al comenzar. El usuario aclaró que
la firma exitosa de la otra JCOP4 fue **solo APDU, sin Idopte**, y pidió
identificar el comando mediante el ZIP y evaluar rutas aunque se omitan
campos ajenos a la firma. Esta instrucción sustituye la pregunta pendiente
sobre Idopte y permite preparar el diagnóstico descrito aquí; no ejecutar
hardware por el agente ni ampliar el guard público.

Revisión de las CLI/AST: candidato concreto **python3 sign_llavefirma.py --sign**,
única CLI APDU encontrada que llama una secuencia completa de firma. No demuestra
que fuera el comando histórico exacto. driverfd/signer.py declara --sign pero
no lo despacha; sign_chipdoc_lite.py solo hace probes. La ruta CLI PKCS#11 queda
descartada por la aclaración del usuario. El candidato hace VERIFY antes de MSE,
MSE SET AT con OID RSA/SHA-256 y PSO con DigestInfo; no verifica la firma con
certificado. Su helper trunca PIN a 8 bytes y rellena FF, y prueba referencias
01/81: **no reproducir truncamiento/relleno, barridos ni aceptar solo SW1=90**.

Nuevo **make sign/jcop4-verify-first READER=0**: misma validación de ATR/AID,
PKCS#15 nativo, clave 91/RSA2048, PIN 1 UTF-8 6..14 sin relleno y certificado;
solo cambia el orden autenticado a VERIFY -> MSE -> PSO. Conserva consulta de
estado sin PIN, MSE 00 22 81 B6/91022891 y PSO con digest SHA-256 de 32 bytes.
No afirmar réplica completa del ZIP: su MSE y DigestInfo son distintos.
Un único intento real; cualquier VERIFY distinto de 9000 detiene antes de
MSE/PSO, sin variantes/reintentos. Borra PIN antes de MSE y resetea tras intento,
incluso fallo. No nuevos READ 4141/D004, canal SM, PIN admin o selección posterior
al PIN. Lanzador limpia trazas, LD_PRELOAD y delegados PC/SC externos en ambos
modos. Firma técnica local, no PDF; éxito exige OpenSSL con certificado asociado.
Informes/artefactos privados local/jcop4-signature-*/. Modo anterior conserva
orden MSE -> VERIFY. Driver y observador v6 intactos.

Pasaron **12 comprobaciones de operación y 6 de lanzador**, sin hardware,
incluidas parada antes de MSE tras VERIFY 6982, fallo MSE posterior, borrado PIN,
reset, aislamiento de entorno y modo explícito. Helper real compilado offline.
No repetir suites de observador/helper público sin cambios. Siguiente intervención:
misma JCOP4, GUI/otros clientes cerrados, **nix develop -c make
sign/jcop4-verify-first READER=0**, una ejecución; revisar informe si falla antes
de otro PIN. Última evidencia autenticada sigue siendo 5Dpadr1N (VERIFY 6982),
no hay firma JCOP4 nueva. Nunca usar 4FGWOtpI. Dar comandos de commit del
principal; agente no escribe Git; preservar PDF no seguido en raíz.

4141/86, etiquetas de presentación y PuKDF pueden omitirse en la ruta mínima;
el helper ya evita depender de ellos. No omitir resultado VERIFY, asociación
clave/certificado, algoritmo/padding o validación criptográfica. Si el nuevo
VERIFY da 6982, la siguiente hipótesis sigue siendo preparación de canal CA/SM
o diferencias de personalización, no ignorar ese estado ni otra prueba de PIN.
PACE/CAN de eOI y CA/ECDH del ZIP siguen sin corroboración para esta tarjeta;
la variante SM del ZIP contradice OID AES con helpers 3DES. Soporte JCOP4,
PKCS#11, PDF y RSA3072 pendientes. Detalles y rutas en docs/jcop4.md.

**Investigación estática posterior: eOI y ZIP aportado por el usuario.**
Registro anterior; la aclaración y el próximo paso actuales están arriba.
Principal `4f5040f` / OpenSC `3dfef6704` al comenzar; driver/submódulo sin cambios.
`ChipDocLite` es constante eoi_model (card-eoi.c:48), devuelta por GET_MODEL;
no lectura del nombre real del applet. eoi_match_card solo compara ATR;
eoi_init consulta CA 01 16. eoi_pin_cmd:400 abre SM/PACE con CAN antes de
VERIFY sin exigir contactless. pkcs15-eoi.c:76 obtiene CAN por ruta del objeto
Card CAN y espera blob de 24 bytes. No referencia fija 4141 ni parser propio
para tag 86; hereda process_fci ISO. Pista de canal previo, no prueba CR.

Usuario añadió **local/driverfd.zip**, 53 archivos: revisión de texto/AST sin
extraer/importar/ejecutar scripts o binarios. No copiar implementación privativa.
Código dice investigación 2026-08-10/BCCR ChipDoc 3.2 y usa el AID JCOP4 conocido;
no incluye capturas originales/outputs ni evidencia de firma verificada. **Nueva
hipótesis: Chip Authentication ECDH + SM**, distinta de PACE/CAN de eOI.
Llama D004 EF.CardAccess y supone clave P-521; nombre/función aún no corroborados.
No menciona 4141 en fuentes revisadas; no identifica su propósito/ACL.

Hallazgo determinista: MSE SET AT constante del ZIP, interpretado como caso-4
corto (Lc 12 + datos 12 + Le 02), contiene tag80/OID
**0.4.0.127.0.7.2.2.3.2.2 = id-CA-ECDH-AES-CBC-CMAC-128**, según OpenPACE.
No es PACE ni el 3DES del nombre del prototipo. La constante OID separada añade
otro arco .2 (11 bytes), inconsistente con los 10 bytes del TLV del comando.
**No afirmar APDU malformado por el byte restante: puede ser Le.** Helpers
criptográficos usan 3DES; no implementar ese protocolo por los comentarios.
probe_sm_with_pubkey acepta True si solo SW1=90 y formato inesperado sin validar
MAC; por tanto el mensaje SM establecido no prueba acuerdo de claves. Capturas
citadas y clave pública fallback están ausentes en ZIP; no buscar/usar secretos.

**Confirmación del usuario:** los intentos correspondían a **otra JCOP4** y
sí se logró firmar satisfactoriamente. Registrar como antecedente exitoso de
otro ejemplar; no como firma de la tarjeta actual ni verificación independiente
por el agente. No extrapolar ATR/AID/versión/RSA/personalización no aportados.
Ruta exacta aún pendiente: ZIP incluye src/signer/cli_sign.py usando Idopte,
sign_llavefirma.py con APDU/VERIFY directo, y driverfd/signer.py con SM propio.
Una firma por Idopte no valida la reproducción Python de CA/SM; una firma directa
sin SM sería un antecedente distinto. No atribuir éxito a ninguna por inercia.
Preguntado qué script/comando produjo esa firma, sin solicitar repetirla.
Próximo trabajo: identificar la ruta exitosa y después corroborar MSE/preparación
con evidencia original saneada sin PIN/llaves de sesión antes de adaptar CA.
OpenSC sm-eac tiene perform_chip_authentication_ex/CA v2, pero hay que comprobar
compatibilidad: el prototipo comenta respuesta 7C00 vacía y no prueba tokens/MAC.
Mantener bloqueados READ 4141/D004 nuevo, VERIFY/MSE/GA/SM; no nuevos comandos ni
PIN por esa pista. Documento técnico actualizado, sin nuevas pruebas del driver
ni hardware/Git por el agente. Ver docs/jcop4.md; dar commit del principal.

**JCOP4, última revisión del 13 de octubre de 2026:**
`local/jcop4-idopte-spy-wYs4iWG8/report.txt`, **17:33:49 UTC**, principal
`4058fdf`, OpenSC `3dfef6704`; ambos Git limpios al comenzar. Compilación,
inventario, contexto/conexiones y transporte de las 14 APDU permitidas correctos;
consulta de capacidades 0. Lector 0 **Bit4id miniLector-s 00 00** en este
informe, distinto del OMNIKEY anterior; ATR JCOP4 conocido confirmado. No
asumir que el lector actual es el histórico ni inferir PIN-pad por su nombre.
**La captura y el decoder funcionaron**:
spy_decode_status=0, spy_result=select4141_control_data_only,
observer_result=events_recorded. module_exit_status=1 ocurrió tras bloquear
READ BINARY 00 B0 00 00 antes de transmitir. No es rechazo de tarjeta a READ,
ni el VERIFY 6982 anterior. Segunda conexión 0 no rehabilitó el transporte.

Un intercambio SELECT capturado: **00 A4 02 00 02 41 41 00 -> 9000**.
Respuesta de control BER-TLV exterior **6F (FCI)**, no 62 en esta captura;
campos directos 80=0190 (400 bytes), 82=010000 (transparente), 83=4141 y
**86=00FFFF**. Estos campos de control no son el contenido del EF. Solo se
extrajeron esos campos/SW de la captura privada; no volcar raw/txt completos
ni inventario con identificadores a Git/chat. Archivos 0600 bajo directorio
privado. No READ 4141, PIN, autenticación o firma; no hardware por el agente.

ISO/IEC 7816-4:2005, tabla 12, define 86 como seguridad en formato propietario:
fuente primaria y límites en docs/jcop4.md. **00FFFF no tiene aquí una
interpretación estándar de READ/UPDATE/DELETE**, ni demuestra archivo público,
PACE/CAN, clave pública o causa de VERIFY 6982. ISO default almacena el atributo
sin interpretarlo. El formato de tres bytes de MyEID es de otro perfil, no
precedente para atribuir reglas ChipDoc. Búsquedas por secuencia/ChipDoc y
revisión de CIE no obtuvieron la definición aplicable; no prueba inexistencia.

**Próximo paso:** obtener la definición ChipDoc de EF 4141/tag 86 y del canal
previo a VERIFY, preferentemente User Guide Manual ref. 678510 o documentación
del perfil BCCR. Pregunta textual enviada al usuario sobre acceso a esos
manuales; respuesta pendiente. Durante la revisión apareció
`anssi-cible-cc-2021_20v2-m01.pdf` sin seguimiento en raíz: Security Target Lite
rev. 1.2 del 13 de julio de 2022. Revisado localmente; cita ref. 678510 y PACE,
pero no aporta definición 4141/tag 86. Preservar ese archivo del usuario; no
agregarlo al commit documental automáticamente. No contactar terceros. **No repetir la captura
ni pedir otro PIN**: ya recuperó los bytes buscados y no resolverá por sí sola
su significado. READ 4141 sigue bloqueado; no ampliar permisos por 00FFFF.
No nuevos comandos/variantes/otros CardAccess por inercia. Solo documentación
en esta revisión; principal/driver sin cambios de protocolo, OpenSC intacto.
Las 30 pruebas observador/decoder, 40 lanzador y 54 helper previas siguen
vigentes; no repetir por documentación. Dar comandos de commit del principal;
el agente no escribe Git. JCOP4/RSA 3072 siguen pendientes.

**Informe anterior de JCOP4, 13 de octubre de 2026:**
`local/jcop4-file4141-9gvwdH1n/report.txt`, **16:53:35 UTC**, principal
`e4a227e`, OpenSC `3dfef6704`; ambos Git limpios al comenzar. Inventario,
compilación, conexión/aplicación correctos. SELECT 4141 -> transporte 0/SW
9000, transparente/400 bytes; **security_direct_tags=86**, security_format=
profile_uninterpreted y standard_compact_read=uninterpreted. Unlock y proceso
0. Sin READ/binding/PIN/autenticación. No error nuevo de tarjeta ni intento de PIN.
Entre los tags directos que busca el helper no aparecen A1/9C/8C/8B/AB/62/6F;
86 es la única forma reconocida, **no demuestra un único atributo físico ni
que no existan otros tags omitidos**. No hay A1 que investigar en este resumen.
No aplicar estructura o política JCOP3, ni identificar 4141 como público,
CAN, llave, PACE obligatorio o causa de VERIFY 6982. No se registraron valores
de 86; la política y el propósito del archivo siguen desconocidos.

**No repetir el resumen SELECT-only ni el descubrimiento sin respuestas.** La
última consulta ya respondió la duda estructural y no permite decidir READ.
El nuevo modo de captura descrito abajo sí recoge información antes omitida.
Se mantiene observador v6 bloqueando READ 4141. El error Idopte mEpNvPwv
fue ese bloqueo antes de transmitir; no es el VERIFY 6982 de 5Dpadr1N.
El usuario confirmó una firma previa satisfactoria de esta misma JCOP4 con
Idopte instalado; no se pidió ni verificó otra firma/PDF por el agente.

Investigación pública adicional no identificó 4141 ni la codificación de 86
para esta personalización. ISO default almacena 86 como atributo de seguridad
sin interpretarlo; los drivers iasecc/eOI/ChipDoc italiano no constituyen
especificación de este perfil. No probar otra ubicación CardAccess, nuevos
archivos/GET DATA/variantes PIN sin precedente aplicable por inercia.
La bibliografía de NXP/ANSSI-CC-2025/03, sección 9.2, identifica exactamente
**ChipDoc 3.2 User Guide Manual ref. 678510**, Crypto Guide ref.
CDv3.2_1_03210_ChipDoc3.2_Crypto_Guide y SSCD Personalization Guide ref. 678712;
referencias en docs/jcop4.md. No se obtuvo el contenido de esos manuales:
una referencia bibliográfica no es especificación APDU ni evidencia CR.
El usuario aportó de nuevo la cible y propuso **pcsc-spy sin PIN**. Es una nueva
solicitud de observación; no se interpreta como permiso para autenticar o leer
4141. Preparado **make inspect/jcop4-idopte-spy READER=0**, que reutiliza la
consulta pública Idopte --show-info --list-slots y el guard v6. No más espera
por la pregunta anterior sobre documentos como primer paso. El PDF documenta
PACE con PIN/CAN/MRZ (1.3.2.1.3), pero no identifica por sí solo el protocolo CR.

Nuevo modo optativo de captura: JCOP4_PUBLIC_SELECT_SPY_TRACE, solo dentro
del proceso Idopte. **Captura exclusivamente el SELECT 4141 ya permitido y su
respuesta de control**, no los otros APDU ni el contenido del archivo.
Coincidencia CLA 00/A4/02/00/Lc 2/FID 4141/total 7 u 8 después de aprobación
del guard y transmisión. Respuesta <=258 bytes incluyendo SW; error de
transporte/buffer inválido/superior al límite omite los datos. Incluye atributos
86 del FCP para investigación; no ampliar por ello READ/PIN ni aplicar política
JCOP3. Se conserva public-headers.txt v6 sin payloads; permisos de comandos
idénticos. VERIFY incluso vacío, MSE/PSO/SM/escrituras/lecturas desconocidas
se bloquean antes de transporte y **no llegan a la captura**. Primer bloqueo
continúa definitivo. No GET RESPONSE adicional para completar la captura.

**No carga libpcscspy ni usa su FIFO global:** el guard emite una traza limitada
compatible y el programa pcsc-spy real la decodifica al terminar. Handles,
PCI y prefijo de hilo son placeholders normalizados, no valores PC/SC reales;
fechas/command/response/resultado sí observados. No confundirla con captura
completa del proceso o del tráfico de tarjeta. Python 3 agregado al flake;
se invoca el script con ese intérprete porque pcsclite.dev trae shebang
/usr/bin/python3, inexistente en NixOS. Sin cambiar lock, HOME o archivos del
sistema. No precarga, login/sesiones/objetos solicitados, PIN ni firma.

Archivos privados local/jcop4-idopte-spy-*/: report.txt/public-headers.txt y
select4141-spy.raw/select4141-spy.txt. No se anexan respuestas a report.txt.
Raw O_EXCL/NOFOLLOW/0600 obligatorio para este modo; destino existente,
symlink/no disponible bloquea antes de transmitir. Fallo de escritura detiene
más transporte. El lanzador elimina env externo del modo de captura antes del
inventario y en otros modos. Falta de herramienta/intérprete, raw vacío o
error del decodificador detiene sin fallback; detecta errores de hilo aun
cuando Python sale 0. Conserva por separado estado del módulo y decodificación.
**Idopte probablemente terminará 1 al intentar READ 4141**; no prueba fallo
de captura o rechazo de tarjeta. Revisar spy_result/spy_decode_status y raw
SELECT, no repetir por el estado del módulo. La respuesta puede contener
campos identificables: mantenerla privada y no volcarla completa al chat/Git.
En la revisión extraer solo estructura/tamaño/FID/tag 86/SW; no nombres,
seriales o valores ajenos por inercia. Condición de VERIFY 6982 sigue pendiente.

Pasaron **30 comprobaciones del observador/constructor/decoder** y **40 del
lanzador** sin tarjeta, incluido pcsc-spy real, redirector real/plugin local
con transporte falso, exclusión de READ/PIN y permisos/fallos de log.
Biblioteca real compilada. Helper y sus 54 pruebas anteriores intactos; no
repetir suite por documentación. Principal 69a4310 y OpenSC 3dfef6704 al
comenzar; submódulo/driver sin cambios. Sin hardware/Idopte/PIN/Git por el
agente. Dar comandos de commit del principal; JCOP4/RSA 3072 siguen pendientes.

**Siguiente intervención:** misma JCOP4 y firmador/otros clientes cerrados:
`nix develop -c make inspect/jcop4-idopte-spy READER=0`.
Compila solo la biblioteca de diagnóstico. No introducir PIN ni usar el modo
con firmador o firmas. Revisar la respuesta de control del único SELECT
capturado antes de decidir otros comandos. No contactar terceros ni capturar
una firma autenticada con pcsc-spy estándar. Preservar capturas; nunca usar
4FGWOtpI como evidencia.

Informe anterior de metadatos, antes de distinguir el tag directo:
`local/jcop4-file4141-o5gvxntR/report.txt`, **16:38:38 UTC**, principal
`3f894c6`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario,
compilación, conexión y aplicación correctos. SELECT 4141 -> 0/9000,
transparente/400 bytes, **security_format=profile_uninterpreted,
standard_compact_read=uninterpreted**, unlock y proceso 0. No READ/binding/
PIN/autenticación. No hubo un nuevo rechazo de tarjeta ni intento de PIN.
El resumen agrupa **86/A1/9C**: acredita al menos uno de esos tags directos,
no cuál, su número/estructura/valores ni el contenido/política del archivo.
No identificar A1 como hecho, no asignar política JCOP3 ni inferir READ siempre,
protección de archivo, PACE/CAN obligatorio o causa de VERIFY 6982. Puede haber
codificaciones estándar anidadas que esta consulta no interpreta.
Búsquedas públicas y de nombres del módulo instalado no identificaron 4141;
no equivale a ausencia de documentación/soporte. **Se mantiene el bloqueo READ
4141 del observador v6**. No repetir los dos últimos diagnósticos por inercia.
El error Idopte mEpNvPwv ya se explicó por READ bloqueado antes de transmitir;
separarlo del único VERIFY autenticado 6982 de 5Dpadr1N.
**El usuario confirmó que esta misma JCOP4 ya firmó satisfactoriamente con
Idopte instalado.** Es testimonio de una prueba previa; no se ejecutó ni
verificó otra firma/PDF por el agente. La tarjeta tiene una ruta de firma
funcional con Idopte, pero eso no identifica el protocolo ni prueba el PIN
usado en 5Dpadr1N. No pedir una nueva firma para confirmar este dato.

Preparado resumen estructural más preciso para **la misma consulta --file4141**,
sin nuevos APDU: conserva security_format/standard_compact_read y añade
security_direct_tags con exclusivamente nombres fijos 8C/8B/AB/86/A1/9C/62/6F.
Si hay un único A1, valida sus TLV inmediatos y resume los mismos tags conocidos;
los demás solo como other_tags=present_values_omitted, sin identificadores,
valores, referencias, tamaños de atributos ni buffers. No recursión ni selección
adicional. A1 opaco/malformado descarta evidencia parcial; duplicado no se
interpreta. Una única 8C inmediata, con descriptor transparente ordinario,
permite resumir exclusivamente su **codificación** READ (always/nonzero/
alternativas/propietaria/no declarada), no su función en este perfil ni política
global. Datos compactos inválidos o duplicados quedan uninterpreted. Nunca
identificar A1 como ACL de contacto ni 9C como contactless para JCOP4. Incluso
always en ese resumen no identifica contenido público ni autoriza READ.

Cambios actuales en principal: helper, regresiones y documentación; driver,
submódulo, observador v6 y lanzador intactos. Principal al comenzar c244218,
OpenSC 3dfef6704. Pasaron **54 comprobaciones del helper** y su compilación real
contra OpenSC offline. Las 25/35 del observador/lanzador anteriores siguen
vigentes, sin repetir suites sin cambios. Sin hardware, Idopte, PIN ni Git por
el agente. Soporte JCOP4 y RSA 3072 aún pendientes.

**Siguiente intervención:** misma JCOP4, firmador/otros clientes cerrados:
`nix develop -c make inspect/jcop4-file4141 READER=0`.
Se necesita SELECT otra vez porque la captura anterior omitió/distinguió
insuficientemente esta estructura; no reintento autenticado ni nueva variante
APDU. Revisar los nuevos campos antes de decidir lecturas o autenticación.
**Mantener READ 4141 bloqueado**; no repetir descubrimiento Idopte todavía.
La confirmación de firma no autoriza READ desconocido, otro PIN, CAN/PACE ni
observación autenticada. No contactar fabricantes/terceros sin instrucción
expresa ni capturar APDU autenticadas con pcsc-spy estándar. Dar comandos de
commit del principal; el agente no escribe Git.

Informe anterior de Idopte, antes de revisar los metadatos de seguridad:
`local/jcop4-idopte-discovery-mEpNvPwv/report.txt`, **16:30:07 UTC**, principal
`e30c61e`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario,
compilación y contexto/conexión correctos. Observador v6: **SELECT AID conocido
known_chipdoc_application -> 0/9000**, SELECT EF/READ bootstrap permitidos
-> 0/9000, **SELECT 4141 known_4141_metadata_only -> 0/9000**, luego
**READ BINARY 00 B0 00 00 bloqueado antes de transmitir**. Catorce APDU
permitidas 0/9000; control de capacidades 42000D48 -> 0, respuestas omitidas.
Segunda conexión 0, sin rehabilitar transporte. Módulo 1, events_recorded.
El guard impidió leer el archivo; no hay respuesta de tarjeta a ese READ,
ni su contenido. No PIN/VERIFY/MSE/PSO. No es el rechazo autenticado 6982.
No repetir esta captura ni permitir lectura de 4141 por inercia.
El helper SELECT-only se amplía para revisar **metadatos de seguridad del FCP**,
no contenido de archivo. Mantiene exactamente la consulta --file4141 anterior,
ATR/AID/conexión/aplicación comprobados, sin READ, binding, Idopte o autenticación.
Resume formas directas compacta 8C, referenciada 8B, expandida AB y variantes
no interpretadas 86/A1/9C/plantillas de control anidadas. Ningún buffer/ACL/SCB/
referencia/valor propietario se registra. Para una única 8C directa y descriptor
EF transparente ordinario, interpreta solo grupos ISO AM/SC no propietarios:
READ siempre con SC 00, nonzero_uninterpreted para cualquier otro SC,
not_declared, alternatives_uninterpreted o proprietary_uninterpreted.
No interpreta PIN/CAN/SE ni asume perfil UICC/JCOP3 para CR. Grupos vacíos/
truncados detienen; múltiples atributos/formas o descriptor distinto impiden
interpretar READ. **Siempre es un resumen del atributo, no una política global
ni permiso de lectura**: ni siquiera always identifica contenido público.
Precedente público ETSI TS 102 221 9.2.4–9.2.5 enlazado en docs/jcop4.md.
Pasaron **49 comprobaciones del helper**, compilación real offline; no hardware.
Lanzador y observador v6 sin cambios: sus 35/25 comprobaciones anteriores
siguen vigentes. Driver/submódulo intactos. Siguiente intervención, misma JCOP4,
firmador/otros clientes cerrados: `nix develop -c make inspect/jcop4-file4141 READER=0`.
Se repite SELECT para obtener un resumen de atributos omitidos en el informe
previo, no por una nueva variante ni reintento de error. No READ ni PIN.
Revisar security_format/standard_compact_read antes de decidir otros pasos;
propósito de 4141 y condición faltante de VERIFY 6982 siguen pendientes.
Dar comandos de commit del principal; no escribir Git por el agente.

Informe anterior de SELECT directo, antes de observar el READ bloqueado:
`local/jcop4-file4141-Kzi6aI8S/report.txt`, **16:12:31 UTC**, principal
`e4c5d1e`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario,
compilación, conexión y aplicación correctos. **SELECT 4141 -> transporte
0/SW 9000, estructura transparente, tamaño 400 bytes**. Existe bajo el AID
ChipDoc conocido; su propósito/contenido/ACL no se identificaron. No READ
ni binding; unlock y proceso 0. No PIN ni autenticación. No equiparar selección
exitosa con archivo público o permiso para leerlo. ODF previo no lo referencia
en el bloque leído; las búsquedas públicas nuevas tampoco lo identificaron.
Preparado observador **v6** para la misma consulta pública Idopte: permite
exclusivamente SELECT 4141, CLA 00/P1 02/P2 00/Lc 2/total 7 u 8, después
de SELECT AID ChipDoc conocido en canal básico con SW 9000. Conserva ese
contexto solo al seleccionar EF bootstrap relativo conocido con éxito; MF,
raíz, ruta absoluta, otro canal, SELECT fallido o pendiente y conexión/
reconexión lo descartan. **Nunca habilita READ BINARY/RECORD de 4141**;
seleccionarlo elimina el permiso de lectura del EF anterior. GET RESPONSE
solo tras 61xx permitido y sin habilitar lectura/autenticación. Nueva observación:
etiquetas fijas public_select=known_chipdoc_application/known_root_application/
known_4141_metadata_only, derivadas exclusivamente de coincidencias con
constantes públicas; no AID/FID arbitrario, contenidos, ACL ni respuestas.
Las etiquetas preceden transporte: consultar SW para saber si SELECT tuvo éxito.
Permisos restantes de v5 intactos: no VERIFY/MSE/PSO/SM/escrituras, no controles
salvo capacidades vacías; primer bloqueo/error de transporte/69xx/otro
handle/límite 128 sigue definitivo. Pasaron **25 comprobaciones del observador/constructor y
35 del lanzador**, biblioteca real compilada, redirector PC/SC con transporte
ficticio y ninguna tarjeta/Idopte por el agente. Las 45 del helper previas
siguen vigentes, sin cambios de ese código. Driver/submódulo intactos.
Siguiente intervención: `nix develop -c make inspect/jcop4-idopte-discovery READER=0`,
misma JCOP4, firmador/otros clientes cerrados, sin PIN. Revisar etiquetas,
transportes/SW y primer bloqueo; **un error de Idopte sigue siendo esperable
si intenta leer 4141**. No permitir esa lectura ni pedir otro PIN por inercia;
VERIFY 6982 sigue pendiente. Dar comandos de commit solo del principal.

Informe anterior del mapa de directorios, antes de SELECT 4141:
`local/jcop4-odf-map-eAjoi575/report.txt`, **15:58:49 UTC**, principal
`458573f`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario,
compilación, contexto/conexión y aplicación correctos. SELECT 5031 y READ
BINARY -> transporte 0/SW 9000. Cuatro referencias del primer bloque:
**auth_objects=7001, private_keys=7002, public_keys=7004, certificates=7005**.
No se siguieron rutas; unlock y proceso 0. No binding, PIN ni autenticación.
**4141 no aparece entre esas referencias**; no demuestra ausencia global ni
completitud general del ODF. No identificarlo como AODF/CAN/archivo público.
Preparado **make inspect/jcop4-file4141**, diagnóstico distinto del observador:
default driver, ATR/AID conocidos y aplicación anunciada en EF.DIR. Selecciona
esa aplicación y envía únicamente **00 A4 02 00 02 41 41 00**, el SELECT EF
observado, con respuesta de control de hasta 256 bytes; no READ ni binding.
No sondeos de otros archivos ni ampliación de la lista del observador. SELECT
solicita información de control ISO, no contenido del EF; resume solo estructura
y tamaño de tags estándar directos de FCP/FCI, nunca nombres/ACL/campos
propietarios/respuesta bruta. La estructura/tamaño no acredita acceso de lectura.
9000 sin datos es diagnóstico válido, 6A82 significa no encontrado en **esta
aplicación/contexto**, no ausencia general. Otros SW, transporte, tamaño o
metadatos inválidos detienen sin GET RESPONSE/reintentos ni comandos posteriores.
Sin Idopte, VERIFY, PIN, MSE, PSO ni comandos administrativos. Inventario previo
solo EF.ATR/EF.DIR ya conocidos; stdin vacío, debug 0, informe privado
local/jcop4-file4141-*/report.txt. Pasaron **45 comprobaciones del helper y
35 del lanzador**, transporte ficticio; helper real compilado offline.
Las 20 del observador anteriores siguen vigentes; su código y permisos v5
no cambiaron. Driver/submódulo intactos. No hardware ni autenticación por el
agente. Siguiente intervención, misma JCOP4/otros clientes cerrados:
`nix develop -c make inspect/jcop4-file4141 READER=0`. Revisar selección y
metadatos antes de decidir cualquier lectura; no repetir Idopte ni pedir otro
PIN. VERIFY 6982 sigue pendiente. Dar comandos de commit solo del principal.

Informe anterior de descubrimiento, antes de consultar ODF:
`local/jcop4-idopte-discovery-OUj06FEQ/report.txt`, **15:45:03 UTC**, principal
`86e8ba9`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario/ATR,
compilación y conexión correctos. Consulta de capacidades **42000D48 ->
control_transport=0**; respuesta omitida, no demuestra qué capacidades anunció.
Trece APDU permitidas dieron transporte 0/SW 9000. Después del control:
SELECT EF/READ, SELECT AID, SELECT EF/READ, **SELECT EF 4141 bloqueado antes
de transmitir**. No SW ni contenido de 4141. Los FIDs/AID permitidos no se
registran; no atribuir lecturas/DF concreto solo por cabeceras. Segunda conexión
0, transporte sigue detenido; módulo 1/slot vacío/events_recorded, compatible
con el guard. No PIN, VERIFY ni firma. Búsqueda en código local y fuentes
públicas consultadas no identificó 4141; no asumir AODF/CAN/archivo público.
No ampliar permisos del observador ni repetir ese descubrimiento por inercia.
Preparado **make inspect/jcop4-odf-map**: usa helper público/default driver,
exige ATR/AID conocidos y AID anunciado en EF.DIR, selecciona aplicación y
únicamente **5031**; un SELECT sin respuesta FCP y un READ BINARY corto de
hasta 256 bytes, sin GET RESPONSE/reintentos/lecturas adicionales. Acepta
lectura 9000/6282 y valida tamaño/ASN.1; solo imprime tipos de directorio y
rutas del primer bloque. No DER bruto, binding PKCS#15, objetos, certificados,
referencias seguidas, 4141, PIN, MSE, firma ni Idopte. No prueba completitud
general del archivo. Más de 32 registros, alternativas inline/extendidas,
truncamiento o relleno no uniforme detienen sin seguir rutas. Permisos del
observador v5 y driver/submódulo intactos. Pasaron **40 comprobaciones del
helper y 33 del lanzador**; helper real compilado contra OpenSC offline,
transporte ficticio, sin hardware. Las 20 del observador previas siguen
vigentes, código sin cambios. Siguiente intervención, misma JCOP4/otros
clientes cerrados: `nix develop -c make inspect/jcop4-odf-map READER=0`.
Revisar si 4141 aparece como referencia tipada antes de decidir seleccionar/
leerlo. No pedir otro intento de PIN; VERIFY 6982 sigue pendiente.

Informe anterior antes de permitir la consulta de capacidades:
`local/jcop4-idopte-discovery-eIa1G5Gt/report.txt`, **15:35:59 UTC**, principal
`526338a`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario/ATR
y compilación correctos; contexto/conexión 0, las mismas ocho APDU 0/9000.
Primer control bloqueado identificado: **42000D48 / feature_request_empty**,
CM_IOCTL_GET_FEATURE_REQUEST (PC/SC Part 10), sin entrada. No llegó al lector;
no hay resultado ni contenido de sus capacidades todavía. Idopte muestra slot
vacío/sale 1, events_recorded; no rechazo APDU, PIN, VERIFY ni firma. Segunda
conexión 0, sin rehabilitar transporte. Observador **v5** preparado: permite
exclusivamente ese control sin entrada, normaliza NULL/0 al backend, limita
respuesta a mínimo de buffer del cliente/256 bytes y registra solo código/
resultado de transporte; nunca capacidades/TLV/longitudes/respuestas. La
lista anunciada no habilita controles adicionales: PIN-pad/escape/PACE y
todos los demás siguen bloqueados, también sin entrada. Requiere buffer/
contador de salida válidos, log privado, primer handle y presupuesto conjunto
de 128 APDU/controles; error/resultado superior al buffer detiene sin copiar
respuesta ni fallback. Una consulta exitosa conserva contexto de archivos.
Lanzador reconoce public_control_code como evento sin atribuirle una APDU.
Pasaron **20 comprobaciones del observador/constructor y 31 del lanzador**,
sin hardware; integración con redirector real/transporte ficticio permite
descubrir un control de PIN sintético pero bloquea su ejecución y VERIFY.
Biblioteca de diagnóstico compilada, driver/submódulo intactos. Próximo paso:
`nix develop -c make inspect/jcop4-idopte-discovery READER=0`, misma JCOP4,
firmador/otros clientes cerrados, sin PIN. Revisar control_transport y el
siguiente bloqueo antes de ampliar permisos. VERIFY 6982 sigue pendiente.

Informe anterior antes de identificar el control:
`local/jcop4-idopte-discovery-qKJ16Ogh/report.txt`, **15:31:33 UTC**, principal
`e240c20`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario/ATR
correctos, compilación del observador 0; contexto/conexión 0. Ocho APDU
permitidas dieron transporte 0/SW 9000: GET DATA nombre/versión, SELECT
inicial, dos SELECT EF con READ BINARY y otra consulta de versión. El formato
v3 no registra los FIDs de SELECT permitidos, así que no asignar ambas lecturas
a D003 ni dar por probado su contenido/ACL. **Primer bloqueo: reader_control**,
antes del transporte. El registro no identifica su dwControlCode ni forma;
no asumir consulta de capacidades ni PIN-pad. Segunda conexión 0 pero el
bloqueo persiste. Idopte muestra slot vacío y sale 1, events_recorded; compatible
con la interrupción del observador, no prueba de rechazo de tarjeta. No PIN,
VERIFY ni firma. Observador v4 preparado: añade exclusivamente identificador
hexadecimal del control bloqueado y clasificación feature_request_empty,
feature_request_unsupported o unknown. CM_IOCTL_GET_FEATURE_REQUEST procede
de reader.h (SCARD_CTL_CODE(3400)); únicamente se clasifica, **ningún control
se transmite**. No buffers de entrada/salida, tamaños, PIN ni respuestas.
Pasaron **17 comprobaciones del observador/constructor** con transporte
ficticio desde nix develop --offline; redirector real comprueba control de
capacidades vacío/con payload bloqueado sin invocar backend y sin fallback.
Lanzador y driver intactos; las 30 comprobaciones previas del lanzador siguen
vigentes. Siguiente intervención, misma JCOP4 y otros clientes cerrados:
`nix develop -c make inspect/jcop4-idopte-discovery READER=0`.
Identificar el control antes de ampliar permisos; VERIFY 6982 sigue pendiente.
No pedir nueva firma ni PIN todavía.

Informe anterior, fallo de entorno ya superado:
`local/jcop4-idopte-discovery-4riCxCot/report.txt`, **15:28:07 UTC**, principal
`e332630`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Inventario/ATR
correctos; **observer_build_status=1**, pkg-config no encontró libpcsclite.
No se cargó Idopte ni se creó public-headers.txt; no hubo nuevas APDU de
descubrimiento, lectura de D003, PIN ni autenticación. No es un rechazo de
tarjeta ni un nuevo bloqueo del observador. No se conoce la causa exacta de
la ausencia de la dependencia en la terminal usada; el flake ya la declara.
El constructor ahora comprueba pkg-config/libpcsclite antes de compilar,
muestra el comando con nix develop y conserva el fallo de consulta de flags
sin continuar por una sustitución de comando dentro de read. Pasaron **16
comprobaciones del observador/constructor** con transporte ficticio, incluidas
dos nuevas de dependencias ausentes/flags fallidos y compilador no invocado.
Lanzador y driver sin cambios; sus 30 comprobaciones previas siguen vigentes.
`nix develop --offline -c pkg-config --exists libpcsclite` terminó con 0
en este entorno; no usó hardware ni cargó Idopte.
Siguiente intervención, misma JCOP4 y otros clientes cerrados:
`nix develop -c make inspect/jcop4-idopte-discovery READER=0`.
No pedir nuevo PIN. Sigue pendiente comprobar SELECT/READ D003 en hardware
y resolver VERIFY 6982.

Informe anterior, último descubrimiento APDU observado:
`local/jcop4-idopte-discovery-FIyDDxFN/report.txt`, **15:07:25 UTC**, principal
`3423abc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Mismo contexto/
conexión 0, GET DATA nombre/versión y SELECT inicial 9000. **SELECT EF bloqueado
identificado como D003**, no transmitido, sin SW de tarjeta. Módulo 1, slot
vacío, events_recorded. No hubo PIN ni autenticación ni lectura de D003.
ANS CPS4 sección 3.1.2 y CIE 3.0 documentan D003 bajo MF como identificación/
serie legible sin autenticación; son perfiles distintos, no prueba del contenido
en CR. No universalizar el FID: OpenSC esteid2018 lo usa para número de documento.
Observador preparado para permitir **solo SELECT EF 00 A4 02 00 / D003 tras
confirmar MF**, y READ BINARY de ese archivo en canal básico, sin SFI ni datos
de entrada. Sus respuestas/seriales no se registran. MF se confirma únicamente
por selección básica de 3F00 o AID raíz conocido aceptada 9000/61xx. Una
selección absoluta de archivo bootstrap desde 3F00 también conserva contexto
MF; cambios de aplicación/canal, selecciones fallidas y reconexión lo retiran.
Forma de SELECT distinta, READ RECORD, otros canales y otros archivos siguen
bloqueados. No nuevos GET DATA/operaciones administrativas ni cambios en OpenSC.
Pasaron **14 comprobaciones del observador**; las **30 del lanzador** anteriores
siguen vigentes (lanzador sin cambios). Integración con redirector real y
transporte ficticio confirma MF/D003/READ seguidos de VERIFY bloqueado y omisión
del serial sintético. Pendiente una consulta pública con mismo Make, sin PIN.
Revisar los SW de D003 y siguiente bloqueo antes de ampliar; VERIFY 6982
sigue pendiente, no solicitar otra firma autenticada todavía.

Informe anterior antes de identificar el EF:
`local/jcop4-idopte-discovery-fZcrQuOX/report.txt`, **14:57:38 UTC**, principal
`7c046dc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. Contexto y
conexión PC/SC 0. GET DATA **00 CA 01 00** y **00 CA 01 16** -> transporte
0/SW 9000; SELECT **00 A4 00 00** -> 0/9000; siguiente SELECT **00 A4 02 00
bloqueado antes de transmitir**. Segunda conexión 0, pero transporte sigue
bloqueado. Módulo 1, slot vacío, events_recorded. No es un rechazo de tarjeta
al último SELECT: no llegó a ella. No hubo PIN ni autenticación. La captura
omite los datos, por lo que **no identifica el EF solicitado** ni el contenido
de las respuestas. No permitir SELECT/lectura de cualquier archivo por inercia.
Observador v3 preparado: añade exclusivamente blocked_select_ef=XXXX para
un SELECT EF 00 A4 02 00 válido con Lc 2 y total 7/8 bytes, sin transmitirlo;
formas distintas se marcan unsupported sin registrar payload/longitud. Es
un identificador de archivo, no contenido ni credencial. Se mantienen todos
los permisos de v2. Pasaron **12 comprobaciones del observador y 30 del
lanzador**, con transporte ficticio; OpenSC sin cambios. Siguiente intervención:
consulta pública realizada en FIyDDxFN: D003 identificado y estudiado.
No repetir ese paso como primer trabajo. No repetir firmas todavía.

Informe anterior con el primer GET DATA bloqueado:
`local/jcop4-idopte-discovery-aPCZL12P/report.txt`, **14:53:29 UTC**, principal
`7f31abc`, OpenSC `3dfef6704`, ambos Git limpios al comenzar. El observador
v2 funcionó: SCardEstablishContext y SCardConnect devuelven 0; primer intento
APDU **00 CA 01 00 bloqueado antes de transmitir**. Una segunda conexión
devuelve 0, pero transporte sigue bloqueado. Idopte muestra slot vacío y
sale 1; consecuencia compatible con el bloqueo, no rechazo de la tarjeta.
No hubo PIN ni autenticación. ANSSI-CC-2018/07, sección 1.2.4, documenta
00 CA 01 00 00 como GET DATA del nombre de la aplicación ChipDoc Lite.
Es precedente público para interpretar el comando, no prueba del contenido
de la respuesta en la JCOP4 CR. Se permite únicamente caso-2 corto, CLA 00,
P1 01/P2 00, sin datos de entrada; respuestas no se registran. Los demás
tags, casos con payload, otros canales, SM y autenticación siguen bloqueados.
Pasaron **30 pruebas del lanzador y 11 del observador**, incluida la nueva
consulta a través del redirector real con transporte ficticio. Pendiente una
consulta pública realizada y revisada en fZcrQuOX; no repetir esa versión
como primer paso. No hubo cambios del driver.

Informe anterior del observador v1:
`local/jcop4-idopte-discovery-y7dskS3z/report.txt`, **14:32:27 UTC**, principal
`fbf3434`, OpenSC `3dfef6704`. Compilación del observador 0, inventario/ATR
correctos; Idopte mostró slot vacío y salió 1. public-headers.txt contiene
solo format=public_discovery_headers_v1, **ningún evento**, ni comando bloqueado.
No demuestra un rechazo APDU de la tarjeta ni que Idopte no transmitiera.
No repetir la versión v1. La prueba local reprodujo un defecto de esa ruta:
RTLD_NEXT desde una precarga sin dependencia PC/SC no encuentra el transporte
de un plugin RTLD_LOCAL. Explica una limitación comprobada del observador,
pero **no demuestra la causa exacta de la ausencia total de eventos en hardware**.
Se corrigió el diagnóstico con LIBPCSCLITE_DELEGATE y una dependencia explícita
de libpcsclite_real.so.1; añade resultados de contexto/conexión/reconexión,
sin nombres/handles/ATR/payloads. Su funcionamiento ya se comprobó en el
informe aPCZL12P anterior. En su preparación pasaron **30 pruebas del
lanzador y 10 del observador**, incluidos redirector PC/SC real, plugin local,
scope global/local, transporte ficticio y rechazo de VERIFY/control/atributos.
No se cargó Idopte ni usó hardware por el agente.

Informe normal anterior ya revisado:
`local/jcop4-idopte-6aaJ5sIO/report.txt`, **14:17:31 UTC**, principal
`697eb0e`, OpenSC `3dfef6704`; ambos Git limpios al comenzar. Idopte 6.23,
Cryptoki 2.40 reconoce **NXP / ChipDoc v3.2**, slot 0, PIN 6/14. Flags:
login required, rng, token initialized, PIN initialized; no bloqueo, último
intento ni cambio pendiente anunciados. No prueba PIN correcto, contador
exacto ni firma; solo información pública, proceso 0. Mantener etiqueta,
serial y URI privados, no copiarlos a documentación/chat. No repetir esta
consulta normal como primer paso.

Informe anterior `local/jcop4-pin-policy-pMGzFYNL/report.txt`, **14:10:23 UTC**,
principal `292c6b4`: binding y
metadatos correctos. SELECT MF y aplicación correctos; ambos GET DATA
00 CB 3F FF / CHV 1 DOCP A0 -> transporte 0/**6D00**, sin datos. Esta
consulta IAS/ECC no está soportada en los contextos probados; no se obtuvo
política. Unlock y proceso 0 indican limpieza/consultas terminadas, no una
política interpretada. Sin PIN ni autenticación. No repetir ni barrer nuevas
referencias/plantillas por inercia. No aplicar política JCOP3 a JCOP4.

Informe anterior `local/jcop4-security-info-pveu5cDK/report.txt`,
**14:03:00 UTC**, principal `83b2eda`: binding y
metadatos correctos: clase AUTH tiene Card PIN y Card Admin Key (AuthKey,
derived=1); no objeto Card CAN en esta enumeración. No usar la llave
administrativa ni interpretar su ausencia de ruta como permiso para leerla.
GET DATA de versión obtuvo transporte 0/SW 9000, **cuatro bytes 03020052**.
Coinciden exactamente con ChipDoc 3.2.0.52 en identificación pública ANSSI,
enlazada en docs/jcop4.md; no acredita por sí solo personalización/certificación
CR ni variante exacta de hardware. SELECT MF correcto; READ B0 9C 00 / Le 256
de CardAccess por SFI 1C -> transporte 0/**6A82**, no disponible en esa
consulta/contexto (no demuestra ausencia general de PACE). Unlock y proceso 0.
No hubo PIN ni autenticación. No repetir esta consulta como primer paso.

El informe anterior 9FQ37Lk3 (14:00:01, b41f16b) falló por exigir seis bytes
exactos y no llegó a CardAccess. El helper corregido registra longitud/bytes
y acepta 0–6 sin imponer formato eOI; superior al buffer detiene. Corrección
validada en el nuevo informe. No repetir el intento autenticado.

Informe autenticado anterior `local/jcop4-signature-5Dpadr1N/report.txt`,
**13:44:00 UTC**, principal `b9c5696`: metadatos y
certificado RSA 2048 validados; VERIFY vacío 63C7 y MSE 9000. **El único
VERIFY con PIN transmitido devolvió 6982**, condición de seguridad no
satisfecha. No hubo PSO ni firma. Reset y unlock dieron 0. No es la respuesta
típica de PIN incorrecto (63Cx), pero tampoco demuestra PIN correcto ni
contador intacto: solo se consultó el contador antes del intento.
**Excluir `jcop4-signature-4FGWOtpI` del diagnóstico por instrucción expresa
del usuario (error humano); preservar el original sin utilizarlo como evidencia.**
No solicitar otro intento de PIN hasta resolver el requisito de autenticación.

Evidencia anterior ya revisada:
- `jcop4-pin-status-BYJtNyTV/report.txt`, 08:10:14 UTC, principal `765ef89`:
  VERIFY vacío 00 20 00 01 -> 63C7. PIN global 1, auth_method CHV=1,
  auth_type PIN=0, UTF8=2, flags 0x11, pad 00, límites 6–14, ruta vacía.
  Sin PIN; no repetir como primer paso.
- `jcop4-sign-setup-qob4FMSh/report.txt`, 08:03:22 UTC, principal `4611409`:
  **MSE 00 22 81 B6 / 91 02 28 91 -> transporte 0/SW 9000**. TokenInfo no
  anuncia algoritmos (count=0), PrKDF tampoco refs. No demuestra firma.
- `jcop4-pkcs15-msMLoEtW/report.txt`, 07:50:37 UTC: dos DER públicos y
  metadatos sin PIN. OpenSSL confirma RSA 2048/exponente 65537; IDs asociados
  a LlaveDeFirma 0x91, autenticación 0x90, ambas nativas. PIN auth ID Card PIN
  vinculado a llave de firma. Auth ID administrativo del objeto PIN no usado.
  DER/PEM/certificate-summary.json privados junto al informe; sin validar cadena.
- `jcop4-probe-9XynOJDU/report.txt`, 07:19:50 UTC: OMNIKEY 3x21/lector 0,
  ATR `3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a`; coincide con contacto
  eOI, difiere de JCOP3/iasecc. OpenPACE deshabilitado y eOI NULL.
- `jcop4-aids-qixa9ZwC/report.txt`, 07:28:12 UTC: cuatro candidatos 6A82.
- `jcop4-files-DORk47gE/report.txt`, 07:39:47 UTC: raíz A0000000003F00,
  pre-issuing 0400ECC2 sin interpretar, máximos APDU 0/0 desconocidos;
  aplicación E828BD080FA00000036364796E6964 / ChipDoc Generic.
La generación JCOP4 la indica el usuario, RSA 2048 sí se corroboró. No activar
excepciones JCOP3/eOI por ATR/etiqueta; precedente ChipDoc ANS enlazado en docs.

**Siguiente intervención:** confirmar si esta misma JCOP4 ya firmó con el
Idopte instalado, sin pedir otra prueba ni PIN. SELECT/FCP y descubrimiento v6
ya revisados; lectura/política de 4141 sin identificar. No repetir consultas
ni habilitar READ para superar el bloqueo por inercia.

**Descubrimiento Idopte conservado para investigación posterior:**
Inventario PC/SC/ATR conocido primero, después pkcs11-tool local con
--module /run/current-system/sw/lib/SCMiddleware/libidop11.so --show-info
--list-slots. Solo información módulo/slots/tokens: sin C_OpenSession,
C_Login, objetos, PIN ni firma solicitados por la herramienta. Compila solo
tools/pcsc-public-observer.c (biblioteca de diagnóstico, no driver) y delega
PC/SC de este proceso mediante LIBPCSCLITE_DELEGATE, sin LD_PRELOAD.
Requiere libpcsclite_real.so.1 de pkg-config; no sustituye archivos del sistema.
**Nunca usar con firmador/login/firma**. Registra nombres de tres funciones
PC/SC (establecer contexto, conectar, reconectar) y resultados, con máximo
128 registros de ciclo de vida. Además, cabeceras de cuatro bytes y transporte/SW,
sin buffers de datos, tamaños, Lc/Le, PIN, hash ni respuestas completas.
**Única excepción de datos de comando en formato
v3/v4/v5/v6:** identificador de dos bytes de un SELECT EF bloqueado, exclusivamente
CLA 00/INS A4/P1 02/P2 00, Lc 2 y tamaño total 7/8; otras formas solo
unsupported. No registra payloads arbitrarios ni cambia permisos por ese dato.
**v6 agrega solo etiquetas fijas** public_select para raíz/aplicación conocida
básica y SELECT 4141 de metadatos, sin registrar los valores de payload ni datos
arbitrarios. SELECT 4141 se permite solo bajo aplicación conocida ya seleccionada
con 9000, CLA 00/P1 02/P2 00/Lc 2/total 7 u 8; jamás habilita B0/B2 del archivo.
El formato v4 también registra dwControlCode en hexadecimal del primer
SCardControl bloqueado, identificador de operación del lector; clasifica
únicamente el código estándar CM_IOCTL_GET_FEATURE_REQUEST como empty o
unsupported según admita la forma sin entrada. Otros códigos son unknown.
No registra buffers, longitudes reales ni respuestas. En v5 se permite solo
CM_IOCTL_GET_FEATURE_REQUEST sin entrada, con respuesta interna acotada a
256 bytes/buffer del cliente, primer handle/log privado y presupuesto
conjunto de 128 operaciones APDU/controles. Registra public_control_code y
control_transport; no la lista devuelta ni su longitud. Esa lista no añade
permisos. Todos los demás controles, incluso con entrada vacía, se bloquean.
Bloquea antes del transporte todo
lo ajeno a la lista de descubrimiento: SELECT ISO limitado al MF/AID raíz/
aplicación observados, archivos bootstrap públicos y SELECT 4141 acotado
sin READ según v6; CA 01 16 de versión,
CA 01 00 de nombre de aplicación solo en canal básico y caso-2 corto,
READ BINARY/RECORD solo tras SELECT de 2F00/2F01/5031/5032 y GET RESPONSE
solo tras 61xx de una operación permitida. Se agrega D003 solo con SELECT
EF básico exacto desde MF confirmado; admite únicamente READ BINARY básico
sin SFI, no READ RECORD. MF/permiso se invalidan antes de cada selección y
se restablecen únicamente tras 9000/61xx con el contexto esperado.
Otros archivos, GET DATA, CLA
protegida/propietaria, VERIFY (incluso vacío), MSE, PSO, autenticación,
escrituras, SCardSetAttrib y controles distintos de la consulta de capacidades
vacía se bloquean. Tras primer bloqueo, error de transporte/69xx, otro handle
o más de 128 operaciones APDU/consulta de capacidades, no transmite más.
Log privado O_EXCL/NOFOLLOW/0600 requerido; sin log no transmite.
No usa flags JCOP3 ni helper autenticado. Stdin vacío y debug OpenSC 0.
Config y delegación aisladas: env externo OPENSC_DEEPBIND, LD_PRELOAD/trace y ambos
LIBPCSCLITE_DELEGATE/LIBPCSCLITE_SPY_DELEGATE se eliminan antes del inventario;
el delegado se activa solo en pkcs11-tool/Idopte. Conexión/reconexión descartan
permisos de lectura/respuesta anteriores; no rehabilitan transporte bloqueado.
Faltas de herramienta/módulo o error terminan sin fallback. Informe privado
local/jcop4-idopte-discovery-*/report.txt y public-headers.txt; el informe
contiene etiquetas/seriales identificables,
mantener fuera de Git y no volcarlo completo al chat. Idopte enumera todos
sus slots, que pueden ser varios por lector; no asumir que índice PC/SC
selecciona el slot PKCS#11. Se revisará reconocimiento del perfil y flags
de PIN sin tomar lo anunciado como prueba de autenticación/firma.
Un error de Idopte puede ser provocado por el bloqueo del observador: revisar
primera cabecera bloqueada antes de concluir fallo del módulo o tarjeta. No
reintentar login ni ampliar permisos del observador sin interpretar ese comando.
Si solo hay ciclo de vida, informa pcsc_lifecycle_only y falla conservando sus
resultados; si no hay eventos, no_pcsc_event_observed. Ninguno acredita
observación APDU ni éxito. Cabeceras y estado del módulo se conservan siempre.

Se pidió al usuario confirmar si esta JCOP4 ya había firmado con Idopte,
sin solicitar una nueva prueba de PIN. Confirmación pendiente en esta revisión.

El código local eoi_pin_cmd y el driver ChipDoc italiano abren PACE/CAN
antes de VERIFY; eOI lo hace sin exigir interfaz contactless en esa condición.
Es una hipótesis para 6982, **no prueba del requisito en Costa Rica**. No
activar eOI por ATR: AID/perfil/RSA difieren. También sigue sin resolverse
si el orden MSE antes de VERIFY influye. La consulta nueva obtiene evidencia
pública sin cambiar el protocolo autenticado ni el submódulo.
La revisión estática del módulo instalado muestra funciones de canal seguro
y restricciones de PIN; los nombres no demuestran qué usa esta tarjeta.
La documentación pública NXP de ChipDoc 3.2 contempla PACE con PIN/CAN/MRZ
y canales de confianza, sin resolver aún el requisito de la personalización CR.
No copiar implementación privativa ni intentar autenticación administrativa.
Las **35 comprobaciones del helper** pasaron en la preparación previa.
Ahora pasaron **30 del lanzador** y **14 del observador**, incluyendo
bloqueo anterior a transporte, privacidad, reproducción de RTLD_NEXT local y
delegación PC/SC real contra transporte falso. make test/pcsc-public-observer
y test/iasecc incluyen este último conjunto.
Biblioteca real compilada; no se cargó Idopte ni usó tarjeta por el agente.
Sin cambios ni recompilación del driver OpenSC; delegación y primer bloqueo
observados en hardware. CA 01 00/01 16 y SELECT A4 00 00 ya dieron 9000;
el siguiente SELECT A4 02 00 es D003 y quedó bloqueado. Su lectura pública
acotada está preparada, pendiente comprobar los SW en hardware.
Reconocimiento normal Idopte ya comprobado.

Herramienta autenticada anterior, conservada para investigación futura:

El lanzador compila solo tools/card-sign-test.c con util_getpass de OpenSC,
crea local/jcop4-signature-*/ (0700/0600), muestra message.txt técnico y calcula
SHA256 en digest.bin. Helper exige stdin/stdout TTY, comprueba ATR conectado,
AID anunciado y binding nativo sin cachés. Exige LlaveDeFirma RSA 2048/0x91,
nativa/uso SIGN/ruta vacía, único PIN vinculado con parámetros observados y
certificado público del mismo ID/RSA 256 bytes. Estas restricciones delimitan
el diagnóstico actual, no arquitectura final del driver. Lee DER antes de
PIN. En una sola transacción: selecciona aplicación, VERIFY vacío de estado,
MSE 91022891, **un VERIFY con PIN**, **un PSO 00 2A 9E 9A con digest de 32
bytes y Le 256**. No SELECT/lecturas de metadatos después del PIN. Sin
reintentos de VERIFY/PSO por 6C; GET RESPONSE permitido solo para respuesta
normal de PSO. Sin CAN/PACE, componentes privados ni comandos administrativos.

Tras intento autenticado llama sc_reset(card,0) para terminar autenticación,
incluso ante fallo; registra resultado y libera transacción. Borra memoria
del PIN inmediatamente tras transmisión y en rechazos locales, sin guardar
valor, longitud real o hash. Debug 0 y trazas/env JCOP3 desactivadas. Reporte
solo etapas, resultados/SW y tamaño público de firma. Un fallo detiene sin
reintentos automáticos ni variantes de protocolo. Solo acepta PSO 9000 y
256 bytes; OpenSSL verifica message.txt con el certificado de esa sesión.
Ante fallo, revisar informe antes de cualquier nuevo PIN; no pedir repetir
por inercia. No afirmación de firma exitosa hasta ver report y Verified OK.

Verificados sin hardware: helper real compiló contra OpenSC offline; **7
comprobaciones de operación/metadatos** con credenciales sintéticas y **4
regresiones del lanzador con PTY y helper/OpenSSL simulados**. Log C
build/opensc-offline/jcop4-sign-operation-tests.log. Incluidos en test/iasecc;
util-linux agregado al flake para script (PTY), sin cambiar lock. No se
repitió la suite JCOP3 ni modificó submódulo. Cambios solo en principal;
dar comandos de commit al usuario. Hubo un VERIFY JCOP4 rechazado; aún no
hay autenticación exitosa ni firma. Modularidad y transporte RSA 3072 (384 bytes) siguen
pendientes, no anunciar soporte completo.

Estado actualizado tras revisar capturas del **13 de octubre de 2026,
05:13–05:18 UTC** (12 de octubre, 23:13–23:18 de Costa Rica), OpenSC
`3dfef6704`. **Reinserción con lock_login validada entre dos firmas en una
misma GUI; ambos PDF verificados.** Principal `d26d23e` y submódulo limpios
al comenzar. Revisión documental, sin cambios de driver ni nuevas firmas
por el agente. No hace falta recompilar ni repetir pruebas como primer paso.

Última GUI lqSwrf8P: 404 eventos; VERIFY/login 117/119 OK, RSA/SignFinal
205/207 OK; presencia 208=7 (PRESENT|CHANGED|INUSE), 213=3 (PRESENT|CHANGED),
limpieza de token/sesiones. Ocho SLOT_TOKEN_REMOVED son dos rondas de cuatro
slots, no ocho retiradas. No hay muestra de ausencia. Después nueva reserva
CARD_LOCK 307, PRE_VERIFY_SELECT 312, VERIFY/login 314/316 OK, RSA/SignFinal
402/404 OK. No hereda autenticación previa ni revalida PIN automáticamente.
Dos PDF: local/jcop3-lock-hotplug-before.pdf 80482 bytes, verificación/copias
sdz525u1; after.pdf 80462, pnc0zlig, bajo local/jcop3-pdf-verification-.
Ambos CAdES con certificado exacto LlaveDeFirma y sello CMS/imprint válidos;
original/texto/render preservados. No confianza/revocación/LTV. Los eventos
no tienen tiempos individuales ni nombres de documento.

**Error inicial sin causa confirmada:** GUI M6TUGgHo 382 eventos, primera firma
OK, cambio de tarjeta y luego dos PIN_APDU_BUILD -1300/C_Login 0x7.
PRE_VERIFY_SELECT pasa; no PIN_TRANSMIT en esos rechazos. Fallan antes de
transmitir el PIN; no son respuestas de PIN incorrecto ni RSA 6982. Usuario
cree que hubo error humano; no darlo por confirmado. No registrar secreto,
longitud o hash ni alterar validación. Causa del rechazo local sigue pendiente.
Otras GUI: RjjRHcGv 207/1 firma, jVrFj373 343/2, 4CWjxEB0 207/1; todas
VERIFY/login/RSA OK, sin cambio de tarjeta registrado en esas capturas.
Se preservaron api-reviewed.log y analysis.json privados de las cinco GUI.

Perfil make firmador/jcop3-lock-login sigue optativo: lock_login=yes,
atomic=no, candidata y cachés desactivadas, debug 0. Pruebas anteriores
confirman que otro cliente espera hasta cerrar GUI y luego obtiene SELECT
9000, con seis firmas exitosas en API y un PDF verificado (37si22cp).
SW_PIN_LOGOUT_ONLY=1 omite unlock en logout; login fallido puede conservar
reserva. Cerrar GUI al terminar/ante error. Limita compartición PKCS#11 y no
prueba concurrencia independiente. Sin reserva, helper tras PIN provocó RSA
6982 aunque login fue aceptado; intención/regla interna de tarjeta desconocida.

Cambios actuales solo en principal: consulta pública JCOP4, regresiones y documentación.
Suite anterior 288 comprobaciones; seis del helper pasan en la revisión
previa. No repetir suite por documentación. Dar comandos de commit del
principal; el agente no escribe Git ni firma/lee pass. Mantener datos privados
fuera de Git. No modificar el perfil normal por defecto.

**Próximo trabajo:** revisar primeras cabeceras de descubrimiento Idopte y determinar
qué condición falta para VERIFY 6982 antes de solicitar otro PIN.
Para integrar JCOP3, revisar/separar correcciones del perfil frente
a instrumentación y experimentos antes de integrar. Diferencia local contra
base original: 21 commits, 20 archivos, +736/-77 líneas; no equivale a revisión
contra master actual. Antes de preparar reporte/PR upstream se debe reproducir
contra master actual conforme al AGENTS.md del submódulo, preservando rama y
capturas. No se ha publicado fork/issue/PR. Solo pedir intervención si hace
falta nuevo acceso de hardware/Git/red; no reclamar soporte completo.
Pendientes: rechazo local intermitente, binding/perfil por defecto, PuKDF,
compartición entre procesos, otras tarjetas/mecanismos/plataformas.

## Objetivo y alcance

Desarrollar soporte libre, sobre OpenSC, para una tarjeta **JCOP3 de firma
digital de Costa Rica**, mediante su interfaz de contacto. El usuario tiene
la tarjeta y un lector USB. El primer hito es obtener una firma RSA SHA-256
con `LlaveDeFirma` y verificarla con OpenSSL. **Ese primer hito se completó**
con RSA-PKCS y DigestInfo SHA-256 el 11 de octubre a las 20:18 de Costa Rica.
Firmador ya produjo PDF verificados tanto con el experimento de omisión
como con la candidata que mantiene consultas físicas sin reselección.
Ahora se validarán los límites de esta corrección antes de preparar cambios
para OpenSC.

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
interna y de sus comandos. No asumir soporte de todas las JCOP3. JCOP4 se
investiga ahora como etapa separada; contactless, CAN/PACE y criptografía
poscuántica quedan fuera de las pruebas actuales.

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
| `scripts/probe-jcop4.sh` | Inventario, SELECT, archivos y PKCS#15 público con driver default |
| `scripts/build-card-public-info.sh`, `tools/card-public-info.c` | EF.ATR/EF.DIR y binding PKCS#15 nativo; certificados públicos/metadatos, sin autenticación |
| `scripts/sign-jcop3.sh` | Firma interactiva de texto y verificación OpenSSL |
| `scripts/sha256-digestinfo.sh` | Prepara DigestInfo SHA-256 DER para `RSA-PKCS`, sin PIN |
| `scripts/firmador-jcop3.sh` | Abre firmador con módulo local y configuración PKCS#15, sin depuración APDU |
| `scripts/peer-context-jcop3.sh` | Segundo cliente: solo SELECT raíz IAS, sin PIN/firma y debug 0; comprueba SW 9000 |
| `scripts/test-iasecc.sh`, `tests/` | Regresiones sin tarjeta |
| `docs/jcop3.md` | Evidencia técnica y evolución de la investigación |
| `docs/jcop4.md` | Alcance y pasos de investigación de la nueva tarjeta |
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
`inspect/jcop3-pkcs11`, los diagnósticos de política, `sign/jcop3`,
`sign/jcop3-sha256`, `sign/jcop3-pin-status`, `sign/jcop3-select-root`,
`firmador/jcop3`, `firmador/jcop3-diagnose`, `firmador/jcop3-session-state` y
`firmador/jcop3-pin-status-in-place`, `firmador/jcop3-lock-login` y
`firmador/jcop3-cli` utilizan
la compilación existente: **no recompilan**.

Las restricciones del entorno del agente pueden impedir usar el daemon de
Nix o acceder a red. Una compilación sin PC/SC sirve para comprobar código,
pero no valida comunicación con la tarjeta.

## Git al momento de esta documentación

Los últimos commits guardados son:

- Repositorio principal: **`775ad54`**, `Inspect public card application metadata`.
- Submódulo OpenSC: **`3dfef6704`**, `Select ChipDoc before VERIFY`.
- Base upstream original: `c7a377af971df03f879c1697e91ebac1f7e28c56`.

Ambos árboles estaban limpios al comenzar esta revisión. Principal agrega
el binding nativo público JCOP4 y documenta su alcance/modularidad requerida. El
submódulo queda intacto; no recompilar por este cambio. Dar comandos de commit
del principal. No hay commits, logins o firmas ejecutados por el agente.

La URL del submódulo aún es `https://github.com/OpenSC/OpenSC.git`. Los commits
locales del driver necesitarán publicarse en un fork y actualizar esa URL
para que otro clon pueda obtenerlos. No se ha enviado un issue ni un PR.
Al continuar, revisar ambos `git status` y conservar cualquier trabajo nuevo;
no restablecer ni actualizar automáticamente el submódulo. Dar al usuario
los comandos de commit; el agente no escribe en `.git`.

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
La prueba de las 19:41 falla con `SHA256-RSA-PKCS` después de pasar el login.
El helper ahora usa `RSA-PKCS` con DigestInfo SHA-256 y esa ruta se verificó
en hardware. El mecanismo combinado también produjo una firma verificada
después de la corrección de negociación (`34a040c32`). No se implementó
QSign con hashing en hardware.

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
5. **Todos los grupos de ACL de SDO** (`0c3e1c4f5`):
   `iasecc_parse_acls()` también perdía los grupos posteriores al primero.
   Ahora conserva todas las condiciones de los DOCP RSA capturados, acepta
   repeticiones idénticas y rechaza alternativas distintas y truncamientos.
   Las condiciones de firma, autenticación interna y descifrado apuntan al
   entorno de seguridad 3. No se ejecutaron operaciones administrativas.
6. **Ruta de la llave de firma** (`fb767b3dd`):
   `_iasecc_fixup_signing_key_paths()` conserva el AID ChipDoc y la referencia
   nativa RSA 3, eliminando la ruta de archivo `0003`, cuya selección devuelve
   `6A82`. Se limita a Amos + Generic PKI + RSA nativa de 2048 bits y ese AID
   en la ruta. La llave 2 y otros perfiles quedan intactos. La corrección se
   aplica al cargar PrKDF y a los objetos ya cargados en la inicialización.
7. **Referencias medidas y DOCP público** (`70d7afc20`):
   `card-iasecc.c` resuelve HT SHA-256 `40`, DST RSA/SHA-256 `42` y AT RSA_PKCS
   `02` cuando no hay tabla declarada. Se limita a Amos, DF seleccionado con
   el AID Generic PKI, RSA 2048 y referencia de llave 3. Tablas declaradas y
   otros perfiles no reciben fallback. La preparación de esa llave lee solo
   DOCP, comprueba tamaño 256 bytes y libera los metadatos; no solicita `7F48`.
   La operación AT RSA-PKCS produjo firmas SHA-256 verificadas tanto con
   DigestInfo explícito como mediante el mecanismo combinado.

La condición `00` de VERIFY permite ejecutar el comando sin autenticación
previa; **no acredita el PIN ni evita su comprobación por la tarjeta**.
El significado de `C9` y las condiciones administrativas siguen pendientes.

La corrección de ruta depende de la emulación PKCS#15. Los scripts generan
configuración local con **`try_emulation_first = yes`** y
**`use_file_caching = no`**. El binding nativo por defecto puede omitir esa
corrección: al integrar con firmador habrá que conservar esta configuración
o resolver la selección del perfil. No basta con cambiar la ruta de la .so.
El nuevo objetivo `make firmador/jcop3` proporciona esa configuración local.

8. **Hash SHA-256 en software para ChipDoc** (`34a040c32`):
   `_iasecc_fixup_signature_algorithms()` en `pkcs15-iasecc.c` retira el flag
   SHA-256 de la capacidad RSA 2048 cuando coincide el perfil y la llave
   observados, no hay algoritmos declarados y se dispone de PKCS#1/HASH_NONE
   sin RSA RAW. Así PKCS#15 prepara DigestInfo y usa AT RSA-PKCS sobre el
   digest que PKCS#11 ya calculó. Preserva SHA-1, otros tamaños/perfiles y
   algoritmos declarados. La capacidad RSA 2048 es compartida por las llaves;
   este ajuste no valida la llave de autenticación. No modifica PIN ni ACL.

## Verificación y últimas capturas

La validación previa de JCOP3 tiene **288 comprobaciones de regresión** en dieciséis fixtures C: `iasecc-fci.c`,
`iasecc-pin-path.c`, `iasecc-key-path.c`, `iasecc-sdo.c`, `iasecc-pin-policy.c`
y `iasecc-sign-setup.c`, más `iasecc-rsa-pkcs.c`, `pkcs11-api-trace.c`,
`pkcs11-session-state.c`, `iasecc-context-trace.c`,
`iasecc-post-login-status.c`, `pkcs15-pin-status-in-place.c`,
`pkcs11-card-change.c`, `iso7816-pin-trace.c`, `iasecc-pin-application.c`, `pkcs11-login-config.c`, y los scripts
`sha256-digestinfo.sh`, `firmador-launcher.sh`, `peer-context-launcher.sh` y `pkcs11-sign-fallback.sh`, este último con un módulo C
PKCS#11 completamente simulado. Incluyen respuestas
capturadas, conservación/copia de metadatos, rechazo de perfiles distintos y
transporte simulado que verifica que cambio/desbloqueo no autentican.
Pasaron en la comprobación local; también terminó la compilación completa
sin PC/SC, registrada en `build/opensc-offline/pin-application-build.log`.
Las últimas regresiones están en `build/opensc-offline/login-lock-tests.log`.
El checker de configuración se ejecuta desde los casos del lanzador.
Esto no sustituye pruebas de firma en hardware.
Se agregaron trece comprobaciones de `tests/jcop4-probe-launcher.sh`, que
pasaron por separado sin lector: comandos limitados a inventario/SELECT,
aislamiento de configuración/stdin, validación de ATR/SW, errores sin
reintentos, argumentos y compilación local.
Hay además quince comprobaciones de `tests/card-public-info.c`, con API
simuladas, que verifican perfil default, ATR, secuencia, rechazo sin login,
errores y liberación de transacción, binding nativo sin emuladores, certificados
privados omitidos y metadatos RSA 3072/referencia distinta sin prueba de RSA.
El runner `scripts/test-iasecc.sh`
incluye ambas; no se repitieron las 288
anteriores al agregar esta consulta pública.
El lanzador de firmador, en modos normal, diagnóstico y experimento de sesión,
pasó sintaxis y una ejecución con
`firmador` simulado: selección del módulo, sustitución de configuración/debug
heredados, perfil idéntico al de la firma, permisos 0600 y propagación del
código de salida. Los modos normal y diagnóstico eliminan
`OPENSC_JCOP3_SESSION_STATE` heredado; el normal también elimina `OPENSC_API_TRACE_FILE`;
el diagnóstico crea directorio 0700 y archivos 0600. Esa comprobación no
abrió Java ni accedió al lector.

| Captura local | Resultado relevante |
| --- | --- |
| `jcop3-inspection-WAZLkGL7.txt` | Certificados legibles tras corregir FCP |
| `jcop3-signature-S8Pd9rPG/report.txt` | Intento anterior que llegó a `C_Login`: falla con `CKR_GENERAL_ERROR`; no hay firma |
| `jcop3-pin-context-6Hd0jGpP.txt` | CHV 1 accesible en raíz; `0001` no seleccionable |
| `jcop3-pkcs11-gYBEWLz8.txt` | SDO ya conserva CHV 1, pero parser antiguo rechaza `C9` |
| `jcop3-pin-policy-9U5otg3U.txt` | Selecciones anidadas devuelven DOCP entero; controles idénticos, 7/7 intentos |
| **`jcop3-pkcs11-cRVr2acc.txt`** | Última consulta, 21:34:21 UTC, OpenSC `840a7b204`: política del PIN leída, límites 14/4 e intentos 7/7 |
| **`jcop3-key-policy-egE7wr1j.txt`** | Última consulta, 21:34:35 UTC: SELECT ChipDoc y GET DOCP de RSA_PRIVATE 2 y 3 devuelven `9000` |
| **`jcop3-security-env-MMjEmIcC.txt`** | 12 de octubre, 00:31:57 UTC (11 de octubre, 18:31 en Costa Rica): PC/SC no encuentra lectores; ninguna consulta SE/ruta se ejecutó |
| **`jcop3-security-env-yw7ohMX7.txt`**, **`jcop3-security-env-OQCXHYVH.txt`** | 12 de octubre, 00:49:16 y 00:49:19 UTC: seis respuestas idénticas; SE 3 legible en ambos AID, AT USER PASSWORD referencia 1; SELECT `0003` devuelve `6A82` |
| **`jcop3-pkcs11-OwOOGiF2.txt`** | 12 de octubre, 01:16:15 UTC: corrección de ruta de firma aplicada por PKCS#15 y PKCS#11; límites PIN 14/4, intentos 7/7 |
| **`jcop3-algorithms-FinI8oaD.txt`** | 12 de octubre, 01:16:28 UTC: nueve respuestas `9000`; HT `40`, DST `42` y AT `02` aceptados; DOCP CHV 1 antes/después idénticos, 7/7 intentos |
| **`jcop3-signature-FktWTv57/report.txt`**, **`jcop3-signature-jQYkimJu/report.txt`**, **`jcop3-signature-54OpXE2f/report.txt`** | 12 de octubre, 01:28:26, 01:28:46 y 01:28:53 UTC, OpenSC `70d7afc20`: `No slots.` en lectura del certificado; no se ejecutó el comando autenticado |
| **`jcop3-pkcs11-XtNrgvHO.txt`** | 12 de octubre, 01:39:13 UTC, OpenSC `70d7afc20`: lector y ATR visibles, slot 0 presente, dos certificados y dos llaves públicas RSA; correcciones de rutas aplicadas, intentos PIN 7/7 |
| **`jcop3-signature-CXq9hBMQ/report.txt`** | 12 de octubre, 01:41:29 UTC: certificado leído y login aceptado; `C_SignFinal` devuelve `CKR_FUNCTION_NOT_SUPPORTED`; no hay firma ni verificación OpenSSL |
| **`jcop3-signature-Pwfndadv/report.txt`** | 12 de octubre, 01:59:58 UTC: helper `RSA-PKCS`, certificado y DigestInfo preparados; `C_Login` devuelve `CKR_PIN_LEN_RANGE`; no se llegó a la firma |
| **`jcop3-pkcs11-EqveuSwF.txt`** | 12 de octubre, 02:12:28 UTC: mismo módulo y ATR, slot 0 y objetos RSA visibles; AODF 4–20, CHV 4–14, intentos 7/7; no hubo login |
| **`jcop3-signature-crUXrnAi/report.txt`** | 12 de octubre, 02:18:39 UTC: login y RSA-PKCS exitosos, firma de 256 bytes, OpenSSL `Verified OK`, estado 0; primera firma verificada |
| **`jcop3-signature-kiHbc1Qh/report.txt`** | 12 de octubre, 02:37:33 UTC, OpenSC `34a040c32`: login y SHA256-RSA-PKCS exitosos, texto original como entrada, firma de 256 bytes, OpenSSL `Verified OK`, estado 0 |
| **`jcop3-firmador-api-1LBmDPbP/api.log`** | 12 de octubre, 03:06:20 UTC, OpenSC `f7752be36`: un login exitoso, PIN_STATUS logged in, sesiones RW USER, SHA256-RSA-PKCS y llave 3; `C_SignFinal` devuelve `CKR_USER_NOT_LOGGED_IN`; no hay logout/cierre intermedio |
| **`jcop3-firmador-api-hBIzcp79/api.log`** | 12 de octubre, 03:24:32 UTC, OpenSC `a19ee5927`: un login exitoso; llave 3; selección, DOCP y MSE pasan; RSA AT devuelve `6982`, traducido a `SC_ERROR_SECURITY_STATUS_NOT_SATISFIED (-1211)` y `CKR_USER_NOT_LOGGED_IN`; no reintento RSA |
| **`jcop3-firmador-api-HQ6utMiv/api.log`** | 12 de octubre, 03:39:50 UTC, OpenSC `ca8f65684`: experimento activo; `C_GetSessionInfo` omite consultas físicas; login, selección y MSE pasan; RSA AT sigue devolviendo `6982`; un login y una firma fallida |
| **`jcop3-firmador-api-D83Fsxrk/api.log`** | 12 de octubre, 03:54:31 UTC, OpenSC `a3b001ea2`: un login exitoso; una consulta token y cuatro consultas sesión después del login; 15 SELECT raíz y un SELECT ChipDoc; sin reset detectado; RSA AT devuelve `6982` |
| **`jcop3-firmador-api-WV8KnIjI/api.log`** | 12 de octubre, 05:34:00 UTC, OpenSC `da1edb457`: experimento sesión+token activo; login OK, llave 3, SHA256-RSA-PKCS; ninguna selección raíz después del login; RSA AT `9000`, retorno 256 y `C_SignFinal CKR_OK`; firma PDF y sello verificados posteriormente con OpenSSL |
| **`jcop3-signature-rwfMn3zh/report.txt`**, **`jcop3-signature-ELmY1jCa/report.txt`** | 12 de octubre, 06:02:54 y 06:03:14 UTC, OpenSC `0e7549960`: lectura del certificado detenida en `C_OpenSession CKR_TOKEN_NOT_PRESENT`, slot 0; no login ni consulta experimental |
| **`jcop3-signature-QfdEBgds/report.txt`**, **`api.log`** | 12 de octubre, 06:04:31 UTC: consulta vacía inmediatamente después del VERIFY con retorno 0 y estado LOGGED_IN; login OK; llave 3; RSA AT `9000`, 256 bytes; OpenSSL `Verified OK`, estado 0 |
| **`jcop3-signature-zPoqBbBZ/api.log`**, **`jcop3-signature-rb2JHk1X/api.log`** | 12 de octubre, 06:13:20 y 06:13:35 UTC, OpenSC `416c11c7b`: un SELECT raíz aislado `9000` después de VERIFY; login OK; cada ejecución tiene dos rechazos RSA `6982` debido al fallback multipart del CLI; no firma ni segundo login |
| **`jcop3-firmador-api-MWUUuIrV/api.log`** | 12 de octubre, 06:37:01 UTC, OpenSC `23b3d25fa`: candidata sin selección, login OK, cuatro consultas físicas autenticadas, cero SELECT raíz después del login, RSA AT `9000`, 256 bytes y C_SignFinal OK; nuevo PDF y sello verificados |
| **`jcop3-firmador-api-UpCXoCn0/api.log`** | 12 de octubre, 06:47:51 UTC, OpenSC `23b3d25fa`: un login y una firma RSA exitosa, cuatro consultas físicas autenticadas |
| **`jcop3-firmador-api-LB7oJvqS/api.log`** | 12 de octubre, 06:48:43 UTC: primera firma exitosa, logout software, CHV sigue autenticado, un SELECT raíz antes de segundo login exitoso; segunda RSA `6982`; primer PDF consecutivo y sello verificados |
| **`jcop3-firmador-api-fgRfkdCF/api.log`** | 12 de octubre, 07:07:46 UTC, OpenSC `381c469b2`: dos logins con VERIFY real, logout intermedio, consultas físicas autenticadas sin reselección, ambas RSA `9000` de 256 bytes; los dos PDF y sellos se verificaron |
| **`jcop3-firmador-api-9ujB6ts6/api.log`** | 12 de octubre, 07:15:42 UTC: primera firma OK y PDF verificado; tras retirada/reinserción cinco bloqueos -1105, segundo login CKR_DEVICE_REMOVED antes de VERIFY y sesión final inválida; Java NullPointerException Token.secretCache null |
| **`jcop3-firmador-api-oJtaNn1s/api.log`** | 12 de octubre, 07:39:06 UTC: 430 eventos; ausencia y limpieza de token observadas, nueva presencia, dos VERIFY reales y dos RSA de 256 bytes; ambos PDF y sellos verificados. Base 381c469b2 con trazas modificadas, luego comprometidas como 1736ce3a9 |
| **`jcop3-firmador-api-y17eLwTD/api.log`** | 12 de octubre, 07:46:21 UTC, 1736ce3a9 limpio: 260 eventos, primera firma verificada, diez CARD_PRESENCE=3 sin limpieza del token, segundo login 0x32 antes de VERIFY y sesión 0xB3; regresión reproducida y corrección local preparada |
| **`jcop3-firmador-api-und1k27k/api-reviewed.log`** | 12 de octubre, 08:02:29 UTC, 8842f394b limpio: 463 eventos revisados; dos CARD_CHANGED procesados y limpieza del token, dos VERIFY reales y dos RSA de 256 bytes; reinserción rápida y ambos PDF/sellos verificados |
| **`jcop3-firmador-api-Z0aJ18Ge/api-reviewed.log`** | 12 de octubre, 08:11:29 UTC, 8842f394b limpio: primer CLI, 174 eventos; VERIFY/login/RSA OK, llave 3 de 2048 bits, firma de 256 bytes y PDF/sello verificados |
| **`jcop3-firmador-api-tYcckn70/api-reviewed.log`** | 12 de octubre, 08:18:56 UTC, 8842f394b limpio: 1170 eventos, seis VERIFY y RSA exitosas, cuatro cambios con limpieza; cinco PDF previstos y un extra, todos con firma/sello verificados |
| **`jcop3-firmador-api-GbaOMELO/api-reviewed.log`**, **`jcop3-firmador-api-j0FPyGAS/api-reviewed.log`** | 08:35/08:36 UTC, 8842f394b limpio: 241/145 eventos, cuatro/dos logins CKR_ARGUMENTS_BAD; VERIFY -1300, sin firma ni fallos de lock; causa pendiente de marcas ISO |
| **`jcop3-firmador-api-PkFq1kEy/api-reviewed.log`** | 08:51:29 UTC, c052ee925 limpio: 147 eventos, dos builds -1300, ningún PIN_TRANSMIT o firma; rechazo local confirmado, comparación CLI ya pasó en DgJlCm1N |
| **`jcop3-firmador-api-DgJlCm1N/api-reviewed.log`** | 08:56:49 UTC, c052ee925 limpio: 176 eventos, build/VERIFY/login/firma exitosos vía CLI; PDF/sello verificados en 0xwdzxbh; rechazo GUI aún sin causa establecida |
| **`jcop3-firmador-api-Ie57KYQq/api-reviewed.log`** | 09:00:25 UTC, c052ee925 limpio: 210 eventos preservados, build/VERIFY/login/firma GUI exitosos; doc-firmado.pdf verificado/copias en bkt8tbsu; causa del rechazo previo desconocida |

Los informes PKCS#11 y DOCP RSA de las 21:34 UTC ya se revisaron: **no pedir ejecutarlos de nuevo como
primer paso**. Los DOCP RSA se reprodujeron en las regresiones del parser
corregido; ambos declaran tamaño 256 bytes y ACL idénticas. La condición de
firma `13` significa autenticación de usuario mediante SE 3 según OpenSC;
las nuevas capturas confirman que su AT refiere al CHV 1. Las consultas solicitaron solo
DOCP `A0`, nunca la plantilla `7F48` ni componentes privados.

TokenInfo no incluye `supportedAlgorithms`, y las llaves muestran
`Algo_refs: 0`. Se preparó el fallback limitado a las tres referencias MSE
aceptadas. La corrección de ruta ya aparece en la enumeración de la tarjeta.
Hubo tres ejecuciones del helper a las 19:28, pero todas se
detuvieron en la enumeración de slots previa a leer el certificado. Ninguna
ejecutó `--login --sign` ni verificó un PIN. Los informes no distinguen entre
lector no visible, servicio PC/SC inaccesible o configuración de slots.
El diagnóstico posterior `jcop3-pkcs11-XtNrgvHO.txt` sí detecta lector,
tarjeta y slot 0 con el mismo módulo y proveedor PC/SC. No se reprodujo
`No slots.` y su causa sigue sin confirmarse. Persiste el aviso conocido
de PuKDF, pero no impide esa enumeración. No hubo autenticación; el PIN
conserva 7/7 intentos en ese diagnóstico previo al último login.
La ejecución de las 19:41 sí pasó el login y falló en la firma. No hay captura
APDU autenticada que confirme el comando exacto o el punto físico del fallo.
La regresión reproduce `SC_ERROR_NOT_SUPPORTED` en QSign SHA-256 con OpenSSL 3
después de preparar HT/DST. El helper evita ese camino con `RSA-PKCS` y un
DigestInfo de 51 bytes; la tarjeta debe hacer el padding PKCS#1 v1.5 y RSA
mediante la operación AT ya presente en el driver. No se añadió una corrección
de QSign al submódulo. La cadena combinada requiere revisar también cómo
PKCS#11 calcula el hash al faltar referencias declaradas de algoritmos.
Aceptar MSE no demuestra una operación RSA correcta.

El usuario confirmó que pasan las pruebas con el helper actualizado. El nuevo
intento de las 19:59 se detuvo en `C_Login` con `CKR_PIN_LEN_RANGE (0xa2)`.
No demuestra un fallo de la ruta RSA-PKCS ni un PIN incorrecto. En el camino
de código revisado, este error de longitud se produce antes del VERIFY con
el PIN. `_validate_pin()` valida primero el rango PKCS#15 declarado en AODF
(4–20), distinto del límite público CHV (4–14) que aplica después el driver.
No alterar ninguno de esos límites para eludir el error.
El usuario no puede confirmar si introdujo el mismo PIN que en el login
exitoso anterior; cree que sí. La lectura de `util_getpass()` y el paso de
su resultado a `C_Login` no muestran un cambio entre ambos intentos.
La consulta posterior `jcop3-pkcs11-EqveuSwF.txt` confirma que los límites
permanecen iguales y los intentos siguen en 7/7. Lector, ATR, slot 0 y objetos
RSA son visibles; persiste PuKDF `-1402`, sin bloquear la enumeración.
No hubo login en esa consulta ni hay captura autenticada. No se confirmó
la causa de la entrada rechazada y no se cambiaron límites ni código.
El intento posterior de las 20:18 pasó el login y produjo la primera firma
RSA SHA-256 verificada. Se compararon la llave pública extraída del certificado
y el DigestInfo recalculado con los archivos guardados; una comprobación
OpenSSL independiente también devolvió `Verified OK`. No repetir esa firma
como primer paso. La causa de la entrada rechazada anterior sigue sin
confirmarse; no se modificó el tratamiento del PIN para obtener este éxito.

El primer intento del firmador usando el lanzador local falló con
`sun.security.pkcs11.wrapper.PKCS11Exception: CKR_FUNCTION_NOT_SUPPORTED`,
seguido de `signatureValue cannot be null!` y un aviso de sello/revocación.
No hay una firma PDF verificada. Ese aviso no demuestra un fallo de Internet:
falta primero el valor de firma. No hay stack trace ni APDU autenticadas para
identificar la función exacta. La revisión estática confirma SunPKCS11, cuyo
código OpenJDK 21 prefiere CKM_SHA256_RSA_PKCS sobre CKM_RSA_PKCS. La regresión
reproduce la ruta QSign fallida y la corrige con DigestInfo + RSA-PKCS mediante
el ajuste de capacidades. El intento posterior de las 20:37 con
`make sign/jcop3-sha256` produjo una firma verificada del texto original.
La llave pública extraída nuevamente del certificado y el DigestInfo
recalculado coinciden con los archivos guardados; una verificación OpenSSL
independiente también devolvió `Verified OK`. El mecanismo combinado queda
validado en esta prueba técnica. La validación del PDF sigue pendiente.
El usuario volvió al firmador y recibió `CKR_USER_NOT_LOGGED_IN` seguido
de los errores de firma nula. Pidió el PIN; el certificado seleccionado
no se confirmó inicialmente. El registro de las 21:06 ya confirma un
`C_Login CKR_OK`, llave 3, mecanismo `0x40` y fallo en `C_SignFinal`. Las
cuatro consultas de estado posteriores al login informan autenticación
(`PIN_STATUS a=2`, sesión RW USER `a=3`). No hay logout/cierre intermedio.
Los tres mensajes del usuario no corresponden a tres firmas en este
registro: solo hay un login y un `C_SignFinal`. Un logout previo al login
devuelve `0x101`; no confundirlo con el error posterior de firma.
El registro ampliado de las 21:24 confirma que selección, DOCP y MSE
pasan. RSA AT devuelve `6982`, y la revalidación de PIN queda rechazada
por caché desactivada. No hay otra operación RSA en ese registro.
Falta explicar qué condición de seguridad perdió o no satisfizo la tarjeta.
Las consultas posteriores de `C_GetSessionInfo` seleccionan MF y leen
política; eso es una diferencia concreta con la prueba CLI verificada.
El experimento de las 21:39 omitió esas consultas y el mismo rechazo RSA
persistió: la hipótesis no se confirma. No descarta todas las consultas de
PIN: el registro de las 21:54 confirma que `C_GetTokenInfo` llama a
`sc_pkcs15_get_pin_info` inmediatamente después del login y vuelve a
seleccionar la raíz IAS. Después hay cuatro consultas de sesión similares.
Las cinco informan PIN verificado. No se observaron selecciones de archivo
por enumeración de objetos tras login ni reinicios detectados al bloquear.
La firma selecciona ChipDoc, prepara MSE y recibe `6982`. El experimento
se amplió para omitir ambas clases de consulta en el perfil autenticado.
`TOKEN_PIN_CACHED` marca la omisión; las banderas de intentos del token usan
metadatos públicos anteriores y pueden estar desactualizadas. No afirma una
nueva lectura física del PIN, ni guarda su valor. El registro de las 23:34
confirma `TOKEN_PIN_CACHED` y tres `SESSION_LOGIN_STATE`, ninguna selección
raíz después del login, y RSA AT `9000` con retorno de 256 bytes y
`C_SignFinal CKR_OK`. No hay revalidación ni segunda operación RSA. Esto
sustenta revisar consultas/contexto como origen del fallo, pero no distingue
el efecto de SELECT, VERIFY vacío y GET DATA individualmente. Falta diseñar y validar
una corrección definitiva; el experimento sigue optativo. Los PDF se
verificaron después: la firma CAdES coincide con el certificado de la
tarjeta, y el CMS del sello RFC 3161 y su huella sobre el PDF final pasan.
Informe: `local/jcop3-pdf-verification-ydbv_z5o/report.json`; no se validaron
confianza ni revocación de los certificados.
No hay evidencia de PIN incorrecto, llave equivocada o problema de Internet.

Las consultas vacías de VERIFY devuelven `63 C7`: PIN no verificado, 7 intentos
restantes. No llevan el valor del PIN. El primer intento de firma no tuvo
depuración APDU; no se puede asegurar a partir del registro si el PIN llegó
a transmitirse. No hay evidencia de PIN incorrecto. La firma exitosa posterior
consta en `jcop3-signature-crUXrnAi/report.txt`.
Un `exit_status=0` en una herramienta no garantiza que no haya errores
internos de parsing: revisar las respuestas y el contexto.

## Interferencia de otro cliente: nueva candidata

XVK9ZA8V: 582 eventos, tres logins con VERIFY real 9000; RSA 9000/6982/9000.
xDibKvHa: 230 eventos, login real 9000 y RSA 6982. Locks/presencia normales,
sin retirada o atajo VERIFY_CACHED. Helper SELECT raíz 9000 en los informes
Bpr8civj/9oASsRGl/HqnS20vq/rhal9Uiu; el usuario reporta ejecución con diálogo
PIN abierto. El mecanismo exacto que pierde autorización no está demostrado.
La candidata agrega iasecc_chipdoc_select_for_verify después de merge_policy
antes de iasecc_chv_verify, solo opt-in y perfil/política conocidos. No afecta
GET_INFO o VERIFY vacío; errores de SELECT/política paran antes del PIN.
Diecisiete regresiones modelan CHV aceptado pero RSA sin autorización desde
raíz, control anterior, nueva secuencia, transporte/SW/PIN incorrecto sin
reintentos, fallo de política/consulta vacía y nueve perfiles excluidos.
Son simulaciones de contexto, no criptografía/hardware. La reproducción
PL8z86NE ya confirmó PRE_VERIFY_SELECT → VERIFY → RSA 9000 y PDF verificado
tras helper antes del PIN. Helper posterior al PIN todavía falla en 6xTEQpUr.
Mantener candidata optativa. Próximo protocolo en el encabezado de este
archivo **tiene prioridad sobre los pasos históricos que siguen**.

## Próximo trabajo, en orden

1. **Las dos firmas consecutivas ya pasaron y se verificaron.** No pedir
   recompilar ni repetirlas sin un motivo nuevo. Captura
   `local/jcop3-firmador-api-fgRfkdCF/`, resumen `analysis.json`:
   dos CARD_PIN_VERIFY exitosos, dos C_Login CKR_OK, logout software
   intermedio y dos RSA `9000` con 256 bytes. Cada login tiene cuatro consultas
   físicas antes de firmar, sin SELECT raíz en ese intervalo. Hay además dos
   consultas entre primera firma y nuevo login, incluso tras logout software.
   El estado CHV físico sigue autenticado; el segundo login ejecuta VERIFY real
   después de seleccionar raíz y leer política. No hay atajo por hash,
   omisión de consultas ni reset detectado.
2. Los PDF `local/jcop3-fresh-verify-1.pdf` y `local/jcop3-fresh-verify-2.pdf`
   se verificaron sin lector, PIN ni red. Informes privados:
   `local/jcop3-pdf-verification-w2t9vw2q/report.json` y
   `local/jcop3-pdf-verification-_5f871pd/report.json`, con copias signed.pdf.
   Ambos tienen 80485 bytes; CAdES cubre revisión de 38403 bytes y sello
   RFC 3161 se vincula al archivo final salvo su propio Contents.
   Certificado idéntico a LlaveDeFirma, CMS e imprint SHA-512 válidos;
   original, texto y página sin anotaciones conservados. No se validaron
   confianza ni revocación. Para versiones previas usar las copias y hashes
   de cada informe, no nombres o tamaños. No publicar esos archivos.
3. **Reinserción rápida corregida y validada en hardware; no repetir el caso.**
   Captura `und1k27k`, OpenSC 8842f394b limpio, analysis.json y copia estable
   api-reviewed.log con 463 eventos. Primera firma en línea 227; presencia
   PRESENT|CHANGED en 228 y 233, limpieza del token en 229 y luego slots vacíos.
   No se observó ausencia: el pase procesa cambio aun sin capturar tarjeta fuera.
   VERIFY real en 133 y 331, logins OK en 134 y 332, segunda firma en 425.
   Cada login tiene cuatro consultas físicas antes de firmar y ningún SELECT
   raíz en ese intervalo. Todos los locks y consultas de sesión/slot pasan.
   Ocho SLOT_TOKEN_REMOVED son dos rondas de cuatro slots, solo una con b=1;
   no representan ocho retiradas. No hay atajo por hash ni omisión de consultas.
   Los PDF `local/jcop3-hotplug-fixed-before.pdf` (80465 bytes) y
   `jcop3-hotplug-fixed-after.pdf` en raíz (80485 bytes) están verificados:
   informes privados `local/jcop3-pdf-verification-o7f8m3jn/report.json` y
   `local/jcop3-pdf-verification-m9wzm0yz/report.json`. Ambos usan LlaveDeFirma,
   CMS/imprint SHA-512 válidos y preservan original, texto y render sin anotaciones.
   No se validaron confianza/revocación. La raíz se conserva y queda ignorada.
   No hubo cambios del driver en esta revisión ni nuevas firmas del agente.
   Los fallos previos (`9ujB6ts6` y `y17eLwTD`) quedan como evidencia, no como
   bloqueo actual del caso. slot.c excluye CARD_CHANGED del atajo no_change,
   limpia ese estado al procesar el cambio y usa retry por llamada. La corrección
   afecta la capa PKCS#11 general; no modifica VERIFY/RSA ni el opt-in de firma.
   La prueba real cubre una reinserción y la simulación cinco reemplazos;
   múltiples ciclos reales, otros lectores y concurrencia siguen pendientes.
4. La candidata sigue **optativa** mediante
   `OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes`. PKCS#15 usa estado físico sin SELECT,
   limitado a Amos + Generic PKI + CHV global 1 + MF FILE_ID, excluyendo SO/PUK.
   Driver STATUS_ONLY requiere política pública conocida y solo conserva su
   máximo público de intentos, nunca estado ni contador restante. Si aún no
   se conoce, NOT_SUPPORTED permite inicialización completa. Fallos invalidan
   observaciones físicas; no afirmar que resuelven sesiones software o cambios
   externos. La ampliación desactiva consulta y almacenamiento del atajo SHA-1
   para VERIFY explícito Amos + Generic PKI completo + CHV global 1. Conserva
   estado previo, política/ACL/límites y un VERIFY real con la entrada del
   usuario, sin reintentos. No modifica logout físico. Fuera del opt-in se
   conserva el camino anterior. Pasan 256 regresiones. No activar caché PIN.
5. CARD_PIN_VERIFY y CARD_PIN_VERIFY_CACHED indican resultado del VERIFY
   mediante el driver ISO o retorno anticipado por hash. Llevan retorno y
   referencia pública, sin valor, longitud ni hash del PIN. El informe
   `pin_verify_experiment=fresh_verify` no basta sin eventos del módulo.
   La captura nueva confirma un VERIFY por login y ninguna marca CACHED.
   Firmador pidió PIN dos veces porque hace logout y login entre documentos,
   no porque la candidata genere un diálogo adicional o reintente un error.
   No inferir una política de una firma por PIN de la tarjeta. El experimento
   de omisión sigue disponible pero no convertirlo en predeterminado. SELECT
   raíz aislado ya reprodujo `6982`; C9 sigue sin significado confirmado.
   No activar atomic/lock_login, modificar PIN/llaves/ACL, forzar reset/logout
   físico ni capturar APDU autenticadas como arreglo especulativo.
6. **CLI ya probado: no repetir el mismo caso como primer paso.** Captura
   `Z0aJ18Ge`, 174 eventos; copia api-reviewed.log y analysis.json. VERIFY real
   en 96, C_Login OK en 97, llave 3 RSA 2048 en 165 y C_SignFinal OK en 174.
   SHA256-RSA-PKCS, RSA AT 9000 y 256 bytes. Dos consultas físicas después
   del login sin SELECT raíz. No hay atajo por hash o errores de lock/sesión.
   PDF local/jcop3-cli.pdf de 76918 bytes, informe privado
   `local/jcop3-pdf-verification-fttds461/report.json`: certificado LlaveDeFirma,
   firma CMS y sello RFC 3161/imprint SHA-512 válidos, original/texto/render
   conservados; no confianza/revocación/LTV. El usuario ejecutó pass por stdin,
   el agente no leyó el gestor ni firmó. El wrapper conserva stdin hacia
   firmador y rechaza salida existente; no registra el PIN. El tamaño distinto
   al PDF GUI no demuestra un error: la revisión CAdES mide 34916 bytes frente
   a los 38403 de las pruebas GUI, y contenido/render coinciden. No se atribuye
   esa diferencia a una causa concreta. No inferir identidad byte a byte de archivos firmados por ambos
   modos. Nueve regresiones del lanzador y helper TTY siguen vigentes.
7. **Selección ChipDoc antes de VERIFY validada tras helper previo al PIN.**
   PL8z86NE y PDF peer-fixed verificado. No repetir ese control automáticamente.
   Helper posterior al PIN en 6xTEQpUr: login OK, RSA 6982. No es rechazo local
   ni PIN incorrecto demostrado. No activar revalidación automática del PIN.
8. **Reserva de tarjeta desde login observada y firmas exitosas.** Helpers
   esperan hasta cerrar GUI y terminan 9000; control sin GUI pasa directamente.
   Seis operaciones RSA/SignFinal OK, con dos pares consecutivos en API.
   Nuevo doc-firmado.pdf verificado/copias en 37si22cp. No repetir estos casos.
   lock_login sigue optativo: limita compartición y puede durar hasta cerrar
   GUI; no prueba concurrencia independiente ni regla interna de tarjeta.
9. **Reinserción con transacción mantenida validada.** Última GUI lqSwrf8P,
   cambio de tarjeta entre dos firmas, nueva autenticación y RSA/SignFinal OK.
   Ambos PDF lock-hotplug-before/after verificados y copiados sdz525u1/pnc0zlig.
   No repetir caso ni compilar por esta revisión documental. Error inicial
   M6TUGgHo: dos rechazos locales -1300 antes de transmitir PIN; causa pendiente,
   no atribuir a error humano solo por la impresión del usuario.
10. **Consolidar integración.** Revisar 21 commits/20 archivos contra base local,
    separar correcciones, instrumentación y experimentos. Preservar rama y
    reproducir contra master actual antes de preparar reporte/PR según
    vendor/opensc/AGENTS.md. No cambiar configuración por defecto todavía;
    lock_login limita compartición. PuKDF y otros mecanismos/plataformas no
    validados. Si cambia driver, commit de submódulo primero y principal después.
    No generar llaves ni pedir otro PIN sin motivo nuevo.

**Firma RSA/PDF, consecutivas, reserva frente a otro cliente y reinserción
están respaldadas para la tarjeta/lector y perfil optativo probados. No se
reclama soporte completo ni solución del rechazo local intermitente.**

## PIN, privacidad e Idopte

- El PIN lo introduce el usuario sin eco en terminal/GUI, o lo entrega a
  firmador CLI por stdin desde pass, según su autorización explícita.
  Nunca ponerlo en argumentos, variables de entorno, archivos nuevos, chat,
  informes o capturas. El almacén existente de pass queda fuera del proyecto;
  el wrapper no lee ni guarda el flujo. No automatizar reintentos de PIN.
- `sign-jcop3.sh` exige stdin y stdout TTY porque `util_getpass` usa stdout
  para desactivar el eco. No redirigir ni pasar por `tee` la salida del comando
  autenticado. El helper fija `OPENSC_DEBUG=0` y `debug = 0`.
- El helper desactiva `use_pin_caching`: impide la revalidación automática
  de PKCS#15 después de un fallo de firma. No volver a activarla durante
  la investigación de compatibilidad.
- No habilitar capturas APDU durante login/firma: podrían contener el PIN.
  No se ha capturado una sesión autenticada de Idopte.
- La prueba firma el texto técnico visible en `message.txt`, no un PDF.
  Calcula SHA-256 en el equipo y prepara `digest-info.der` antes del login;
  la tarjeta recibe ese DigestInfo para RSA PKCS#1 v1.5, no el texto completo.
  Verifica `signature.bin` con la llave pública del certificado mediante
  OpenSSL. No intenta exportar la llave privada de la tarjeta.
- Firmar con la tarjeta no requiere Internet. Descargar dependencias Nix sí
  puede requerirlo; firmador puede usar red para otros servicios como sellado
  de tiempo o comprobación de revocación.
- `doc.pdf`, `doc-firmado.pdf` y los PDF de prueba /jcop3-*.pdf son archivos
  del usuario excluidos de Git. No incorporarlos a commits.
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
