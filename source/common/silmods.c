#include "unicode/uchar.h"
#include "utrie2.h"
#include "silmods.h"
#ifndef _MSC_VER
#include <errno.h>
#endif
#include <stdio.h>
#include <malloc.h>
#include <string.h> // for strncmp
#include <stdlib.h> // for atoi
#include "uprops.h" // for UPROPS_ALPHABETIC and similar...may be able to remove if we implement all properties.
#include "ucase.h" // for constants used in SIL_ucase_getType

// The data we keep about one character.
typedef struct
{
	uint32_t props;  // corresponds to the value stored in propsTrie. The low bits store the character category, high ones store numeric value (not implemented).
	// correspond to the three values stored in propsVectors at the index looked up in propsVectorTrie.
	// See "Properties in vector word 0" and similar comments in source/common/uprops.h.
	uint32_t words[3];
	// case equivalents; 0 if none.
	UChar32 upper;
	UChar32 lower;
	UChar32 title;
	// Todo:  a pointer to the character name?
} cdata;

// The data for a block of characters.
typedef struct block
{
	UChar32 first; // first character in the block
	int count; // number of characters
	cdata** propData; // data about the characters in the block; items may be null if not overridden.
	struct block * nextBlock;
} block;

static block * firstBlock;

/* Convert one hex digit to a numeric value 0..F, or -1 on failure */
static int8_t _digit16(char c) {
    if (c >= 0x0030 && c <= 0x0039) { // 0-9
        return (int8_t)(c - 0x0030);
    }
    if (c >= 0x0041 && c <= 0x0046) { // A-F
        return (int8_t)(c - (0x0041 - 10));
    }
    if (c >= 0x0061 && c <= 0x0066) { // a-f
        return (int8_t)(c - (0x0061 - 10));
    }
    return -1;
}

// Return the leading characters of the line as a unicode value
UChar32 ParseHexChar(char * line)
{
	int result = 0;
	char * pch = line;
	int val;
	while ((val = _digit16(*pch++)) >= 0)
		result = (result << 4) + val;
	return result;
}

char * nextItem(char * start)
{
	char *pch = start;
	for (; *pch; pch++)
	{
		if (*pch == ';')
			return pch + 1;
	}
	return pch;
}

int GetCategory(char * pch)
{
	switch (*pch)
	{
	case 'L':
		switch (*(pch+1))
		{
		case 'u': return U_UPPERCASE_LETTER;
		case 'l': return U_LOWERCASE_LETTER;
		case 't': return U_TITLECASE_LETTER;
		case 'm': return U_MODIFIER_LETTER;
		case 'o': return U_OTHER_LETTER;
		default: return -1;
		}
	case 'M':
		switch (*(pch+1))
		{
		case 'n': return U_NON_SPACING_MARK;
		case 'e': return U_ENCLOSING_MARK;
		case 'c': return U_COMBINING_SPACING_MARK;
		default: return -1;
		}
	case 'N':
		switch (*(pch+1))
		{
		case 'd': return U_DECIMAL_DIGIT_NUMBER;
		case 'l': return U_LETTER_NUMBER;
		case 'o': return U_OTHER_NUMBER;
		default: return -1;
		}
	case 'Z':
		switch (*(pch+1))
		{
		case 's': return U_SPACE_SEPARATOR;
		case 'l': return U_LINE_SEPARATOR;
		case 'p': return U_PARAGRAPH_SEPARATOR;
		default: return -1;
		}
	case 'C':
		switch (*(pch+1))
		{
		case 'n': return U_GENERAL_OTHER_TYPES;
		case 'c': return U_CONTROL_CHAR;
		case 'f': return U_FORMAT_CHAR;
		case 'o': return U_PRIVATE_USE_CHAR;
		case 's': return U_SURROGATE;
		default: return -1;
		}
	case 'P':
		switch (*(pch+1))
		{
		case 'd': return U_DASH_PUNCTUATION;
		case 's': return U_START_PUNCTUATION;
		case 'e': return U_END_PUNCTUATION;
		case 'c': return U_CONNECTOR_PUNCTUATION;
		case 'o': return U_OTHER_PUNCTUATION;
		case 'i': return U_INITIAL_PUNCTUATION;
		case 'f': return U_FINAL_PUNCTUATION;
		default: return -1;
		}
	case 'S':
		switch (*(pch+1))
		{
		case 'm': return U_MATH_SYMBOL;
		case 'c': return U_CURRENCY_SYMBOL;
		case 'k': return U_MODIFIER_SYMBOL;
		case 'o': return U_OTHER_SYMBOL;
		default: return -1;
		}
	default:
		return -1;
	}
}

int MakePropsWord(char * pch)
{
	// Enhance JohnT: the high 10 bits of the props word are supposed to be the numeric value;
	// I think we would get the right result by passing in pchNumericValue and
	// returning GetCategory(pch) | (atoi(pchNumericValue) << 10).
	// I haven't done this or written tests for it because I don't think FW uses the numeric value for anything.
	// In essence, the numeric value for any character we override is zero.
	return GetCategory(pch); // Todo: error handling; high bits (numeric value);
}

void FreeBlocks()
{
	block * currentBlock = firstBlock;
	block * nextBlock;
	cdata ** pdata;
	while (currentBlock != NULL)
	{
		nextBlock = currentBlock->nextBlock;
		if (firstBlock->propData != NULL)
		{
			for (pdata = currentBlock->propData; pdata < currentBlock->propData + currentBlock->count; pdata++)
			{
				if (*pdata != NULL)
					free(*pdata);
			}
			free(currentBlock->propData);
		}
		free(currentBlock);
		currentBlock = nextBlock;
	}
	firstBlock = NULL;
}

block * InitBlock(int charCount, UChar32 firstChar)
{
	cdata ** pdata;
	block * newBlock = (block *)malloc(sizeof(block));
	newBlock->count = charCount;
	newBlock->first = firstChar;
	newBlock->nextBlock = NULL;
	newBlock->propData = (cdata **)malloc(charCount * sizeof(cdata *));
	for (pdata = newBlock->propData; pdata < newBlock->propData + charCount; pdata++)
		*pdata = NULL;
	return newBlock;
}

void ProcessLine(char * line, cdata * newData)
{
	uint32_t overrideProps;
	char * pchName = nextItem(line);
	char * pchCategory = nextItem(pchName);
	char * pchCombiningClass = nextItem(pchCategory);
	char * pchBidiCategory = nextItem(pchCombiningClass);
	char * pchDecomposition = nextItem(pchBidiCategory);
	char * pchDecimalDigitValue = nextItem(pchDecomposition);
	char * pchDigitValue = nextItem(pchDecimalDigitValue);
	char * pchNumericValue = nextItem(pchDigitValue);
	char * pchMirrored = nextItem(pchNumericValue);
	char * pchUnicode1Name = nextItem(pchMirrored);
	char * pchComment = nextItem(pchUnicode1Name);
	char * pchUcEquivalent = nextItem(pchComment);
	char * pchLcEquivalent = nextItem(pchUcEquivalent);
	char * pchTcEquivalent= nextItem(pchLcEquivalent);

	int word1 = 0;
	int category = GetCategory(pchCategory);
	// This is the definition of 'alphabetic' given in http://pic.dhe.ibm.com/infocenter/tivihelp/v15r1/index.jsp?topic=%2Fcom.ibm.itm.doc_6.2.2fp2%2Ficu-regular-expressions.htm
	// as the definition of \w in a regular expression. It's only a guess that this is what the flag is supposed to be.
	if (category == U_LOWERCASE_LETTER || category == U_UPPERCASE_LETTER || category == U_TITLECASE_LETTER || category == U_OTHER_LETTER || category == U_DECIMAL_DIGIT_NUMBER)
		word1 |= U_MASK(UPROPS_ALPHABETIC);
	// This is the definition of white space given at the same URL for \s in regular expressions.
	if (category == U_SPACE_SEPARATOR || category == U_LINE_SEPARATOR || category ==  U_PARAGRAPH_SEPARATOR)
		word1 |= U_MASK(UPROPS_WHITE_SPACE);
	overrideProps = MakePropsWord(pchCategory);
	newData->props = overrideProps;

	newData->words[0] = newData->words[2] = 0;
	newData->words[1] = word1;

	newData->lower = ParseHexChar(pchLcEquivalent);
	newData->upper = ParseHexChar(pchUcEquivalent);
	newData->title = ParseHexChar(pchTcEquivalent);}

#define DEFAULTBLOCKSIZE 256
#define MAXBLOCKSIZE 4096
#define MAXLINE 2000
// Initialize our private data structures with the specified data.
U_CAPI UBool U_EXPORT2 SilIcuInit(const char * dataPath)
{
	char line[MAXLINE];
	FILE * reader = fopen(dataPath, "r");
	block * currentBlock = NULL;
	block * lastBlock;
	int index;
	cdata * newData;
	int blockSize = DEFAULTBLOCKSIZE;
	UChar32 c;

	FreeBlocks();

	if (reader == NULL)
	{
#ifdef _MSC_VER
		int error;
		_get_errno(&error);
		//int doserror;
		//_get_doserrno(&doserror);
#endif
		return FALSE;
	}

	fgets(line, MAXLINE, reader); // skip one line, typically labels.
	while (fgets(line, MAXLINE, reader))
	{
		if (strncmp(line, "block:", 6) == 0)
		{
			blockSize = atoi(line + 6);
			if (blockSize <= 0 || blockSize >= MAXBLOCKSIZE)
				blockSize = DEFAULTBLOCKSIZE;
			continue;
		}
		c = ParseHexChar(line); // Todo: error handling...
		if (firstBlock == NULL)
		{
			firstBlock = currentBlock = InitBlock(blockSize, c);
		}
		index = c- currentBlock->first;
		if (index >= currentBlock->count)
		{
			lastBlock = currentBlock;
			currentBlock = InitBlock(blockSize, c);
			lastBlock->nextBlock = currentBlock;
			index = c- currentBlock->first;
		}
		currentBlock->propData[index] = newData = (cdata*)malloc(sizeof(cdata));
		ProcessLine(line, newData);
	}
	fclose(reader);
	return TRUE;
}

cdata * GetOverrides(UChar32 c)
{
	block * currentBlock;
	int index;
	for (currentBlock = firstBlock; currentBlock != NULL; currentBlock = currentBlock->nextBlock)
	{
		if (c < currentBlock->first)
			return NULL;
		if (c >= currentBlock->first + currentBlock->count)
			continue;
		index = c - currentBlock->first;
		// If it is in this block but has no cdata, it definitely has no override; go to the default behavior.
		if (currentBlock->propData[index] == NULL)
			return NULL;
		return currentBlock->propData[index];
	}
	return NULL;
}

uint32_t Sil_GET_PROPS(const UTrie2 * trie, UChar32 c)
{
	cdata * overrideData = GetOverrides(c);
	if (overrideData != NULL)
		return overrideData->props;

	return UTRIE2_GET16(trie, c);
}

typedef struct enumRangeParams
{
	UTrie2EnumValue *enumValue;
	UTrie2EnumRange *enumRange;
	const void *context;
	const uint32_t * propsVectors; // if non-null, indicates usage by SIL_enumPropsVecRanges. The array in which we look up word1,2,3
} enumRangeParams;



typedef struct enumRangeState
{
	enumRangeParams * params;
	int startRemaining; // must output chars from startRemaining to end using the original or overridden value.
	int end;
	int doubtful; // character we have not yet determined output for.
	int currentValue; // if doubtful > startRemaining, this value must be output for chars from startRemaining to doubtful-1
	block * currentBlock;
	// used only from SIL_enumPropsVecRanges
	int word0, word1, word2;  // indicate the values for the three property words for the chars from startRemaining to doubtful-1

} enumRangeState;

UBool EnumFinished(enumRangeState * state)
{
	// We are finished if we have output all the characters.
	return state->startRemaining > state->end;
}

UBool EnumNoMoreRelevantBlocks(enumRangeState * state)
{
	// There are no more relevant blocks if there are none at all, or all the remain affect characters after the end.
	return state->currentBlock == NULL || state->currentBlock->first > state->end;
}

// If there is pending output (chars between startRemaining and doubtful), output them, and adjust the state.
UBool EnumOutputPending(enumRangeState * state)
{
	if (state->doubtful <= state->startRemaining)
		return TRUE; // nothing to output.
	if (!state->params->enumRange(state->params->context, state->startRemaining, state->doubtful - 1, state->currentValue))
		return FALSE;
	state->startRemaining = state->doubtful;
	return TRUE;
}

// The current block is not relevant if we have determined the correct value to output for all the
// characters it contains data about. The last of these is block->first + block->count-1.
// This should be called only after determining that there is a currentBlock.
UBool EnumCurrentBlockIsNotRelevant(enumRangeState * state)
{
	return state->doubtful >= state->currentBlock->first + state->currentBlock->count;
}

// So far the only bits we control (in Word1) are the alphabetic one.
#define OURBITS (U_MASK(UPROPS_ALPHABETIC) | U_MASK(UPROPS_WHITE_SPACE))

uint32_t GetOverrideForWord1(uint32_t defaultVal, cdata * overrideData)
{
	return (defaultVal & !OURBITS) | (overrideData->words[1] & OURBITS);
}

void EnumNextValue(enumRangeState * state, uint32_t defaultValue, uint32_t * values)
{
	int index;
	cdata * data;
	// Set default result;
	values[0] = defaultValue;
	if (state->params->propsVectors)
	{
		values[1] = state->params->propsVectors[defaultValue];
		values[2] = state->params->propsVectors[defaultValue + 1];
		values[3] = state->params->propsVectors[defaultValue + 2];
	}
	else
		values[1]=values[2]=values[3]=0;

	// determine the value for the next doubtful character.
	// it must be before the end of currentBlock.
	if (state->doubtful < state->currentBlock->first)
	{
		// before the range this block overrides.
		return;
	}
	index = state->doubtful - state->currentBlock->first;
	data = state->currentBlock->propData[index];
	if (data == NULL)
	{
		// we don't have an override for this particular one.
		return;
	}
	// We have an override. Currently only word1 is affected.
	if (state->params->propsVectors)
	{
		values[2] = GetOverrideForWord1(values[2], data);
	}
	else
	{
		if (state->params->enumValue == NULL)
			values[0] = data->props; // no converter, use the value itself.
		else
			values[0] = state->params->enumValue(state->params->context, data->props);
	}
}

// Return true if there are values we need to output between startRemaining and doubtful, and the values in
// newValue are different.
UBool EnumDifferentValues(enumRangeState * state, uint32_t * newValues)
{
	if (state->startRemaining >= state->doubtful)
		return FALSE; // nothing to output
	if(state->currentValue != newValues[0])
		return TRUE;
	if (!state->params->propsVectors)
		return FALSE; // other fields don't matter
	return state->word0 != newValues[1] || state->word1 != newValues[2] || state->word2 != newValues[3];
}


// Call the original enumRange for contiguous ranges within the start...end range
// as modified by our tables.
static UBool U_CALLCONV
SIL_UTrie2EnumRange(const void *params1, UChar32 start, UChar32 end, uint32_t value)
{
	uint32_t nextValue[4];

	enumRangeParams * params = (enumRangeParams *) params1;
	enumRangeState state;
	state.params = params;
	state.startRemaining = state.doubtful = start;
	state.end = end;
	state.currentBlock = firstBlock;
	while (!EnumFinished(&state))
	{
		if (EnumNoMoreRelevantBlocks(&state))
		{
			if (state.currentValue != value)
			{
				if (!EnumOutputPending(&state)) // may do nothing, if no range.
					return FALSE; // enumeration cancelled.
			}
			if (state.startRemaining <= end)
				params->enumRange(params->context, state.startRemaining, end, value);
			return TRUE;
		}
		if (EnumCurrentBlockIsNotRelevant(&state))
		{
			state.currentBlock = state.currentBlock->nextBlock;
			continue; // to check we still have a relevant current block.
		}

		// At this point we have a relevant currentBlock, that is, one that may override some value in the range.
		EnumNextValue(&state, value, nextValue);
		if (EnumDifferentValues(&state, nextValue)) // currentValue may not be meaningful, but if so, output pending does nothing so it doesn't matter.
		{
			if (!EnumOutputPending(&state))
				return FALSE;
		}
		state.currentValue = nextValue[0];
		state.word0 = nextValue[1];
		state.word1 = nextValue[2];
		state.word2 = nextValue[3];
		state.doubtful++; // one (or one more) character should eventually be output with the current value.
	}
	return TRUE;
}

void SIL_utrie2_enum(const UTrie2 *trie, UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange, const void *context)
{
	enumRangeParams params;
	params.context = context;
	params.enumRange = enumRange;
	params.enumValue = enumValue;
	params.propsVectors = NULL;
	utrie2_enum(trie, enumValue, SIL_UTrie2EnumRange, &params);
}

uint32_t SIL_getUnicodeProperties(uint32_t defaultVal, UChar32 c, int32_t column)
{
	cdata * overrideData = GetOverrides(c);
	if (overrideData != NULL)
	{
		// This is what we'd ideally do if we had implemented all the fields.
		//return overrideData->words[column];
		switch(column)
		{
		case 0:
		case 2:
			return defaultVal; // We have not yet implemented initializing these.
		case 1:
			return GetOverrideForWord1(defaultVal, overrideData);
		}
	}
	return defaultVal;
}

// We want to invoke the function enumRange for each range of characters for which we get a different
// set of the three words stored (usually) in propsVectors and indexed by the value in propsVectorsTrie,
// and which are overridden in the words array of our cdata.
// Theoretically we should pass a different value of the index looked up in the trie each time, but in fact,
// the method that is invoked by this only uses the start-of-range value.
void SIL_enumPropsVecRanges(const UTrie2 *trie, UTrie2EnumRange *enumRange, const void *context, const uint32_t * propsVectors)
{
	enumRangeParams params;
	params.context = context;
	params.enumRange = enumRange;
	params.enumValue = NULL;
	params.propsVectors = propsVectors;
	utrie2_enum(trie, NULL, SIL_UTrie2EnumRange, &params);
}

U_CAPI UBool U_EXPORT2 SIL_tolower(UChar32 * pch)
{
	cdata * overrideData = GetOverrides(*pch);
	if (overrideData == NULL || overrideData->lower == 0)
		return FALSE;
	*pch = overrideData->lower;
	return TRUE;
}

U_CAPI UBool U_EXPORT2 SIL_toupper(UChar32 * pch)
{
	cdata * overrideData = GetOverrides(*pch);
	if (overrideData == NULL || overrideData->upper == 0)
		return FALSE;
	*pch = overrideData->upper;
	return TRUE;
}

U_CAPI UBool U_EXPORT2 SIL_totitle(UChar32 * pch)
{
	cdata * overrideData = GetOverrides(*pch);
	if (overrideData == NULL || overrideData->title == 0)
		return FALSE;
	*pch = overrideData->title;
	return TRUE;
}

U_CAPI int32_t U_EXPORT2 SIL_ucase_getType(UChar32 c)
{
	cdata * overrideData = GetOverrides(c);
	if (overrideData == NULL)
		return -1;
	switch(GET_CATEGORY(overrideData->props))
	{
	case U_UPPERCASE_LETTER: return UCASE_UPPER;
	case U_LOWERCASE_LETTER: return UCASE_LOWER;
	case U_TITLECASE_LETTER: return UCASE_TITLE;
	default: return UCASE_NONE;
	}
}
