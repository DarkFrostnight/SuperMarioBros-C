flex asm6502.l && yacc -d asm6502.y && g++ asm6502.tab.c lex.yy.c ast.cpp translator.cpp util.cpp -o parser