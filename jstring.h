#ifndef JSTRING_H
#define JSTRING_H

#ifndef LOWER
#define LOWER(x) ((x >= 'A' && x <= 'Z')?(x+32):x)
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

class justin_string_parse;
typedef class justin_string_parse JSTRING;

class justin_string_parse {
private:
	char *stringptr;
public:
	justin_string_parse(char *string) {
		this->linecount = 0;
		this->stringlen = 0;
		this->stringptr = 0;
		if (!string) return;
		this->string(string);
	};
	justin_string_parse(void) {
		this->linecount = 0;
		this->stringlen = 0;
		this->stringptr = 0;
	};
	//~justin_string_parse();
	struct jsstring {
		char *ptr;
		size_t length;
	};
	size_t stringlen;
	size_t linecount;

	/*
	Returns a pointer to the start of the line, and puts the
	line's string length into "length".
	*/
	char *line(int linenumber, size_t *length);
	static char *line(char *string, int linenumber, size_t *length);


	static int countlines(char *string);
	static int countlines(char *string, size_t length);


	/*
	countfields( char *string, char delimiter, size_t length)
	Returns the number of fields are in the string.
	*/
	static size_t countfields(char *string, char delimiter); /* Null-Terminated String*/
	static size_t countfields(char *string, char delimiter, char terminator); /* String terminated by character "terminator" */
	static size_t countfields(char *string, char delimiter, size_t length); /* String of "length" length */




																			/* Gets a pointer to the string */
	char *string();

	/* Sets the string */
	void string(char *);

	/* For "Header: Value" pairs*/
	static char *headername(char *ptr, char string_terminator, size_t *outputlength);
	static char *headername(char *ptr, size_t *outputlength);
	char *headername(char string_terminator, size_t *outputlength);
	char *headername(size_t *outputlength);
	static char *headervalue(char *ptr, char string_terminator, size_t *outputlength);
	static char *headervalue(char *ptr, size_t *outputlength);
	char *headervalue(char string_terminator, size_t *outputlength);
	char *headervalue(size_t *outputlength);


	/* For "key=value" pairs */
	static char *keyname(char *ptr, char string_terminator, size_t *outputlength);
	static char *keyname(char *ptr, size_t *outputlength);
	char *keyname(char string_terminator, size_t *outputlength);
	char *keyname(size_t *outputlength);
	static char *keyvalue(char *ptr, size_t *inputlength, size_t *outputlength);
	static char *keyvalue(char *ptr, char string_terminator, size_t *outputlength);
	static char *keyvalue(char *ptr, size_t *outputlength);
	char *keyvalue(char string_terminator, size_t *outputlength);
	char *keyvalue(size_t *outputlength);



	/*
	This function converts a string referenced
	by a pointer and a length to a
	null-terminated string.

	ARGUMENTS:
	1. A pointer to a JSTRING::jsstring strucure
	describing the position of the text to use.

	RETURNS: A null-terminated string that *must
	be freed*
	*/
	static char *makeNTS(justin_string_parse::jsstring *jstring);

	/*
	This function converts a string referenced
	by a pointer and a length to a
	null-terminated string.

	ARGUMENTS:
	1. A pointer to text.
	2. The length in bytes of the string to
	be returned.

	RETURNS: A null-terminated string that *must
	be freed*
	*/
	static char *makeNTS(char *, size_t);

	/*
	Returns a null-terminated array of struct jsstring pointers
	*/
	static struct jsstring **split(char *string, char delimiter, char stringterminator);

	/* Assumes NULL terminator */
	static struct jsstring **split(char *string, char delimiter);

	/* Split string of "len" characters */
	static struct jsstring **split(char *string, char delimiter, size_t len);


	struct jsstring **split(char delimiter, char stringterminator);

	/* Assumes NULL terminator */
	struct jsstring **split(char delimiter);

	/* Split string of "len" characters */
	struct jsstring **split(char delimiter, size_t len);


	/* Frees an arrya returned by split() */
	static void freesplit(JSTRING::jsstring **);

	/*
	Case-insensitive check to see if "str1" == "str2".
	Also, discards any white space preceeding "str1" or "str2"
	*/
	static int matches(char *str1, char *str2);

	/* Returns 1 if the string matches BACKWARDS disregarding whitespace */
	static int reverse_matches(char *, char *);

	static char *unquote(JSTRING::jsstring *js, size_t *length);
	static char *unquote(char *string, size_t *inlength, size_t *outlength);
	static char *unquote(char *string, size_t inlength, size_t *outlength);



	/*
	Returns a pointer to the first non-whitespace character
	in "string", and populates the "outputlength" with the length
	of the string *excluding* trailing whitespace.

	"string"- The string to trim.
	"stringlength"- The size in bytes of the string to trim.
	"outputlength"- A pointer to a size_t variable into which the
	length of the "trimmed" string will be placed.
	*/
	static char *trim(char *string, size_t stringlength, size_t *outputlength);
	static char *trim(char *string, size_t *stringlength, size_t *outputlength);
	static char *trim(char *string, size_t *outputlength);

	char *trim(size_t *outputlength);

	/*
	Searches the entire string pointed to at "string" for
	"char_to_find".  Returns its zero-based index, or -1
	if it is not found.
	*/
	static int charpos(char *string, char char_to_find, char stop_at_char, size_t length_to_search);
	static int charpos(char *string, char char_to_find, char stop_at_char);
	static int charpos(char *string, char char_to_find);


	int charpos(char char_to_find, char stop_at_char, size_t length_to_search);
	int charpos(char char_to_find, char stop_at_char);
	int charpos(char char_to_find);



	char *between(const char *frame, size_t *length);
	static char *between(char *string, const char *frame, size_t *length);

	int haschar(char testchar, size_t len);
	static int haschar(char *string, char testchar, size_t len);

	static int haschar(char *string, char testchar);
	int haschar(char testchar);

	static char *stringfield(char *string, const char delimiter, int fieldno, int *length);
	static char *stringfield(char *string, const char delimiter, char terminator, int fieldno, int *length);
	static char *stringfield(char *string, const char delimiter, int string_length, int fieldno, int *length);

	char *stringfield(const char delimiter, char terminator, int fieldno, int *length);
	char *stringfield(const char delimiter, int fieldno, int *length);



	static char *stringfield(char *string, const char delimiter, size_t fieldno, size_t *length);
	static char *stringfield(char *string, const char delimiter, char terminator, size_t fieldno, size_t *length);
	static char *stringfield(char *string, const char delimiter, size_t string_length, size_t fieldno, size_t *length);

	char *stringfield(const char delimiter, char terminator, size_t fieldno, size_t *length);
	char *stringfield(const char delimiter, size_t fieldno, size_t *length);



};


#endif


