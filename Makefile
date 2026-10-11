.DEFAULT_GOAL := help

READER ?= 0
export READER

.PHONY: help probe/jcop3

help:
	@printf '%s\n' \
		'make probe/jcop3           Consulta lector y ATR; guarda el informe en local/.' \
		'make probe/jcop3 READER=1  Selecciona otro índice de lector.'

probe/jcop3:
	@bash scripts/probe-jcop3.sh "$$READER"
