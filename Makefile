build:
	go tool yacc -o asm6502/y.go -v /dev/null asm6502/asm6502.y
	${GOPATH}/bin/nex -e asm6502/asm6502.nex
	go build -o jamulator main.go

dev:
	go get github.com/superjoe30/nex

.PHONY: build clean dev
