/**
 *
 * Copyright (c) 2001, Frank Bu�
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * Parser class.
 */

#include "Token.h"
#include "Parser.h"

#include <math.h>
#include <iostream>
#include <time.h>

//Global RAM
//Added by Mathew Friedrichs 7/12/18
//As the parser is global for all Formula modules,
//we can take advantage of that and allocate a fixed chunk of "RAM"
//that any number of Formula modules can share.
//The current allocation is enough for 10 seconds of audio data at 44100Hz.
//The allocation may be lowered in the future as storing audio data
//locally on the module makes much more sense.
float GlobalParserRAM[441000];

//Read Global RAM function
//Takes in a float as a memory address
//and returns the value stored in that address.
//Will retun -10.0f on a "segfault".
float ParserReadGRAM(float argument1)
{
	int pos = (int) argument1;
	if (pos < 0 || pos > 440999) {
		return -10.0f;
	}
	return GlobalParserRAM[pos];
}

//Write Global RAM function
//Argument 1 is the value to be stored in the RAM.
//Argument 2 is the address the value is to be written to.
//Two modules can write to the same address without any crashing.
//The behavior can be used to sequence variables between functions.
//I would encourage using separate addresses and logical functions instead.
//Will return -10.0f on a "segfault".
float ParserWriteGRAM(float argument1, float argument2)
{
	int pos = (int) argument2;
	if (pos < 0 || pos > 440999) {
		return -10.0f;
	}
	GlobalParserRAM[pos] = argument1;
	return 0.0f;
}
//Potential future work:
//Add local RAM to each Formula module. For storing data in JSON!
//Keep a list of used global RAM positions. For real sefault action!
//run tolower or toupper on all strings.

float ParserMax(float argument1, float argument2)
{
	return argument1 > argument2 ? argument1 : argument2;
}

float ParserMin(float argument1, float argument2)
{
	return argument1 < argument2 ? argument1 : argument2;
}

float ParserTime()
{
	return time(NULL);
}

Parser::Parser(string expression)
{
	m_evaluator.setFunction("acos", acosf);
	m_evaluator.setFunction("asin", asinf);
	m_evaluator.setFunction("atan", atanf);
	m_evaluator.setFunction("atan2", atan2f);
	m_evaluator.setFunction("cos", cosf);
	m_evaluator.setFunction("cosh", coshf);
	m_evaluator.setFunction("exp", expf);
	m_evaluator.setFunction("abs", fabsf);
	m_evaluator.setFunction("mod", fmodf);
	m_evaluator.setFunction("log", logf);
	m_evaluator.setFunction("log2", log2f);
	m_evaluator.setFunction("log10", log10f);
	m_evaluator.setFunction("pow", powf);
	m_evaluator.setFunction("sin", sinf);
	m_evaluator.setFunction("sinh", sinhf);
	m_evaluator.setFunction("tan", tanf);
	m_evaluator.setFunction("tanh", tanhf);
	m_evaluator.setFunction("sqrt", sqrtf);
	m_evaluator.setFunction("ceil", ceilf);
	m_evaluator.setFunction("floor", floorf);
	m_evaluator.setFunction("max", ParserMax);
	m_evaluator.setFunction("min", ParserMin);
	//Custom Parser Functions
	//Read and Write to RAM
	//RGR = Read Global Ram
	m_evaluator.setFunction("rgr", ParserReadGRAM);
	//WGR = Write Global Ram
	m_evaluator.setFunction("wgr", ParserWriteGRAM);

	setExpression(expression);
}


char Parser::peekChar()
{
	if (m_currentIndex < (int) m_expression.size()) return m_expression[m_currentIndex];
	return 0;
}


void Parser::skipChar()
{
	m_currentIndex++;
}


char Parser::skipAndPeekChar()
{
	skipChar();
	return peekChar();
}


Token* Parser::peekToken()
{
	if (m_currentTokenIndex < (int) m_tokens.size()) return m_tokens[m_currentTokenIndex];
	return NULL;
}


Token* Parser::peekLastToken()
{
	Token* lastToken = NULL;
	if (m_currentTokenIndex > 0) lastToken = m_tokens[m_currentTokenIndex - 1];
	return lastToken;
}


Token* Parser::peekNextToken()
{
	Token* nextToken = NULL;
	if (m_currentTokenIndex + 1 < (int) m_tokens.size()) nextToken = m_tokens[m_currentTokenIndex + 1];
	return nextToken;
}


void Parser::skipToken()
{
	m_currentTokenIndex++;
}


string Parser::parseNumber(char c) throw(SyntaxError)
{
	string number;
	if (c != '.')
	{
		// parse before '.'
		while (c != 0 && c >= '0' && c <= '9' && c != '.' && c != 'e' && c != 'E') {
			number += c;
			c = skipAndPeekChar();
		}
	}
	if (c == '.')
	{
		// parse after '.'
		number += c;
		c = skipAndPeekChar();
		if (c != 0 && c >= '0' && c <= '9') {
			while (c != 0 && c >= '0' && c <= '9' && c != 'e' && c != 'E') {
				number += c;
				c = skipAndPeekChar();
			}
		} else {
			throw SyntaxError("Expected digit after '.', number: " + number);
		}
	}
	if (c == 'e' || c == 'E')
	{
		// parse after 'e' or 'E'
		number += c;
		c = skipAndPeekChar();
		if (c == '+' || c == '-') {
			number += c;
			c = skipAndPeekChar();
		}
		while (c != 0 && c >= '0' && c <= '9') {
			number += c;
			c = skipAndPeekChar();
		}
	}
	return number;
}


string Parser::parseIdentifier(char c)
{
	string identifier;
	identifier += c;
	c = skipAndPeekChar();
	while (c != 0 && ((c >= 'a' && c <= 'z')
	                  || (c >= 'A' && c <= 'Z')
	                  || (c >= '0' && c <= '9')
	                  || c == '_'))
	{
		identifier += c;
		c = skipAndPeekChar();
	}
	return identifier;
}


void Parser::deleteTokens()
{
	for (int i = 0; i < (int) m_tokens.size(); i++) delete m_tokens[i];
	m_tokens.clear();
}


void Parser::setExpression(string expression) throw(SyntaxError, TooManyArgumentsError)
{
	try {
		m_expression = string("(") + expression + ")";

		m_postfix = "";
		m_evaluator.removeAllActions();
		m_functionArgumentCountStack = stack<int>();
		m_operators = stack<Token*>();
		deleteTokens();

		m_currentIndex = 0;
		char c;
		Token* token;
		while ((c = peekChar())) {
			token = NULL;
			switch (c) {
			case '&':
				token = new AndToken();
				skipChar();
				break;
			case '|':
				token = new OrToken();
				skipChar();
				break;
			case '=':
				token = new EqualToken();
				skipChar();
				break;
			case '!':
				skipChar();
				if (peekChar() == '=') {
					skipChar();
					token = new NotEqualToken();
				} else {
					token = new NotToken();
				}
				break;
			case '<':
				skipChar();
				if (peekChar() == '=') {
					skipChar();
					token = new LessEqualToken();
				} else {
					token = new LessToken();
				}
				break;
			case '>':
				skipChar();
				if (peekChar() == '=') {
					skipChar();
					token = new GreaterEqualToken();
				} else {
					token = new GreaterToken();
				}
				break;
			case '+':
				token = new AddToken();
				skipChar();
				break;
			case '-':
				token = new SubToken();
				skipChar();
				break;
			case '*':
				token = new MulToken();
				skipChar();
				break;
			case '/':
				token = new DivToken();
				skipChar();
				break;
			case '^':
				token = new PowerToken();
				skipChar();
				break;
			case '(':
				token = new OpenBracketToken();
				skipChar();
				break;
			case ')':
				token = new CloseBracketToken();
				skipChar();
				break;
			case ',':
				token = new CommaToken();
				skipChar();
				break;
			default:
				if ((c >= '0' && c <= '9') || c == '.') {
					token = new NumberToken(parseNumber(c));
				} else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
					token = new IdentifierToken(parseIdentifier(c));
				} else if (c == 9 || c == 10 || c == 13 || c == 32) {
					skipChar();
					continue;
				} else {
					skipChar();
					throw SyntaxError(string("Invalid character: ") + c);
				}
			}
			if (token) m_tokens.push_back(token);
		}

		m_currentTokenIndex = 0;
		while ((token = peekToken())) token->eval(*this);
		if (m_operators.size() > 0) throw SyntaxError("Missing ')'.");
		if (m_postfix.size() > 0) m_postfix = m_postfix.substr(1);

	} catch (ParserException&)
	{
		deleteTokens();
		throw;
	}
	deleteTokens();
}
