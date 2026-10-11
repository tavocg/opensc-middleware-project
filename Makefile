.DEFAULT_GOAL := help

READER ?= 0
export READER
JOBS ?= 2
export JOBS
SLOT_INDEX ?= 0
export SLOT_INDEX

.PHONY: help probe/jcop3 inspect/jcop3 read-dir/jcop3 build/opensc test/iasecc inspect/jcop3-built inspect/jcop3-pkcs11 inspect/jcop3-pin-context inspect/jcop3-pin-policy inspect/jcop3-key-policy sign/jcop3

help:
	@printf '%s\n' \
		'make probe/jcop3           Consulta lector y ATR; guarda el informe en local/.' \
		'make probe/jcop3 READER=1  Selecciona otro índice de lector.' \
		'make inspect/jcop3         Identifica la aplicación y enumera objetos sin PIN.' \
		'make read-dir/jcop3        Lee EF.DIR por APDU conocidas, sin PIN.' \
		'make build/opensc          Compila el submódulo con PC/SC en build/opensc/.' \
		'make test/iasecc           Comprueba el parser con datos capturados, sin tarjeta.' \
		'make inspect/jcop3-built   Compila e inspecciona con el OpenSC modificado, sin PIN.' \
		'make inspect/jcop3-pkcs11  Consulta llaves, slots, mecanismos y objetos, sin PIN.' \
		'make inspect/jcop3-pin-context  Compara contextos del PIN mediante consultas sin PIN.' \
		'make inspect/jcop3-pin-policy   Consulta campos estándar de la política, sin PIN.' \
		'make inspect/jcop3-key-policy   Consulta permisos DOCP de las llaves RSA, sin PIN.' \
		'make sign/jcop3            Firma un texto de prueba con PIN y verifica con OpenSSL.'

probe/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER"

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

inspect/jcop3-pin-context:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --pin-context

inspect/jcop3-pin-policy:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --pin-policy

inspect/jcop3-key-policy:
	@PATH="$(CURDIR)/build/opensc/src/tools:$$PATH" bash scripts/probe-jcop3.sh "$$READER" --key-policy

sign/jcop3:
	@bash scripts/sign-jcop3.sh
