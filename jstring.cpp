#include "jstring.h"



size_t JSTRING::countfields(char *string, char delimiter, size_t length) {
	size_t i = 0, retcount = 0;
	if (!string) return 0;
	retcount = 1;
	for (i = 0; i < length; i++) {
		if (string[i] == delimiter) retcount++;
	}
	return retcount;
}

size_t JSTRING::countfields(char *string, char delimiter, char terminator) {
	size_t i = 0, argstrlen = 0, retcount = 1;
	if (!string) return 0;
	argstrlen = strlen(string);
	for (; i < argstrlen; i++) {
		if (string[i] == delimiter) retcount++;
		if (string[i] == terminator) break;
	}
	return retcount;
}

size_t JSTRING::countfields(char *string, char delimiter) {
	if (!string) return 0;
	return JSTRING::countfields(string, delimiter, strlen(string));
}

void JSTRING::freesplit(JSTRING::jsstring **splitarray) {
	size_t i = 0;
	if (!splitarray) return;
	while (splitarray[i]) {
		free(splitarray[i++]);
	}
	free(splitarray);
}


JSTRING::jsstring **JSTRING::split(char *string, char delimiter, size_t len) {
	char *instanceptr = 0;
	JSTRING::jsstring **retval = 0;
	size_t length = 0, count = 0, i = 0, j = 0;
	if (!string) return 0;
	count = JSTRING::countfields(string, delimiter, len);
	//retval = (JSTRING::jsstring **)MEMORY::calloc_(sizeof(JSTRING::jsstring *), (count + 1), (char *)"jstring.cpp > split() - 1st");
	retval = (JSTRING::jsstring **)calloc(sizeof(JSTRING::jsstring *), (count + 1));
	for (i = 1; i <= count; i++) {
		//instanceptr = JSTRING::stringfield(string, delimiter, i, &length);
		//instanceptr = JSTRING::stringfield(string, delimiter, i, &length);
		instanceptr = JSTRING::stringfield(string, delimiter, len, i, &length);
		if (length > 0) {
			//retval[j] = (JSTRING::jsstring *)MEMORY::calloc_(sizeof(JSTRING::jsstring), 1, (char *)"jstring.cpp > split() - 2nd");
			retval[j] = (JSTRING::jsstring *)calloc(sizeof(JSTRING::jsstring), 1);
			retval[j]->ptr = instanceptr;
			retval[j++]->length = length;
		}
	}
	return retval;
}

JSTRING::jsstring **JSTRING::split(char *string, char delimiter, char stringterminator) {
	char *instanceptr = 0;
	JSTRING::jsstring **retval = 0;
	size_t length = 0, count = 0, i = 0, j = 0;
	if (!string) return 0;
	count = JSTRING::countfields(string, delimiter, stringterminator);
	retval = (JSTRING::jsstring **)calloc(sizeof(JSTRING::jsstring *), (count + 1));
	for (i = 1; i <= count; i++) {
		instanceptr = JSTRING::stringfield(string, delimiter, stringterminator, i, &length);
		if (length > 0) {
			retval[j] = (JSTRING::jsstring *)calloc(sizeof(JSTRING::jsstring), 1);
			retval[j]->ptr = instanceptr;
			retval[j++]->length = length;
		}
	}
	return retval;
}

JSTRING::jsstring **JSTRING::split(char *string, char delimiter) {
	return JSTRING::split(string, delimiter, '\0');
}

JSTRING::jsstring **JSTRING::split(char delimiter, char stringterminator) {
	return JSTRING::split(this->stringptr, delimiter, stringterminator);
}

JSTRING::jsstring **JSTRING::split(char delimiter, size_t len) {
	return JSTRING::split(this->stringptr, delimiter, len);
}

char *JSTRING::makeNTS(JSTRING::jsstring *jstring) {
	char *retval = 0;
	if (!jstring) return 0;
	if (jstring->length == 0) return 0;
	retval = (char *)calloc(jstring->length + 1, 1);
	memcpy(retval, jstring->ptr, jstring->length);
	return retval;
}

char *JSTRING::makeNTS(char *start, size_t length) {
	char *retval = 0;
	if (!start) return 0;
	if (length == 0) return 0;
	retval = (char *)calloc(length + 1, 1);
	memcpy(retval, start, length);
	return retval;
}

char *JSTRING::headername(char *ptr, char string_terminator, size_t *outputlength) {
	*outputlength = 0;
	return JSTRING::trim(
		JSTRING::stringfield(ptr, ':', string_terminator, 1, outputlength),
		outputlength, outputlength);
}
char *JSTRING::headername(char *ptr, size_t *outputlength) {
	return JSTRING::headername(ptr, '\0', outputlength);
}
char *JSTRING::headername(char string_terminator, size_t *outputlength) {
	return JSTRING::headername(this->stringptr, string_terminator, outputlength);
}
char *JSTRING::headername(size_t *outputlength) {
	return JSTRING::headername(this->stringptr, '\0', outputlength);
}


char *JSTRING::keyname(char *ptr, char string_terminator, size_t *outputlength) {
	*outputlength = 0;
	return JSTRING::trim(
		JSTRING::stringfield(ptr, '=', string_terminator, 1, outputlength),
		outputlength, outputlength);
}
char *JSTRING::keyname(char *ptr, size_t *outputlength) {
	return JSTRING::keyname(ptr, '\0', outputlength);
}
char *JSTRING::keyname(char string_terminator, size_t *outputlength) {
	return JSTRING::keyname(this->stringptr, string_terminator, outputlength);
}
char *JSTRING::keyname(size_t *outputlength) {
	return JSTRING::keyname(this->stringptr, '\0', outputlength);
}



char *JSTRING::unquote(char *string, size_t inlength, size_t *outlength) {
	char *ptr = 0;
	*outlength = 0;
	ptr = JSTRING::trim(string, &inlength, outlength);
	if (ptr[0] == '"' && ptr[(*outlength) - 1] == '"') {
		ptr = &ptr[1];
		*outlength = (*outlength) - 2;
	}
	return ptr;
}

char *JSTRING::unquote(char *string, size_t *inlength, size_t *outlength) {
	size_t passlen = 0;
	if (!inlength) {
		if (outlength) *outlength = 0;
		return 0;
	}
	passlen = *inlength;
	return JSTRING::unquote(string, passlen, outlength);
}
char *JSTRING::unquote(JSTRING::jsstring *js, size_t *length) {
	return JSTRING::unquote(js->ptr, &js->length, length);
}



int JSTRING::charpos(char *string, char char_to_find, char stop_at_char, size_t length_to_search) {
	int i = 0;
	if (!string) return -1;
	if (strlen(string) < length_to_search) length_to_search = strlen(string);
	for (; i < (int)length_to_search; i++) {
		if (string[i] == stop_at_char && stop_at_char != char_to_find) return -1;
		if (string[i] == char_to_find) return i;
	}
	return -1;
}

int JSTRING::charpos(char *string, char char_to_find, char stop_at_char) {
	if (!string) return -1;
	return JSTRING::charpos(string, char_to_find, stop_at_char, strlen(string));
}

int JSTRING::charpos(char *string, char char_to_find) {
	if (!string) return -1;
	return JSTRING::charpos(string, char_to_find, '\0', strlen(string));
}

int JSTRING::charpos(char char_to_find, char stop_at_char, size_t length_to_search) {
	if (!this->stringptr || this->stringlen == 0) return -1;
	return JSTRING::charpos(this->stringptr, char_to_find, stop_at_char, length_to_search);
}
int JSTRING::charpos(char char_to_find, char stop_at_char) {
	if (!this->stringptr || this->stringlen == 0) return -1;
	return JSTRING::charpos(this->stringptr, char_to_find, stop_at_char, this->stringlen);
}

int JSTRING::charpos(char char_to_find) {
	if (!this->stringptr || this->stringlen == 0) return -1;
	return JSTRING::charpos(this->stringptr, char_to_find, '\0', this->stringlen);
}



int JSTRING::reverse_matches(char *haystack, char *needle) {
	int i = 0;
	char *phaystack = 0;
	int needlelen = 0;
	if (!haystack || !needle) return 0;
	phaystack = haystack;
	needlelen = (int)strlen(needle);
	for (i = (needlelen - 1); i >= 0; i--) {
		while (
			phaystack[0] == '\r' ||
			phaystack[0] == '\n' ||
			phaystack[0] == ' ' ||
			phaystack[0] == '\t') {
			phaystack--;
		}
		if (phaystack[0] != needle[i]) return 0;
		phaystack--;
	}
	return 1;
}



char *JSTRING::keyvalue(char *ptr, size_t *inputlength, size_t *outputlength) {
	int separator = 0;
	size_t input_length = *inputlength;
	char *valuestart = 0;
	*outputlength = 0;
	separator = JSTRING::charpos(ptr, '=', ptr[input_length]);
	if (separator == -1) return ptr;

	valuestart = &ptr[separator + 1];
	*outputlength = &ptr[input_length] - valuestart;   //JSTRING::charpos(valuestart, string_terminator, string_terminator);
	if ((*outputlength) == -1) {
		*outputlength = strlen(ptr);
	}
	return JSTRING::trim(valuestart, outputlength, outputlength);
}


char *JSTRING::keyvalue(char *ptr, char string_terminator, size_t *outputlength) {
	int separator = 0;
	char *valuestart = 0;
	*outputlength = 0;
	separator = JSTRING::charpos(ptr, '=', string_terminator);
	if (separator == -1) return ptr;
	valuestart = &ptr[separator + 1];
	*outputlength = JSTRING::charpos(valuestart, string_terminator, string_terminator);
	if ((*outputlength) == -1) {
		*outputlength = strlen(ptr);
	}
	return JSTRING::trim(valuestart, outputlength, outputlength);
}


char *JSTRING::keyvalue(char *ptr, size_t *outputlength) {
	return JSTRING::keyvalue(ptr, '\0', outputlength);
}

char *JSTRING::keyvalue(char string_terminator, size_t *outputlength) {
	return JSTRING::keyvalue(this->stringptr, string_terminator, outputlength);
}
char *JSTRING::keyvalue(size_t *outputlength) {
	return JSTRING::keyvalue(this->stringptr, '\0', outputlength);
}


char *JSTRING::headervalue(char *ptr, char string_terminator, size_t *outputlength) {
	int separator = 0, oplen = 0;
	char *valuestart = 0;
	*outputlength = 0;
	separator = JSTRING::charpos(ptr, ':', string_terminator);
	if (separator == -1) return ptr;
	valuestart = &ptr[separator + 1];
	oplen = JSTRING::charpos(valuestart, string_terminator, string_terminator);
	if (oplen == -1) {
		*outputlength = strlen(valuestart);
	} else {
		*outputlength = oplen;
	}
	return JSTRING::trim(valuestart, outputlength, outputlength);
}
char *JSTRING::headervalue(char *ptr, size_t *outputlength) {
	return JSTRING::headervalue(ptr, '\0', outputlength);
}

char *JSTRING::headervalue(char string_terminator, size_t *outputlength) {
	return JSTRING::headervalue(this->stringptr, string_terminator, outputlength);
}
char *JSTRING::headervalue(size_t *outputlength) {
	return JSTRING::headervalue(this->stringptr, '\0', outputlength);
}






int JSTRING::matches(char *str1, char *str2) {
	size_t i = 0, sl1, sl2;
	//char c1 = 0, c2 = 0;
	if (!str1 || !str2) return 0;
	sl1 = strlen(str1);
	sl2 = strlen(str2);
	if (sl1 == 0 || sl2 == 0) {
		if (sl1 > 0 || sl2 > 0) return 0;
	}
	for (; i < ((sl1 > sl2) ? sl2 : sl1); i++) {
		//c1 = LOWER(str1[i]);
		//c2 = LOWER(str2[i]);
		if (LOWER(str1[i]) != LOWER(str2[i])) return 0;
	}
	return 1;
}

char *JSTRING::trim(char *string, size_t stringlength, size_t *outputlength) {
	//static char mt[] = "";
	size_t i = 0;
	char *retval = 0;
	if (!string || !outputlength) return string;
	retval = string;
	for (i = 0; i < stringlength; i++) {
		if (retval == string) {
			if (string[i] != ' ' &&
				string[i] != '\t' &&
				string[i] != '\r' &&
				string[i] != '\n') {
				retval = &string[i];
				break;
			}
		}
	}
	// Here, "retval" is the first non-whitespace character.
	if ( /* It was ALL whitespace */ i == stringlength) {
		*outputlength = 0;
		return retval;
	}
	*outputlength = stringlength - (retval - string);
	for (i = (stringlength - 1); &string[i] > retval; i--) {
		if (string[i] != ' ' &&
			string[i] != '\t' &&
			string[i] != '\r' &&
			string[i] != '\n') {
			*outputlength -= (stringlength - (i + 1));
			break;
		}
	}
	return retval;
}
char *JSTRING::trim(char *string, size_t *stringlength, size_t *outputlength) {
	size_t length = *stringlength;
	return JSTRING::trim(string, length, outputlength);
}
char *JSTRING::trim(char *string, size_t *outputlength) {
	return JSTRING::trim(string, strlen(string), outputlength);
}
char *JSTRING::trim(size_t *outputlength) {
	return JSTRING::trim(this->stringptr, outputlength);
}


char *JSTRING::between(char *string, const char *frame, size_t *length) {
	size_t i = 0;
	char *retval = 0;
	char csframe[2];
	if (length) (*length) = 0;
	if (!string || !frame) return 0;
	retval = string;
	csframe[0] = frame[0];
	csframe[1] = ((frame[1] == 0) ? frame[0] : frame[1]);

	for (i = 0; i < strlen(string); i++) {
		if (/* We haven't found the first frame char */retval == string) {
			if ( /* First frame char */ string[i] == csframe[0]) {
				retval = &string[(i + 1)];
			}
		} else /* We're looking for the second frame char, or the end of the string */ {
			if ( /* The second frame char has been found */ string[i] == csframe[1]) {
				break;
			}
		}
	}
	if (length) (*length) = (size_t)(&string[i] - retval);
	return retval;
}

char *JSTRING::between(const char *frame, size_t *length) {
	return JSTRING::between(this->stringptr, frame, length);
}



int JSTRING::haschar(char *string, char testchar, size_t len) {
	size_t i = 0;
	for (i = 0; i < len; i++) {
		if (string[i] == testchar) return 1;
	}
	return 0;
}
int JSTRING::haschar(char testchar, size_t len) {
	return JSTRING::haschar(this->stringptr, testchar, len);
}
int JSTRING::haschar(char *string, char testchar) {
	return JSTRING::haschar(string, testchar, strlen(string));
}
int JSTRING::haschar(char testchar) {
	return JSTRING::haschar(this->stringptr, testchar);
}


char *JSTRING::stringfield(char *string, const char delimiter, char terminator, int fieldno, int *length) {
	char *retval = string;
	size_t i = 0;
	if (!string) return 0;
	if (fieldno == 0) return 0;
	if (length) *length = 0;
	for (i = 0; i < strlen(string); i++) {
		if (string[i] == terminator) break;
		if (string[i] == delimiter) {
			if (--fieldno == 0) {
				if (length) *length = (int)(&string[i] - retval);
				return retval;
			}
			retval = &string[i + 1];
		}
	}
	if (fieldno == 1) {
		if (length) *length = (int)(&string[i] - retval);
		return retval;
	}
	return 0;
}

char *JSTRING::stringfield(char *string, const char delimiter, int fieldno, int *length) {
	return JSTRING::stringfield(string, delimiter, '\0', fieldno, length);
}

char *JSTRING::stringfield(const char delimiter, char terminator, int fieldno, int *length) {
	return JSTRING::stringfield(this->stringptr, terminator, delimiter, fieldno, length);
}

char *JSTRING::stringfield(const char delimiter, int fieldno, int *length) {
	return JSTRING::stringfield(this->stringptr, '\0', delimiter, fieldno, length);
}

char *JSTRING::stringfield(char *string, const char delimiter, int string_length, int fieldno, int *length) {
	char *retval = string;
	int i = 0;
	if (length) *length = 0;
	if (!string) return 0;
	if (fieldno == 0) return 0;
	for (i = 0; i < string_length; i++) {
		if (string[i] == delimiter) {
			if (--fieldno == 0) {
				if (length) *length = (int)(&string[i] - retval);
				return retval;
			}
			retval = &string[i + 1];
		}
	}
	if (fieldno == 1) {
		if (length) *length = (int)(&string[i] - retval);
		return retval;
	}
	return 0;
}


char *JSTRING::stringfield(char *string, const char delimiter, char terminator, size_t fieldno, size_t *length) {
	char *retval = string;
	size_t i = 0;
	if (length) *length = 0;
	if (!string) return 0;
	if (fieldno == 0) return 0;
	for (i = 0; i < strlen(string); i++) {
		if (string[i] == terminator) break;
		if (string[i] == delimiter) {
			if (--fieldno == 0) {
				if (length) *length = &string[i] - retval;
				return retval;
			}
			retval = &string[i + 1];
		}
	}
	if (fieldno == 1) {
		if (length) *length = &string[i] - retval;
		return retval;
	}
	return 0;
}


char *JSTRING::stringfield(char *string, const char delimiter, size_t string_length, size_t fieldno, size_t *length) {
	char *retval = string;
	size_t i = 0;
	if (length) *length = 0;
	if (!string) return 0;
	if (fieldno == 0) return 0;
	for (i = 0; i < string_length; i++) {
		if (string[i] == delimiter) {
			if (--fieldno == 0) {
				if (length) *length = &string[i] - retval;
				return retval;
			}
			retval = &string[i + 1];
		}
	}
	if (fieldno == 1) {
		if (length) *length = &string[i] - retval;
		return retval;
	}
	return 0;
}


char *JSTRING::stringfield(char *string, const char delimiter, size_t fieldno, size_t *length) {
	return JSTRING::stringfield(string, delimiter, '\0', fieldno, length);
}


char *JSTRING::stringfield(const char delimiter, char terminator, size_t fieldno, size_t *length) {
	return JSTRING::stringfield(this->stringptr, terminator, delimiter, fieldno, length);
}


char *JSTRING::stringfield(const char delimiter, size_t fieldno, size_t *length) {
	return JSTRING::stringfield(this->stringptr, '\0', delimiter, fieldno, length);
}




/* Static linecount, for use outside of any specific object */
int JSTRING::countlines(char *string, size_t length) {
	int lc = 0, i = 0;
	char *lineending = 0;
	if (!string || length == 0) return 0;
	lineending = string;
	for (; &lineending[i] < &string[length]; i++) {
		if (lineending[i] == '\n') {
			lc++;
		}
	}
	return lc;
}

int JSTRING::countlines(char *string) {
	return JSTRING::countlines(string, strlen(string));
}



/* Static line-fetching function */
char *JSTRING::line(char *string, int linenumber, size_t *length) {
	if (!length) return 0;
	*length = 0;
	if (linenumber == 0) {
		return 0;
	}
	if (linenumber > JSTRING::countlines(string)) {
		return 0;
	}
	return JSTRING::trim(
		JSTRING::stringfield(string, '\n', linenumber, length),
		length,
		length);
}



char *JSTRING::line(int linenumber, size_t *length) {
	return JSTRING::line(this->stringptr, linenumber, length);
}



char *JSTRING::string() {
	return this->stringptr;
}
void JSTRING::string(char *string) {
	this->stringptr = (char *)string;
	this->stringlen = strlen(string);
	this->linecount = JSTRING::countlines(this->stringptr);
	return;
}






/*
JSTRING::~justin_string_parse() {
return;
}
*/
