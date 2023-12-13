/**
 * @file
 * @author Diego Passos <dpassos@ic.uff.br>
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 * Módulo que realiza o parsing do arquivo de configuração.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "net.h"

/**
 * Tamanho máximo de um lexema.
 */
#define BUFFER_SIZE	300

/*
 * Variaveis Globais.
 */

/**
 * Linha atual.
 * Guarda a linha atualmente analisada no arquivo de configuração.
 */
int currLine = 1;

/**
 * Descritor do arquivo de configuração.
 * Guarda o ponteiro para o buffer de leitura do arquivo de configuração.
 */
FILE * cfg_file;

/*
 * Analise Lexica.
 */

/**
 * Faz o dump de uma variável do tipo string.
 * @param x ponteiro para a string.
 * @return Nada.
 */
#define PRINT_STRING(x)	printf("%s = \"%s\"\n", #x, x)

/**
 * Faz o dump de uma variável do tipo int.
 * @param x valor inteiro.
 * @return Nada.
 */
#define PRINT_INT(x)	printf("%s = %d\n", #x, x)

/**
 * Faz o dump de uma variável do tipo float.
 * @param x valor real.
 * @return Nada.
 */
#define PRINT_FLOAT(x)	printf("%s = %f\n", #x, x)

/**
 * Faz o dump de uma lista de endereços IP (possivelmente com máscara).
 * @param x pointeiro para a lista de IPs.
 * @return Nada.
 */
#define PRINT_IP_LIST(x) \
			{\
				printf("%s = \n", #x);\
				t_ip_list * __m_p = x;\
				while(__m_p) {\
						\
					printf("\t- %s/%d\n", inetaddr(__m_p->ip), __m_p->mask);\
					__m_p = __m_p->next;\
				}\
			}


/*
 * TODO: generalizar o macro IS_LETTER.
 */

/**
 * Insere um novo caracter na string do token
 * atual.
 * @param s ponteiro para a string do token atual.
 * @param ch caracter a ser inserido.
 * @param read numero de caracteres da string.
 * @return Nada.
 * @note Verifica overflow na string.
 */
#define APPEND_CHAR(s, ch, read) if (read == BUFFER_SIZE) {\
					fprintf(stderr, "String is too long on line %d\n", currLine);\
					return(TOKEN_ERROR);\
				 }\
				 s[read + 1] = 0; \
				 s[read++] = ch

/**
 * Remove o último caracter da string do token atual.
 * @param s ponteiro para a string do token atual.
 * @return Nada.
 * @note Verifica underflow.
 */
#define REMOVE_LAST(s)		if (strlen(s) > 0) s[strlen(s) - 1] = 0;

/**
 * Lê o próximo caracter do arquivo de configuração.
 * @param ch variável de saída que armazena caracter lido.
 * @return Nada.
 * @note Se o caracter lido é uma quebra de linha, atualiza
 * a variável currLine.
 */
#define NEXTCHAR(ch)		ch = fgetc(cfg_file); \
				if (ch == '\n') currLine++

/**
 * Retorna um caracter para o buffer do arquivo de configuração.
 * @param ch caracter a ser retornado.
 * @return Nada.
 * @note Se o caracter a ser retornado é uma quebra de linha,
 * atualiza a variável currLine.
 */
#define BACKCHAR(ch)		(ungetc(ch, cfg_file)); \
				if (ch == '\n') currLine--;

/**
 * Retorna o token atual e retorna o último caracter lido para
 * o buffer do arquivo de configuração.
 * @param token Token a ser retornado.
 * @param ch Caracter a ser colocado de volta no buffer.
 * @return Nada.
 * @sa nextToken(char *, char *, int *, float *)
 */
#define RETURN(token,ch)	BACKCHAR(ch);\
				return(token)

/**
 * Testa se caracter é um espaço em branco.
 */
#define IS_BLANK(ch)		(ch == '\r' || ch == ' ' || ch == '\t')
/**
 * Testa se caracter é um dígito.
 */
#define IS_DIGIT(ch)		(ch <= '9' && ch >= '0')
/**
 * Testa se caracter é um sinal de igual.
 */
#define IS_EQUAL(ch)		(ch == '=')
/**
 * Testa se caracter é uma letra.
 */
#define IS_LETTER(ch)		((ch <= 'z' && ch >= 'a') || \
				(ch <= 'Z' && ch >= 'A') || ch == '_' || ch == '/' || ch == '.')
/**
 * Testa se caracter é uma quebra de linha.
 */
#define IS_LINEBREAK(ch)	(ch == '\n')
/**
 * Testa se caracter é uma aspa dupla.
 */
#define IS_QUOTE(ch)		(ch == '"')
/**
 * Testa se caracter é um ponto.
 */
#define IS_DOT(ch)		(ch == '.')
/**
 * Testa se caracter é uma vírgula.
 */
#define IS_COMMA(ch)		(ch == ',')
/**
 * Testa se caracter é uma barra.
 */
#define IS_SLASH(ch)		(ch == '/')
/**
 * Testa se caracter é um abre-chaves.
 */
#define IS_OPEN_CBRACKET(ch)	(ch == '{')
/**
 * Testa se caracter é um fecha-chaves.
 */
#define IS_CLOSE_CBRACKET(ch)	(ch == '}')
/**
 * Testa se caracter é uma tralha.
 */
#define IS_SHARP(ch)		(ch == '#')
/**
 * Testa se caracter é um fim de arquivo.
 */
#define IS_EOF(ch)		(ch == (char) EOF)

/**
 * Tipos de tokens aceitos.
 */
typedef enum {

	TOKEN_EQUAL,
	TOKEN_END_LINE,
	TOKEN_VAR,
	TOKEN_CONST_STRING,
	TOKEN_CONST_FLOAT,
	TOKEN_CONST_INT,
	TOKEN_CONST_SUBNET,
	TOKEN_OPEN_CBRACKETS,
	TOKEN_CLOSE_CBRACKETS,
	TOKEN_COMMA,
	TOKEN_ERROR,
	TOKEN_EOF
} t_token;

/**
 * Vetor de conversão de tokens para strings de descrição.
 */
char * token2str[] = {
	"igual",
	"quebra de linha",
	"variavel",
	"string",
	"valor real",
	"valor inteiro",
	"endereco de subrede",
	"abre chaves",
	"fecha chaves",
	"virgula",
	"",
	"fim de arquivo"
};

/**
 * Retorna o próximo token sintático do arquivo de configuração.
 * @param resWord variável de saída para o token secundário (se for uma palavra
 * reservada).
 * @param string variável de saída para o token secundário (se for uma string).
 * @param integer variável de saída para o token secundário (se for um inteiro).
 * @param real variável de saída para o token secundário (se for um real).
 * @return Token sintático lido.
 * @sa t_token
 */
t_token nextToken(char * resWord, char * string, int * integer, float * real) {

	char buffer[BUFFER_SIZE];
	char ch, ich;
	int read;
	int state;

	buffer[0] = 0;
	read = 0;
	state = 0;

	while(read < BUFFER_SIZE) {

		NEXTCHAR(ich);
		ch = (char) ich;

		APPEND_CHAR(buffer, ch, read);
		switch(state) {

			case 0:

				if (IS_BLANK(ch)) {

					state = 0;

					/*
					 * "BLANK" nao faz parte do token secundario.
					 */

					buffer[0] = 0;
					read--;
					break ;
				}

				if (IS_LINEBREAK(ch)) {

					state = 1;
					break ;
				}

				if (IS_EQUAL(ch)) {

					state = 2;
					break ;
				}
				
				if (IS_LETTER(ch)) {

					state = 3;
					break ;
				}

				if (IS_QUOTE(ch)) {

					state = 4;
					break ;
				}

				if (IS_DIGIT(ch)) {

					state = 5;
					break ;
				}

				if (IS_SHARP(ch)) {

					state = 9;
					break ;
				}

				if (IS_COMMA(ch)) {

					state = 10;
					break ;
				}

				if (IS_OPEN_CBRACKET(ch)) {

					state = 11;
					break ;
				}

				if (IS_CLOSE_CBRACKET(ch)) {

					state = 12;
					break ;
				}

				if (IS_EOF(ch) || feof(cfg_file)) {

					state = 13;
					break ;
				}

				/*
				 * Nao eh estado final.
				 */

				fprintf(stderr, "Invalid token near \'%c\', on line %d\n", ch, currLine);
				RETURN(TOKEN_ERROR, ch);

			case 1:

				/*
				 * Estado final (e poco).
				 */

				RETURN(TOKEN_END_LINE, ch);

			case 2:

				/*
				 * Estado final (e poco).
				 */

				RETURN(TOKEN_EQUAL, ch);

			case 3:

				/*
				 * Estado final, mas com transicao.
				 */

				if (IS_LETTER(ch)) {

					state = 3;
					break ;
				}

				REMOVE_LAST(buffer);
				strcpy(resWord, buffer);
				RETURN(TOKEN_VAR, ch);

			case 4:

				if (IS_LETTER(ch) || IS_DIGIT(ch)) {

					state = 4;
					break ;
				}

				if (IS_QUOTE(ch)) {

					state = 7;
					break ;
				}

				/*
				 * Nao eh estado final.
				 */

				fprintf(stderr, "Invalid string format on line %d: expected letter or digit. Found \'%c\'\n", currLine, ch);
				RETURN(TOKEN_ERROR, ch);

			case 5:

				/*
				 * Estado final com transicao.
				 */

				if (IS_DIGIT(ch)) {

					state = 5;
					break ;
				}

				if (IS_DOT(ch)) {

					state = 6;
					break ;
				}

				* integer = atoi(buffer);
				RETURN(TOKEN_CONST_INT, ch);

			case 6:

				if (IS_DIGIT(ch)) {

					state = 8;
					break ;
				}

				if (IS_DOT(ch)) {

					state = 14;
					break ;
				}

				/*
				 * Nao eh estado final.
				 */

				fprintf(stderr, "Invalid float format on line %d: digit. Found \'%c\'\n", currLine, ch);
				RETURN(TOKEN_ERROR, ch);

			case 7:

				/*
				 * Estado final e poco.
				 */

				/*
				 * Retirar as aspas antes de retornar o token
				 * secundario.
				 */

				strcpy(string, & buffer[1]);
				REMOVE_LAST(string);
				REMOVE_LAST(string);

				RETURN(TOKEN_CONST_STRING, ch);

			case 8:

				/*
				 * Estado final, com transicao.
				 */

				if (IS_DIGIT(ch)) {

					state = 8;
					break ;
				}

				if (IS_DOT(ch)) {

					state = 14;
					break ;
				}

				* real = atof(buffer);
				RETURN(TOKEN_CONST_FLOAT, ch);

			case 9:

				if (IS_LINEBREAK(ch)) {

					/*
					 * Tudo aqui foi um comentario. Vamos "roubar" um pouco 
					 * e retornar para o analisador sintatico que o token lido 
					 * foi uma quebra de linha.
					 */

					RETURN(TOKEN_END_LINE, ch);
				}

				if (IS_EOF(ch) || feof(cfg_file)) {

					state = 10;
					break ;
				}

				state = 9;
				break ;

			case 10:

				/*
				 * Estado final e poco.
				 */

				RETURN(TOKEN_COMMA, ch);

			case 11:

				/*
				 * Estado final e poco.
				 */

				RETURN(TOKEN_OPEN_CBRACKETS, ch);

			case 12:

				/*
				 * Estado final e poco.
				 */

				RETURN(TOKEN_CLOSE_CBRACKETS, ch);

			case 13:

				/*
				 * Se chegamos aqui, foi lido o EOF.
				 */

				RETURN(TOKEN_EOF, ch);

			case 14:

				if (IS_DIGIT(ch)) {

					state = 14;
					break ;
				}

				if (IS_DOT(ch)) {

					state = 15;
					break ;
				}

				/*
				 * Nao eh estado final.
				 */

				fprintf(stderr, "Invalid subnet format on line %d: third byte. Found \'%c\'\n", currLine, ch);
				RETURN(TOKEN_ERROR, ch);
				
			case 15:

				if (IS_DIGIT(ch)) {

					state = 15;
					break ;
				}

				if (IS_SLASH(ch)) {

					state = 16;
					break ;
				}

				/*
				 * Nao eh estado final.
				 */

				fprintf(stderr, "Invalid subnet format on line %d: fourth byte. Found \'%c\'\n", currLine, ch);
				RETURN(TOKEN_ERROR, ch);
				
			case 16:

				/*
				 * Estado final e poco.
				 */

				if (IS_DIGIT(ch)) {

					state = 16;
					break ;
				}

				strcpy(string, buffer);
				REMOVE_LAST(string);

				RETURN(TOKEN_CONST_SUBNET, ch);

			default:

				break ;
		}
	}

	/*
	 * Apenas para evitar o warning do compilador.
	 */

	return(0);
}

/*
 * Analise Sintatica.
 */

/**
 * Realiza a análise sintática do arquivo de configuração. O parser é implementado 
 * como um pequeno autômato finito.
 * @param filename ponteiro para string contendo o nome (caminho) do arquivo de configuração.
 * @return Nada.
 */
void sintatics(char * filename) {

	t_token t;
	char string[BUFFER_SIZE];
	char resWord[BUFFER_SIZE];
	int i;
	float r;
	int state;
	int currVar;	/* Indice da variavel sendo referenciada. */
	t_assign_type type;

	cfg_file = fopen(filename, "r");
	if (cfg_file == NULL) {

		fprintf(stderr, "Falha ao tentar abrir o arquivo '%s':\n", filename);
		perror(NULL);
		exit(1);
	}

	init_var_pool();

	state = 0;

	while(1) {

		t = nextToken(resWord, string, & i, & r);
		if (t == TOKEN_ERROR) {

			fclose(cfg_file);
			exit(1);
		}

		switch(state) {

			case 0:

				if (t == TOKEN_EOF) {

					fclose(cfg_file);
					return ;
				}

				if (t == TOKEN_END_LINE) {

					state = 0;
					break ;
				}

				if (t == TOKEN_VAR) {

					/*
					 * Precisamos confirmar que o usuario
					 * esta referenciando uma variavel valida.
					 */

					if ((currVar = find_variable(resWord, & type)) < 0) {

						/*
						 * Nao eh valida.
						 */

						fprintf(stderr, "Erro no arquivo de configuracao (linha %d): variavel desconhecida ('%s').\n", 
							currLine, resWord);

						fclose(cfg_file);
						exit(1);
					}

					state = 1;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): nome de variavel, quebra de linha ou fim de arquivo"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);

			case 1:

				if (t == TOKEN_EQUAL) {

					state = 2;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): igual"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);
			
			case 2:

				if (t == TOKEN_CONST_STRING) {

					if (type != TYPE_STRING) {

						fprintf(stderr, "Erro de tipo no arquivo de configuracao (linha %d): string esperada.\n", currLine);
						fclose(cfg_file);
						exit(1);
					}

					assign_variable_value(currVar, string);
					state = 3;
					break ;
				}

				if (t == TOKEN_CONST_INT) {

					if (type != TYPE_INTEGER) {

						fprintf(stderr, "Erro de tipo no arquivo de configuracao (linha %d): valor inteiro esperado.\n", currLine);
						fclose(cfg_file);
						exit(1);
					}

					assign_variable_value(currVar, & i);

					state = 3;
					break ;
				}

				if (t == TOKEN_CONST_FLOAT) {

					if (type != TYPE_FLOAT) {

						fprintf(stderr, "Erro de tipo no arquivo de configuracao (linha %d): valor real esperado.\n", currLine);
						fclose(cfg_file);
						exit(1);
					}

					assign_variable_value(currVar, & r);

					state = 3;
					break ;
				}

				if (t == TOKEN_OPEN_CBRACKETS) {

					/*
					 * Lista de valores.
					 */

					state = 5;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): constante ou abre-chaves"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);

			case 3:

				if (t == TOKEN_END_LINE) {

					state = 4;
					break ;
				}

				if (TOKEN_EOF) {

					fclose(cfg_file);
					return ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): quebra de linha"
						" ou fim de arquivo esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);


			case 4:

				if (t == TOKEN_END_LINE) {

					state = 4;
					break ;
				}

				if (t == TOKEN_VAR) {

					/*
					 * Precisamos confirmar que o usuario
					 * esta referenciando uma variavel valida.
					 */

					if ((currVar = find_variable(resWord, & type)) < 0) {

						/*
						 * Nao eh valida.
						 */

						fprintf(stderr, "Erro no arquivo de configuracao (linha %d): variavel desconhecida ('%s').\n", 
							currLine, resWord);

						fclose(cfg_file);
						exit(1);
					}

					state = 1;
					break ;
				}

				if (TOKEN_EOF) {

					fclose(cfg_file);
					return ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): quebra de linha, nome de variavel ou"
						" fim de arquivo esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);

			case 5:

				if (t == TOKEN_CONST_SUBNET) {

					assign_variable_value(currVar, string);
					state = 6;
					break ;
				}

				if (t == TOKEN_CLOSE_CBRACKETS) {

					state = 3;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): sub-rede ou fecha-chaves"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);

			case 6:

				if (t == TOKEN_COMMA) {

					state = 7;
					break ;
				}

				if (t == TOKEN_CLOSE_CBRACKETS) {

					state = 3;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): virgula ou fecha-chaves"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);

			case 7:

				if (t == TOKEN_CONST_SUBNET) {

					assign_variable_value(currVar, string);
					state = 6;
					break ;
				}

				/*
				 * Estado nao eh final.
				 */

				fprintf(stderr, "Erro de sintaxe no arquivo de configuracao (linha %d): sub-rede"
						" esperado. Obtido %s.\n", currLine, token2str[t]);
				fclose(cfg_file);
				exit(1);
		}
	}
}

/**
 * Função principal do parser. Verifica os argumentos, chama o analisador 
 * sintático e valida os valores das variáveis.
 * @param filename ponteiro para a string contendo o nome (caminho) do arquivo de configuração.
 * @return Nada.
 */
void parse_cfg(char * filename) {

	if (filename) sintatics(filename);
	validate_variables();

	printf("####################################################\n");
	printf("Configuracao:\n");
	printf("####################################################\n");

	PRINT_STRING(ifce_name);
	PRINT_STRING(rmmod_path);
	PRINT_STRING(insmod_path);
	PRINT_STRING(pprs_path);
	PRINT_STRING(pprs_classes_file);
	PRINT_STRING(pprs_rates_file);
	PRINT_STRING(per_table_path);

	PRINT_INT(port);
	PRINT_INT(lq_level);
	PRINT_INT(hello_life_time);
	PRINT_INT(topology_life_time);
	PRINT_INT(hello_intervall);
	PRINT_INT(topology_intervall);

	PRINT_FLOAT(aging);

	PRINT_IP_LIST(sub_nets);

	printf("####################################################\n");

}

