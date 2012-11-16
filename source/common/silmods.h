#ifndef SILMODS_H
#define SILMODS_H

// All our modifications (even in this file) should be controlled by this #ifdef.
#define SIL_MODIFICATIONS

#ifdef SIL_MODIFICATIONS

// Initialize the SIL overrides. Return TRUE if successful, otherwise FALSE.
U_CAPI UBool U_EXPORT2 SilIcuInit(const char * dataPath);

// Override function replaces GET_PROPS in uchar.c
uint32_t Sil_GET_PROPS(const UTrie2 * trie, UChar32 c);

// Function is called for start of each range of characters with different properties.
// As far as I can tell, it isn't critical that they be called in order, nor is it disastrous to call
// it for more code points than necessary.
// Essentially we want to call enumRange(context, start, end, enumValue(context, propsWord)) for each range of code points where the enumValue
// gives a different result than for the previous code point.
void InvokeForStartOfEachPropRange(UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange, const void *context);

// Override of utrie2_enum, called only when the trie is propsTrie, the one our character properties override.
void SIL_utrie2_enum(const UTrie2 *trie, UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange, const void *context);

// Override of u_getUnicodeProperties.
uint32_t SIL_getUnicodeProperties(uint32_t defaultVal, UChar32 c, int32_t column);

// Used to patch
void SIL_enumPropsVecRanges(const UTrie2 *trie, UTrie2EnumRange *enumRange, const void *context, const uint32_t * propsVectors);

// Used to patch case functions
U_CAPI UBool U_EXPORT2 SIL_tolower(UChar32 * pch);
U_CAPI UBool U_EXPORT2 SIL_toupper(UChar32 * pch);
U_CAPI UBool U_EXPORT2 SIL_totitle(UChar32 * pch);
U_CAPI int32_t U_EXPORT2 SIL_ucase_getType(UChar32 c);
#endif
#endif
