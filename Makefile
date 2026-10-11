.DEFAULT_GOAL := help

READER ?= 0
export READER
JOBS ?= 2
export JOBS

.PHONY: help probe/jcop3 inspect/jcop3 read-dir/jcop3 build/opensc test/iasecc inspect/jcop3-built

help:
	@printf '%s\n' \
		'make probe/jcop3           Consulta lector y ATR; guarda el informe en local/.' \
		'make probe/jcop3 READER=1  Selecciona otro índice de lector.' \
		'make inspect/jcop3         Identifica la aplicación y enumera objetos sin PIN.' \
		'make read-dir/jcop3        Lee EF.DIR por APDU conocidas, sin PIN.' \
		'make build/opensc          Compila el submódulo con PC/SC en build/opensc/.' \
		'make test/iasecc           Comprueba el parser con datos capturados, sin tarjeta.' \
		'make inspect/jcop3-built   Compila e inspecciona con el OpenSC modificado, sin PIN.'

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
