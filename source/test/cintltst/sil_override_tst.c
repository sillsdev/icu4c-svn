/*
**********************************************************************
* Copyright (c) 2002-2009, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/uset.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include <stdlib.h>
#include <string.h>
#include "unicode/uchar.h" // required before silmods.h
#include "utrie2.h" // required before silmods.h
#include "silmods.h"
#include "unicode/uregex.h"
#include "ucase.h"
#include "unicode/unorm.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

#define TEST(x) addTest(root, &x, "SIL ext/" # x)

static void TestCharProps(void);
static void TestRegexProps(void);
static void TestIsAlphabetic(void);
static void TestCaseMapping(void);
static void TestCaseType(void);
static void TestIsSpace(void);
static void TestNormalization(void);

void addSilSetTest(TestNode** root);

static void expect(const USet* set,
                   const char* inList,
                   const char* outList,
                   UErrorCode* ec);
static void expectContainment(const USet* set,
                              const char* list,
                              UBool isIn);
static char oneUCharToChar(UChar32 c);
static void expectItems(const USet* set,
                        const char* items);

void
addSilSetTest(TestNode** root) {
    TEST(TestCharProps);
    TEST(TestRegexProps);
    TEST(TestIsAlphabetic);
    TEST(TestCaseMapping);
    TEST(TestCaseType);
    TEST(TestIsSpace);
    TEST(TestNormalization);
}

/*------------------------------------------------------------------
 * Helper functions
 *------------------------------------------------------------------*/
static int assertIntegersEqual(const char* message, const int expected,
                           const int actual) {
    if (expected != actual) {
        log_err("FAIL: %s; got \"%d\"; expected \"%d\"\n",
                message, actual, expected);
        return FALSE;

    }
    return TRUE;
}

/*------------------------------------------------------------------
 * Tests
 *------------------------------------------------------------------*/

static void TestCharProps() {
    SilIcuInit("SilPropsTestData.txt");
    if (!assertIntegersEqual("F130 char should be So", U_OTHER_SYMBOL, u_charType((UChar32)L'\uF130')))
        return;
    if (!assertIntegersEqual("F131 char should be So", U_OTHER_SYMBOL, u_charType((UChar32)L'\uF131')))
        return;
    if (!assertIntegersEqual("F136 (non-override PUA char) should be U_PRIVATE_USE_CHAR", U_PRIVATE_USE_CHAR, u_charType((UChar32)L'\uF136')))
        return;
    // Tests character in second block.
    if (!assertIntegersEqual("F170 char should be Mn", U_NON_SPACING_MARK, u_charType((UChar32)L'\uF170')))
        return;
    if (!assertIntegersEqual("F32D char should be Ll", U_LOWERCASE_LETTER, u_charType((UChar32)L'\uF32D')))
        return;
}

static void TestIsAlphabetic()
{
    SilIcuInit("SilPropsTestData.txt");
    if (!assertTrue("PUA override char of type Ll should be alphabetic", u_isUAlphabetic(L'\uF32D')))
        return;
    if (!assertTrue("PUA override char of type So should not be alphabetic", !u_isUAlphabetic(L'\uF132')))
        return;
}

static void TestIsSpace()
{
    SilIcuInit("SilPropsTestData.txt");
    if (!assertTrue("Comma should not be white space", !u_isspace(L',')))
        return;
    if (!assertTrue("Space should be white space", u_isspace(L' ')))
        return;
    if (!assertTrue("PUA override char of type Ll should not be space", !u_isspace(L'\uF32D')))
        return;
    if (!assertTrue("PUA override char of type Zs should be space", u_isspace(L'\uF304')))
        return;
}
static void TestRegexProps()
{
    UErrorCode ecode = U_ZERO_ERROR;
    URegularExpression * handle;

    // Note that L"" is implemented by wchar_t[], and sizeof(wchar_t) can be either 2 or 4.
    // sizeof(UChar) is carefully defined to be 2 regardless of system or compiler.
    UChar pattern1[7] = {'\\','p','{','L','l','}',0};   // L"\\p{Ll}"
    UChar pattern2[3] = {'\\','w',0};                   // L"\\w"
    UChar pattern3[3] = {'\\','s',0};                   // L"\\s"
    UChar pattern4[3] = {'\\','d',0};                   // L"\\d"
    UChar text_Ll[2] = {0xF32D,0};                      // L"\uF32D"
    UChar text_So[2] = {0xF132,0};                      // L"\uF132"
    UChar text_Zs[2] = {0xF304,0};                      // L"\uF304"
    UChar text_Nd[2] = {0xF305,0};                      // L"\uF305"

    SilIcuInit("SilPropsTestData.txt");

    handle = uregex_open(pattern1, -1, 0, NULL, &ecode);
    uregex_setText(handle, text_Ll, -1, &ecode);
    if (!assertTrue("\\p{Ll} should match PUA override of type Ll", uregex_matches(handle, 0, &ecode)))
        return;

    handle = uregex_open(pattern2, -1, 0, NULL, &ecode);
    uregex_setText(handle, text_Ll, -1, &ecode);
    if (!assertTrue("\\w should match PUA override of type Ll", uregex_matches(handle, 0, &ecode)))
        return;
    uregex_setText(handle, text_So, -1, &ecode);
    if (!assertTrue("\\w should not match PUA override of type So", !uregex_matches(handle, 0, &ecode)))
        return;

    handle = uregex_open(pattern3, -1, 0, NULL, &ecode);
    uregex_setText(handle, text_Zs, -1, &ecode);
    if (!assertTrue("\\s should match PUA override of type Zs", uregex_matches(handle, 0, &ecode)))
        return;
    uregex_setText(handle, text_So, -1, &ecode);
    if (!assertTrue("\\s should not match PUA override of type So", !uregex_matches(handle, 0, &ecode)))
        return;

    handle = uregex_open(pattern4, -1, 0, NULL, &ecode);
    uregex_setText(handle, text_Nd, -1, &ecode);
    if (!assertTrue("\\d should match PUA override of type Nd", uregex_matches(handle, 0, &ecode)))
        return;
    uregex_setText(handle, text_So, -1, &ecode);
    if (!assertTrue("\\d should not match PUA override of type So", !uregex_matches(handle, 0, &ecode)))
        return;
}

static void TestCaseMapping()
{
    SilIcuInit("SilPropsTestData.txt");

    if (!assertIntegersEqual("F208 should convert to lower-case 0251", L'\u0251', u_tolower(L'\uF208')))
        return;
    if (!assertIntegersEqual("F207 should not change when converted to lower case", L'\uF207', u_tolower(L'\uF207')))
        return;

    if (!assertIntegersEqual("F208 should not change when converted to upper case", L'\uF208', u_toupper(L'\uF208')))
        return;
    if (!assertIntegersEqual("F207 should not change when converted to upper case", L'\uF207', u_toupper(L'\uF207')))
        return;
    if (!assertIntegersEqual("F20E should convert to upper-case F20F", L'\uF20F', u_toupper(L'\uF20E')))
        return;

    if (!assertIntegersEqual("F208 should not change when converted to title case", L'\uF208', u_totitle(L'\uF208')))
        return;
    if (!assertIntegersEqual("F207 should not change when converted to title case", L'\uF207', u_totitle(L'\uF207')))
        return;
    if (!assertIntegersEqual("F20E should convert to title-case F20F", L'\uF20F', u_totitle(L'\uF20E')))
        return;
    // I put in a phony character just to prove that we really can get a different answer for upper and title.
    if (!assertIntegersEqual("F306 (phony) should convert to title-case F304", L'\uF304', u_totitle(L'\uF306')))
        return;
    if (!assertIntegersEqual("F306 (phony) should convert to upper-case F305", L'\uF305', u_toupper(L'\uF306')))
        return;
}

static void TestCaseType()
{
    SilIcuInit("SilPropsTestData.txt");

    if (!assertTrue("F208 should not be LC", !u_isULowercase(L'\uF208')))
        return;
    if (!assertTrue("F208 should be UC", u_isUUppercase(L'\uF208')))
        return;

    if (!assertTrue("F1F9 (non-letter) should not be LC", !u_isULowercase(L'\uF1F9')))
        return;
    if (!assertTrue("F1F9 (non-letter) should not be UC", !u_isUUppercase(L'\uF1F9')))
        return;

    if (!assertTrue("F20E should be LC", u_isULowercase(L'\uF20E')))
        return;
    if (!assertTrue("F20E should not be UC", !u_isUUppercase(L'\uF20E')))
        return;
    // And a phone title case character F307
    if (!assertTrue("F307 (TC) should not be LC", !u_isULowercase(L'\uF307')))
        return;
    if (!assertTrue("F307 should not be UC", !u_isUUppercase(L'\uF307')))
        return;
}

static void TestNormalization()
{
    UErrorCode ecode = U_ZERO_ERROR;
    // Note that L"" is implemented by wchar_t[], and sizeof(wchar_t) can be either 2 or 4.
    // sizeof(UChar) is carefully defined to be 2 regardless of system or compiler.
    UChar input[2] = {0xF1BE,0};    // L"\uF1BE"
    UChar output[50];

    SilIcuInit("SilPropsTestData.txt");
    // look in the test source (./icudt50l/nfkc.nrm) for custom normalization data for testing.
    // This file is generated using a command like
    // (You may need to "mkdir C:\icu\icu\source\test\cintltst\icudt50l" first.)
    // gennorm2 -o C:\icu\icu\source\test\cintltst\icudt50l\nfkc.nrm C:\icu\icu\source\data\unidata\norm2\nfc.txt C:\icu\icu\source\data\unidata\norm2\nfkc.txt C:\icu\icu\source\test\cintltst\nfkcOverridesTest.txt
    u_setDataDirectory(".");

    if (!assertIntegersEqual("Normalizing F1BE should yield one character", 1, unorm_normalize(input,-1, UNORM_NFKD, 0, output, 50, &ecode)))
        return;
    if (!assertIntegersEqual("Normalizing F1BE should succeed", U_ZERO_ERROR, ecode))
        return;
    if (!assertIntegersEqual("Normalizing F1BE should yield F22A", L'\uF22A', output[0]))
        return;
}

/*eof*/
