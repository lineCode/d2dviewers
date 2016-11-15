// jsmn.h
#pragma once
#include <stddef.h>
//{{{
#ifdef __cplusplus
	extern "C" {
#endif
//}}}

// JSON type identifier. Basic types are:
//  o Object
//  o Array
//  o String
//  o Other primitive: number, boolean (true/false) or null
typedef enum {
	JSMN_UNDEFINED = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3,
	JSMN_PRIMITIVE = 4
	} jsmntype_t;

enum jsmnerr {
	JSMN_ERROR_NOMEM = -1, /* Not enough tokens were provided */
	JSMN_ERROR_INVAL = -2, /* Invalid character inside JSON string */
	JSMN_ERROR_PART = -3   /* The string is not a full JSON packet, more bytes expected */
	};

// JSON token description.
// type   type (object, array, string etc.)
// start  start position in JSON data string
// end    end position in JSON data string
typedef struct {
	jsmntype_t type;
	int start;
	int end;
	int size;
#ifdef JSMN_PARENT_LINKS
	int parent;
#endif
	} jsmntok_t;

// JSON parser. Contains array of token blocks available, also stores string being parsed now and curposition in string
typedef struct {
	unsigned int pos;     /* offset in the JSON string */
	unsigned int toknext; /* next token to allocate */
	int toksuper;         /* superior token node, e.g parent object or array */
	} jsmn_parser;

void jsmn_init (jsmn_parser* parser);
int jsmn_parse (jsmn_parser* parser, const char* js, size_t len, jsmntok_t* tokens, unsigned int num_tokens);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
