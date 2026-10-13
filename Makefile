.DEFAULT_GOAL := help
.PHONY: sign/jcop3-select-root
.PHONY: firmador/jcop3-pin-status-in-place
.PHONY: firmador/jcop3-cli
.PHONY: firmador/jcop3-lock-login
.PHONY: inspect/jcop3-peer-context
.PHONY: probe/jcop4
.PHONY: inspect/jcop4-aids
.PHONY: inspect/jcop4-files
.PHONY: inspect/jcop4-pkcs15
.PHONY: inspect/jcop4-sign-setup
.PHONY: inspect/jcop4-pin-status
.PHONY: sign/jcop4-apdu sign/jcop4-verify-first
.PHONY: inspect/jcop4-security-info
.PHONY: inspect/jcop4-pin-policy
.PHONY: inspect/jcop4-odf-map
.PHONY: inspect/jcop4-file4141 inspect/jcop4-file4109
.PHONY: inspect/jcop4-idopte-compare
.PHONY: inspect/jcop4-idopte
.PHONY: inspect/jcop4-idopte-discovery inspect/jcop4-idopte-spy test/pcsc-public-observer

PDF_INPUT ?= doc.pdf
PDF_OUTPUT ?= local/jcop3-cli.pdf
export PDF_INPUT PDF_OUTPUT

READER ?= 0
export READER
JOBS ?= 2
export JOBS
SLOT_INDEX ?= 0
export SLOT_INDEX

.PHONY: help probe/jcop3 inspect/jcop3 read-dir/jcop3 build/opensc test/iasecc inspect/jcop3-built inspect/jcop3-pkcs11 inspect/jcop3-pin-context inspect/jcop3-pin-policy inspect/jcop3-key-policy inspect/jcop3-security-env inspect/jcop3-algorithms sign/jcop3 sign/jcop3-sha256 sign/jcop3-pin-status firmador/jcop3 firmador/jcop3-diagnose firmador/jcop3-session-state

help:
	@printf '%s\n' \
		'make probe/jcop3           Consulta lector y ATR; guarda el informe en local/.' \
		'make probe/jcop3 READER=1  Selecciona otro índice de lector.' \
		'make probe/jcop4           Consulta lector y ATR con la compilación local, sin PIN ni identificar aplicación.' \
		'make inspect/jcop4-aids    Comprueba cuatro AID conocidos con SELECT público, sin PIN ni PACE.' \
		'make inspect/jcop4-files   Lee metadatos de EF.ATR y enumera EF.DIR, sin PIN ni PACE.' \
		'make inspect/jcop4-pkcs15  Lee certificados y metadatos PKCS#15 del AID anunciado, sin login.' \
		'make inspect/jcop4-sign-setup Prueba una selección temporal RSA/SHA-256 por MSE, sin PIN ni firma.' \
		'make inspect/jcop4-pin-status Consulta el estado de PIN 1 con VERIFY vacío, sin introducir PIN.' \
		'make inspect/jcop4-security-info Consulta metadatos de autenticación, versión y CardAccess, sin PIN.' \
		'make inspect/jcop4-pin-policy Consulta DOCP del PIN 1 en raíz/aplicación, sin PIN ni autenticación.' \
		'make inspect/jcop4-odf-map Lee referencias del directorio 5031 sin seguirlas ni autenticar.' \
		'make inspect/jcop4-file4109 Consulta SELECT/metadatos del EF 4109 observado en Idopte, sin READ ni PIN.' \
		'make inspect/jcop4-file4141 Consulta SELECT/metadatos del EF 4141, sin leer contenido ni autenticar.' \
		'make inspect/jcop4-idopte Consulta módulo y estado público de los tokens Idopte, sin login.' \
		'make inspect/jcop4-idopte-discovery Observa cabeceras de descubrimiento público con bloqueo de comandos ajenos.' \
		'make inspect/jcop4-idopte-spy Captura solo SELECT 4141 para pcsc-spy, sin PIN ni lectura del EF.' \
		'make inspect/jcop4-idopte-compare READ 4141 hasta 400 bytes y SELECT 4109, sin leer 4109, guardar contenido ni autenticar.' \
		'make test/pcsc-public-observer Comprueba el observador sin lector ni tarjeta.' \
		'make sign/jcop4-apdu      Firma experimental RSA SHA-256 por APDU y verifica, un intento de PIN.' \
		'make sign/jcop4-verify-first Mismo diagnóstico, verifica PIN antes de MSE; un intento y verificación OpenSSL.' \
		'make inspect/jcop3         Identifica la aplicación y enumera objetos sin PIN.' \
		'make read-dir/jcop3        Lee EF.DIR por APDU conocidas, sin PIN.' \
		'make build/opensc          Compila el submódulo con PC/SC en build/opensc/.' \
		'make test/iasecc           Comprueba el parser con datos capturados, sin tarjeta.' \
		'make inspect/jcop3-built   Compila e inspecciona con el OpenSC modificado, sin PIN.' \
		'make inspect/jcop3-pkcs11  Consulta llaves, slots, mecanismos y objetos, sin PIN.' \
		'make inspect/jcop3-peer-context Selecciona raíz IAS desde otro cliente, sin PIN ni depuración.' \
		'make inspect/jcop3-pin-context  Compara contextos del PIN mediante consultas sin PIN.' \
		'make inspect/jcop3-pin-policy   Consulta campos estándar de la política, sin PIN.' \
		'make inspect/jcop3-key-policy   Consulta permisos DOCP de las llaves RSA, sin PIN.' \
		'make inspect/jcop3-security-env Consulta el entorno de seguridad RSA 3, sin PIN.' \
		'make inspect/jcop3-algorithms Prueba configuración temporal MSE de RSA/SHA-256, sin PIN.' \
		'make sign/jcop3            Firma un texto de prueba con PIN y verifica con OpenSSL.' \
		'make sign/jcop3-sha256     Prueba SHA256-RSA-PKCS con el mismo texto y verificación.' \
		'make sign/jcop3-pin-status Aísla una consulta vacía de estado después del login y firma un texto.' \
		'make sign/jcop3-select-root Aísla una selección de raíz IAS después del login y firma un texto.' \
		'make firmador/jcop3        Abre firmador con el módulo y perfil locales, sin depuración APDU.' \
		'make firmador/jcop3-diagnose Registra únicamente metadatos de sesiones y firma, sin PIN ni APDU.' \
		'make firmador/jcop3-pin-status-in-place Consulta el PIN conservando el contexto; corrección candidata optativa.' \
		'make firmador/jcop3-lock-login Prueba aislada: reserva la tarjeta desde login; otros clientes pueden esperar hasta cerrar GUI.' \
		'make firmador/jcop3-cli    Firma PDF_INPUT hacia PDF_OUTPUT con -dargs; PIN desde stdin o terminal.' \
		'make firmador/jcop3-session-state Prueba aislada: omite consultas físicas de PIN de sesión y token tras login.'

probe/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER"

probe/jcop4:
	@bash scripts/probe-jcop4.sh "$$READER"

inspect/jcop4-aids:
	@bash scripts/probe-jcop4.sh "$$READER" --aids

inspect/jcop4-files:
	@bash scripts/probe-jcop4.sh "$$READER" --files

inspect/jcop4-pkcs15:
	@bash scripts/probe-jcop4.sh "$$READER" --pkcs15

inspect/jcop4-sign-setup:
	@bash scripts/probe-jcop4.sh "$$READER" --sign-setup

inspect/jcop4-pin-status:
	@bash scripts/probe-jcop4.sh "$$READER" --pin-status

inspect/jcop4-security-info:
	@bash scripts/probe-jcop4.sh "$$READER" --security-info

inspect/jcop4-pin-policy:
	@bash scripts/probe-jcop4.sh "$$READER" --pin-policy

inspect/jcop4-odf-map:
	@bash scripts/probe-jcop4.sh "$$READER" --odf-map

inspect/jcop4-file4109:
	@bash scripts/probe-jcop4.sh "$$READER" --file4109

inspect/jcop4-file4141:
	@bash scripts/probe-jcop4.sh "$$READER" --file4141

inspect/jcop4-idopte-compare:
	@bash scripts/probe-jcop4.sh "$$READER" --idopte-compare

inspect/jcop4-idopte:
	@bash scripts/probe-jcop4.sh "$$READER" --idopte

inspect/jcop4-idopte-discovery:
	@bash scripts/probe-jcop4.sh "$$READER" --idopte-discovery

inspect/jcop4-idopte-spy:
	@bash scripts/probe-jcop4.sh "$$READER" --idopte-spy

test/pcsc-public-observer:
	@bash tests/pcsc-public-observer.sh

sign/jcop4-apdu:
	@bash scripts/sign-jcop4.sh

sign/jcop4-verify-first:
	@bash scripts/sign-jcop4.sh --verify-first

inspect/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER" --inspect

read-dir/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER" --read-dir

build/opensc:
	@bash scripts/build-opensc.sh "$$JOBS"

test/iasecc: build/opensc
	@bash scripts/test-iasecc.sh

inspect/jcop3-built: build/opensc
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --inspect

inspect/jcop3-pkcs11:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --pkcs11

inspect/jcop3-peer-context:
	@bash scripts/peer-context-jcop3.sh

inspect/jcop3-pin-context:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --pin-context

inspect/jcop3-pin-policy:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --pin-policy

inspect/jcop3-key-policy:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --key-policy

inspect/jcop3-security-env:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --security-env

inspect/jcop3-algorithms:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --algorithms

sign/jcop3:
	@bash scripts/sign-jcop3.sh

sign/jcop3-pin-status:
	@bash scripts/sign-jcop3.sh --pin-status

sign/jcop3-select-root:
	@bash scripts/sign-jcop3.sh --select-root

sign/jcop3-sha256:
	@bash scripts/sign-jcop3.sh --sha256

firmador/jcop3:
	@bash scripts/firmador-jcop3.sh

firmador/jcop3-diagnose:
	@bash scripts/firmador-jcop3.sh --trace-api

firmador/jcop3-session-state:
	@bash scripts/firmador-jcop3.sh --session-state

firmador/jcop3-pin-status-in-place:
	@bash scripts/firmador-jcop3.sh --pin-status-in-place

firmador/jcop3-lock-login:
	@bash scripts/firmador-jcop3.sh --lock-login

firmador/jcop3-cli:
	@bash scripts/firmador-jcop3.sh --cli "$$PDF_INPUT" "$$PDF_OUTPUT"
