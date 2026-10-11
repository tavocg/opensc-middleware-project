.DEFAULT_GOAL := help

READER ?= 0
export READER

.PHONY: help probe/jcop3 inspect/jcop3 read-dir/jcop3

help:
	@printf '%s\n' \
		'make probe/jcop3           Consulta lector y ATR; guarda el informe en local/.' \
		'make probe/jcop3 READER=1  Selecciona otro índice de lector.' \
		'make inspect/jcop3         Identifica la aplicación y enumera objetos sin PIN.' \
		'make read-dir/jcop3        Lee EF.DIR por APDU conocidas, sin PIN.'

probe/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER"

inspect/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER" --inspect

read-dir/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER" --read-dir
