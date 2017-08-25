// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/**
 *******************************************************************************
 * Copyright (C) 2006-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "brkeng.h"
#include "dictbe.h"
#include "unicode/uniset.h"
#include "unicode/chariter.h"
#include "unicode/ubrk.h"
#include "uvectr32.h"
#include "uvector.h"
#include "uassert.h"
#include "unicode/normlzr.h"
#include "cmemory.h"
#include "dictionarydata.h"

U_NAMESPACE_BEGIN

/*
 ******************************************************************
 */

DictionaryBreakEngine::DictionaryBreakEngine(uint32_t breakTypes) :
    clusterLimit(3)
{
    UErrorCode status = U_ZERO_ERROR;
    fTypes = breakTypes;
    fViramaSet.applyPattern(UNICODE_STRING_SIMPLE("[[:ccc=VR:]]"), status);

    // note Skip Sets contain fIgnoreSet characters too.
    fSkipStartSet.applyPattern(UNICODE_STRING_SIMPLE("[[:lb=OP:][:lb=QU:]\\u200C\\u200D\\u2060]"), status);
    fSkipEndSet.applyPattern(UNICODE_STRING_SIMPLE("[[:lb=CP:][:lb=QU:][:lb=EX:][:lb=CL:]\\u200C\\u200D\\u2060]"), status);
    fNBeforeSet.applyPattern(UNICODE_STRING_SIMPLE("[[:lb=CR:][:lb=LF:][:lb=NL:][:lb=SP:][:lb=ZW:][:lb=IS:][:lb=BA:][:lb=NS:]]"), status);
}

DictionaryBreakEngine::~DictionaryBreakEngine() {
}

UBool
DictionaryBreakEngine::handles(UChar32 c, int32_t breakType) const {
    return (breakType >= 0 && breakType < 32 && (((uint32_t)1 << breakType) & fTypes)
            && fSet.contains(c));
}

int32_t
DictionaryBreakEngine::findBreaks( UText *text,
                                 int32_t startPos,
                                 int32_t endPos,
                                 UBool reverse,
                                 int32_t breakType,
                                 UStack &foundBreaks ) const {
    int32_t result = 0;

    // Find the span of characters included in the set.
    //   The span to break begins at the current position in the text, and
    //   extends towards the start or end of the text, depending on 'reverse'.

    int32_t start = (int32_t)utext_getNativeIndex(text);
    int32_t current;
    int32_t rangeStart;
    int32_t rangeEnd;
    UChar32 c = utext_current32(text);
    if (reverse) {
        UBool   isDict = fSet.contains(c);
        while((current = (int32_t)utext_getNativeIndex(text)) > startPos && isDict) {
            c = utext_previous32(text);
            isDict = fSet.contains(c);
        }
        if (current < startPos) {
            rangeStart = startPos;
        } else {
            rangeStart = current;
            if (!isDict) {
                utext_next32(text);
                rangeStart = (int32_t)utext_getNativeIndex(text);
            }
        }
        // rangeEnd = start + 1;
        utext_setNativeIndex(text, start);
        utext_next32(text);
        rangeEnd = (int32_t)utext_getNativeIndex(text);
    }
    else {
        while((current = (int32_t)utext_getNativeIndex(text)) < endPos && fSet.contains(c)) {
            utext_next32(text);         // TODO:  recast loop for postincrement
            c = utext_current32(text);
        }
        rangeStart = start;
        rangeEnd = current;
    }
    if (breakType >= 0 && breakType < 32 && (((uint32_t)1 << breakType) & fTypes)) {
        result = divideUpDictionaryRange(text, rangeStart, rangeEnd, foundBreaks);
        utext_setNativeIndex(text, current);
    }

    return result;
}

void
DictionaryBreakEngine::setCharacters( const UnicodeSet &set ) {
    fSet = set;
    // Compact for caching
    fSet.compact();
}

bool
DictionaryBreakEngine::scanBeforeStart(UText *text, int32_t& start, bool &doBreak) const {
    UErrorCode status = U_ZERO_ERROR;
    UText* ut = utext_clone(NULL, text, false, true, &status);
    utext_setNativeIndex(ut, start);
    UChar32 c = utext_current32(ut);
    bool res = false;
    doBreak = true;
    while (start >= 0) {
        if (!fSkipStartSet.contains(c)) {
            res = (c == ZWSP);
            break;
        }
        --start;
        c = utext_previous32(ut);
        doBreak = false;
    }
    utext_close(ut);
    return res;
}

bool
DictionaryBreakEngine::scanAfterEnd(UText *text, int32_t textEnd, int32_t& end, bool &doBreak) const {
    UErrorCode status = U_ZERO_ERROR;
    UText* ut = utext_clone(NULL, text, false, true, &status);
    utext_setNativeIndex(ut, end);
    UChar32 c = utext_current32(ut);
    bool res = false;
    doBreak = !fNBeforeSet.contains(c);
    while (end < textEnd) {
        if (!fSkipEndSet.contains(c)) {
            res = (c == ZWSP);
            break;
        }
        ++end;
        c = utext_next32(ut);
        doBreak = false;
    }
    utext_close(ut);
    return res;
}

void
DictionaryBreakEngine::scanBackClusters(UText *text, int32_t textStart, int32_t& start) const {
    UChar32 c = 0;
    start = utext_getNativeIndex(text);
    while (start > textStart) {
        c = utext_previous32(text);
        --start;
        if (!fSkipEndSet.contains(c))
            break;
    }
    for (int i = 0; i < clusterLimit; ++i) { // scan backwards clusterLimit clusters
        while (start > textStart) {
            while (fIgnoreSet.contains(c))
                c = utext_previous32(text);
            if (!fMarkSet.contains(c)) {
                if (fBaseSet.contains(c)) {
                    c = utext_previous32(text);
                    if (!fViramaSet.contains(c)) { // Virama (e.g. coeng) preceding base. Treat sequence as a mark
                        utext_next32(text);
                        c = utext_current32(text);
                        break;
                    } else {
                        --start;
                    }
                } else {
                    break;
                }
            }
            c = utext_previous32(text);
            --start;
        }
        if (!fBaseSet.contains(c) || start < textStart) {  // not a cluster start so finish
            break;
        }
        c = utext_previous32(text);
        --start;        // go round again
    }                   // ignore hitting previous inhibitor since scanning for it should have found us!
    ++start;            // counteract --before
}

void
DictionaryBreakEngine::scanFwdClusters(UText *text, int32_t textEnd, int32_t& end) const {
    UChar32 c = utext_current32(text);
    end = utext_getNativeIndex(text);
    while (end < textEnd) {
        if (!fSkipStartSet.contains(c))
            break;
        utext_next32(text);
        c = utext_current32(text);
        ++end;
    }
    for (int i = 0; i < clusterLimit; ++i) { // scan forwards clusterLimit clusters
        while (fIgnoreSet.contains(c)) {
            utext_next32(text);
            c = utext_current32(text);
        }
        if (fBaseSet.contains(c)) {
            while (end < textEnd) {
                utext_next32(text);
                c = utext_current32(text);
                ++end;
                if (!fMarkSet.contains(c))
                    break;
                else if (fViramaSet.contains(c)) {  // handle coeng + base as mark
                    utext_next32(text);
                    c = utext_current32(text);
                    ++end;
                    if (!fBaseSet.contains(c))
                        break;
                }
            }
        } else {
            --end;    // bad char so break after char before it
            break;
        }
    }
}

bool
DictionaryBreakEngine::scanWJ(UText *text, int32_t &start, int32_t end, int32_t &before, int32_t &after) const {
    UErrorCode status = U_ZERO_ERROR;
    UText* ut = utext_clone(NULL, text, false, true, &status);
    int32_t nat = start;
    utext_setNativeIndex(ut, nat);
    bool foundFirst = true;
    int32_t curr = start;
    while (nat < end) {
        UChar32 c = utext_current32(ut);
        if (c == ZWSP || c == WJ) {
            curr = nat + 1;
            if (foundFirst)     // only scan backwards for first inhibitor
                scanBackClusters(ut, start, before);
            foundFirst = false; // don't scan backwards if we go around again. Also marks found something

            utext_next32(ut);
            scanFwdClusters(ut, end, after);
            nat = after + 1;

            if (c == ZWSP || c == WJ) {  // did we hit another one?
                continue;
            } else {
                break;
            }
        }

        ++nat;                  // keep hunting
        utext_next32(ut);
    }

    utext_close(ut);

    if (nat >= end && foundFirst) {
        start = before = after = nat;
        return false;           // failed to find anything
    }
    else {
        start = curr;
    }
    return true;                // yup hit one
}

/*
 ******************************************************************
 * PossibleWord
 */

// Helper class for improving readability of the Thai/Lao/Khmer word break
// algorithm. The implementation is completely inline.

// List size, limited by the maximum number of words in the dictionary
// that form a nested sequence.
static const int32_t POSSIBLE_WORD_LIST_MAX = 20;

class PossibleWord {
private:
    // list of word candidate lengths, in increasing length order
    // TODO: bytes would be sufficient for word lengths.
    int32_t   count;      // Count of candidates
    int32_t   prefix;     // The longest match with a dictionary word
    int32_t   offset;     // Offset in the text of these candidates
    int32_t   mark;       // The preferred candidate's offset
    int32_t   current;    // The candidate we're currently looking at
    int32_t   cuLengths[POSSIBLE_WORD_LIST_MAX];   // Word Lengths, in code units.
    int32_t   cpLengths[POSSIBLE_WORD_LIST_MAX];   // Word Lengths, in code points.

public:
    PossibleWord() : count(0), prefix(0), offset(-1), mark(0), current(0) {};
    ~PossibleWord() {};

    // Fill the list of candidates if needed, select the longest, and return the number found
    int32_t   candidates( UText *text, DictionaryMatcher *dict, int32_t rangeEnd, UnicodeSet const *ignoreSet = NULL, int32_t minLength = 0 );

    // Select the currently marked candidate, point after it in the text, and invalidate self
    int32_t   acceptMarked( UText *text );

    // Back up from the current candidate to the next shorter one; return TRUE if that exists
    // and point the text after it
    UBool     backUp( UText *text );

    // Return the longest prefix this candidate location shares with a dictionary word
    // Return value is in code points.
    int32_t   longestPrefix() { return prefix; };

    // Mark the current candidate as the one we like
    void      markCurrent() { mark = current; };

    // Get length in code points of the marked word.
    int32_t   markedCPLength() { return cpLengths[mark]; };
};


int32_t PossibleWord::candidates( UText *text, DictionaryMatcher *dict, int32_t rangeEnd, UnicodeSet const *ignoreSet, int32_t minLength) {
    // TODO: If getIndex is too slow, use offset < 0 and add discardAll()
    int32_t start = (int32_t)utext_getNativeIndex(text);
    if (start != offset) {
        offset = start;
        count = dict->matches(text, rangeEnd-start, UPRV_LENGTHOF(cuLengths), cuLengths, cpLengths, NULL, &prefix, ignoreSet, minLength);
        // Dictionary leaves text after longest prefix, not longest word. Back up.
        if (count <= 0) {
            utext_setNativeIndex(text, start);
        }
    }
    if (count > 0) {
        utext_setNativeIndex(text, start+cuLengths[count-1]);
    }
    current = count-1;
    mark = current;
    return count;
}

int32_t
PossibleWord::acceptMarked( UText *text ) {
    utext_setNativeIndex(text, offset + cuLengths[mark]);
    return cuLengths[mark];
}


UBool
PossibleWord::backUp( UText *text ) {
    if (current > 0) {
        utext_setNativeIndex(text, offset + cuLengths[--current]);
        return TRUE;
    }
    return FALSE;
}

/*
 ******************************************************************
 * ThaiBreakEngine
 */

// How many words in a row are "good enough"?
static const int32_t THAI_LOOKAHEAD = 3;

// Will not combine a non-word with a preceding dictionary word longer than this
static const int32_t THAI_ROOT_COMBINE_THRESHOLD = 3;

// Will not combine a non-word that shares at least this much prefix with a
// dictionary word, with a preceding word
static const int32_t THAI_PREFIX_COMBINE_THRESHOLD = 3;

// Ellision character
static const int32_t THAI_PAIYANNOI = 0x0E2F;

// Repeat character
static const int32_t THAI_MAIYAMOK = 0x0E46;

// Minimum word size
static const int32_t THAI_MIN_WORD = 2;

// Minimum number of characters for two words
static const int32_t THAI_MIN_WORD_SPAN = THAI_MIN_WORD * 2;

ThaiBreakEngine::ThaiBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status)
    : DictionaryBreakEngine((1<<UBRK_WORD) | (1<<UBRK_LINE)),
      fDictionary(adoptDictionary)
{
    fThaiWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Thai:]&[:LineBreak=SA:]]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fThaiWordSet);
    }
    fMarkSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Thai:]&[:LineBreak=SA:]&[:M:]]"), status);
    fMarkSet.add(0x0020);
    fEndWordSet = fThaiWordSet;
    fEndWordSet.remove(0x0E31);             // MAI HAN-AKAT
    fEndWordSet.remove(0x0E40, 0x0E44);     // SARA E through SARA AI MAIMALAI
    fBeginWordSet.add(0x0E01, 0x0E2E);      // KO KAI through HO NOKHUK
    fBeginWordSet.add(0x0E40, 0x0E44);      // SARA E through SARA AI MAIMALAI
    fSuffixSet.add(THAI_PAIYANNOI);
    fSuffixSet.add(THAI_MAIYAMOK);

    // Compact for caching.
    fMarkSet.compact();
    fEndWordSet.compact();
    fBeginWordSet.compact();
    fSuffixSet.compact();
}

ThaiBreakEngine::~ThaiBreakEngine() {
    delete fDictionary;
}

int32_t
ThaiBreakEngine::divideUpDictionaryRange( UText *text,
                                                int32_t rangeStart,
                                                int32_t rangeEnd,
                                                UStack &foundBreaks ) const {
    utext_setNativeIndex(text, rangeStart);
    utext_moveIndex32(text, THAI_MIN_WORD_SPAN);
    if (utext_getNativeIndex(text) >= rangeEnd) {
        return 0;       // Not enough characters for two words
    }
    utext_setNativeIndex(text, rangeStart);


    uint32_t wordsFound = 0;
    int32_t cpWordLength = 0;    // Word Length in Code Points.
    int32_t cuWordLength = 0;    // Word length in code units (UText native indexing)
    int32_t current;
    UErrorCode status = U_ZERO_ERROR;
    PossibleWord words[THAI_LOOKAHEAD];
    
    utext_setNativeIndex(text, rangeStart);
    
    while (U_SUCCESS(status) && (current = (int32_t)utext_getNativeIndex(text)) < rangeEnd) {
        cpWordLength = 0;
        cuWordLength = 0;

        // Look for candidate words at the current position
        int32_t candidates = words[wordsFound%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
        
        // If we found exactly one, use that
        if (candidates == 1) {
            cuWordLength = words[wordsFound % THAI_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % THAI_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        // If there was more than one, see which one can take us forward the most words
        else if (candidates > 1) {
            // If we're already at the end of the range, we're done
            if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                goto foundBest;
            }
            do {
                int32_t wordsMatched = 1;
                if (words[(wordsFound + 1) % THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) > 0) {
                    if (wordsMatched < 2) {
                        // Followed by another dictionary word; mark first word as a good candidate
                        words[wordsFound%THAI_LOOKAHEAD].markCurrent();
                        wordsMatched = 2;
                    }
                    
                    // If we're already at the end of the range, we're done
                    if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                        goto foundBest;
                    }
                    
                    // See if any of the possible second words is followed by a third word
                    do {
                        // If we find a third word, stop right away
                        if (words[(wordsFound + 2) % THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd)) {
                            words[wordsFound % THAI_LOOKAHEAD].markCurrent();
                            goto foundBest;
                        }
                    }
                    while (words[(wordsFound + 1) % THAI_LOOKAHEAD].backUp(text));
                }
            }
            while (words[wordsFound % THAI_LOOKAHEAD].backUp(text));
foundBest:
            // Set UText position to after the accepted word.
            cuWordLength = words[wordsFound % THAI_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % THAI_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        
        // We come here after having either found a word or not. We look ahead to the
        // next word. If it's not a dictionary word, we will combine it with the word we
        // just found (if there is one), but only if the preceding word does not exceed
        // the threshold.
        // The text iterator should now be positioned at the end of the word we found.
        
        UChar32 uc = 0;
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd &&  cpWordLength < THAI_ROOT_COMBINE_THRESHOLD) {
            // if it is a dictionary word, do nothing. If it isn't, then if there is
            // no preceding word, or the non-word shares less than the minimum threshold
            // of characters with a dictionary word, then scan to resynchronize
            if (words[wordsFound % THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                  && (cuWordLength == 0
                      || words[wordsFound%THAI_LOOKAHEAD].longestPrefix() < THAI_PREFIX_COMBINE_THRESHOLD)) {
                // Look for a plausible word boundary
                int32_t remaining = rangeEnd - (current+cuWordLength);
                UChar32 pc;
                int32_t chars = 0;
                for (;;) {
                    int32_t pcIndex = (int32_t)utext_getNativeIndex(text);
                    pc = utext_next32(text);
                    int32_t pcSize = (int32_t)utext_getNativeIndex(text) - pcIndex;
                    chars += pcSize;
                    remaining -= pcSize;
                    if (remaining <= 0) {
                        break;
                    }
                    uc = utext_current32(text);
                    if (fEndWordSet.contains(pc) && fBeginWordSet.contains(uc)) {
                        // Maybe. See if it's in the dictionary.
                        // NOTE: In the original Apple code, checked that the next
                        // two characters after uc were not 0x0E4C THANTHAKHAT before
                        // checking the dictionary. That is just a performance filter,
                        // but it's not clear it's faster than checking the trie.
                        int32_t candidates = words[(wordsFound + 1) % THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
                        utext_setNativeIndex(text, current + cuWordLength + chars);
                        if (candidates > 0) {
                            break;
                        }
                    }
                }
                
                // Bump the word count if there wasn't already one
                if (cuWordLength <= 0) {
                    wordsFound += 1;
                }
                
                // Update the length with the passed-over characters
                cuWordLength += chars;
            }
            else {
                // Back up to where we were for next iteration
                utext_setNativeIndex(text, current+cuWordLength);
            }
        }
        
        // Never stop before a combining mark.
        int32_t currPos;
        while ((currPos = (int32_t)utext_getNativeIndex(text)) < rangeEnd && fMarkSet.contains(utext_current32(text))) {
            utext_next32(text);
            cuWordLength += (int32_t)utext_getNativeIndex(text) - currPos;
        }
        
        // Look ahead for possible suffixes if a dictionary word does not follow.
        // We do this in code rather than using a rule so that the heuristic
        // resynch continues to function. For example, one of the suffix characters
        // could be a typo in the middle of a word.
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd && cuWordLength > 0) {
            if (words[wordsFound%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                && fSuffixSet.contains(uc = utext_current32(text))) {
                if (uc == THAI_PAIYANNOI) {
                    if (!fSuffixSet.contains(utext_previous32(text))) {
                        // Skip over previous end and PAIYANNOI
                        utext_next32(text);
                        int32_t paiyannoiIndex = (int32_t)utext_getNativeIndex(text);
                        utext_next32(text);
                        cuWordLength += (int32_t)utext_getNativeIndex(text) - paiyannoiIndex;    // Add PAIYANNOI to word
                        uc = utext_current32(text);     // Fetch next character
                    }
                    else {
                        // Restore prior position
                        utext_next32(text);
                    }
                }
                if (uc == THAI_MAIYAMOK) {
                    if (utext_previous32(text) != THAI_MAIYAMOK) {
                        // Skip over previous end and MAIYAMOK
                        utext_next32(text);
                        int32_t maiyamokIndex = (int32_t)utext_getNativeIndex(text);
                        utext_next32(text);
                        cuWordLength += (int32_t)utext_getNativeIndex(text) - maiyamokIndex;    // Add MAIYAMOK to word
                    }
                    else {
                        // Restore prior position
                        utext_next32(text);
                    }
                }
            }
            else {
                utext_setNativeIndex(text, current+cuWordLength);
            }
        }

        // Did we find a word on this iteration? If so, push it on the break stack
        if (cuWordLength > 0) {
            foundBreaks.push((current+cuWordLength), status);
        }
    }

    // Don't return a break for the end of the dictionary range if there is one there.
    if (foundBreaks.peeki() >= rangeEnd) {
        (void) foundBreaks.popi();
        wordsFound -= 1;
    }

    return wordsFound;
}

/*
 ******************************************************************
 * LaoBreakEngine
 */

// How many words in a row are "good enough"?
static const int32_t LAO_LOOKAHEAD = 3;

// Will not combine a non-word with a preceding dictionary word longer than this
static const int32_t LAO_ROOT_COMBINE_THRESHOLD = 3;

// Will not combine a non-word that shares at least this much prefix with a
// dictionary word, with a preceding word
static const int32_t LAO_PREFIX_COMBINE_THRESHOLD = 3;

// Minimum word size
static const int32_t LAO_MIN_WORD = 2;

// Minimum number of characters for two words
static const int32_t LAO_MIN_WORD_SPAN = LAO_MIN_WORD * 2;

LaoBreakEngine::LaoBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status)
    : DictionaryBreakEngine((1<<UBRK_WORD) | (1<<UBRK_LINE)),
      fDictionary(adoptDictionary)
{
    fLaoWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Laoo:]&[:LineBreak=SA:]]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fLaoWordSet);
    }
    fMarkSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Laoo:]&[:LineBreak=SA:]&[:M:]]"), status);
    fMarkSet.add(0x0020);
    fEndWordSet = fLaoWordSet;
    fEndWordSet.remove(0x0EC0, 0x0EC4);     // prefix vowels
    fBeginWordSet.add(0x0E81, 0x0EAE);      // basic consonants (including holes for corresponding Thai characters)
    fBeginWordSet.add(0x0EDC, 0x0EDD);      // digraph consonants (no Thai equivalent)
    fBeginWordSet.add(0x0EC0, 0x0EC4);      // prefix vowels

    // Compact for caching.
    fMarkSet.compact();
    fEndWordSet.compact();
    fBeginWordSet.compact();
}

LaoBreakEngine::~LaoBreakEngine() {
    delete fDictionary;
}

int32_t
LaoBreakEngine::divideUpDictionaryRange( UText *text,
                                                int32_t rangeStart,
                                                int32_t rangeEnd,
                                                UStack &foundBreaks ) const {
    if ((rangeEnd - rangeStart) < LAO_MIN_WORD_SPAN) {
        return 0;       // Not enough characters for two words
    }

    uint32_t wordsFound = 0;
    int32_t cpWordLength = 0;
    int32_t cuWordLength = 0;
    int32_t current;
    UErrorCode status = U_ZERO_ERROR;
    PossibleWord words[LAO_LOOKAHEAD];
    
    utext_setNativeIndex(text, rangeStart);
    
    while (U_SUCCESS(status) && (current = (int32_t)utext_getNativeIndex(text)) < rangeEnd) {
        cuWordLength = 0;
        cpWordLength = 0;

        // Look for candidate words at the current position
        int32_t candidates = words[wordsFound%LAO_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
        
        // If we found exactly one, use that
        if (candidates == 1) {
            cuWordLength = words[wordsFound % LAO_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % LAO_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        // If there was more than one, see which one can take us forward the most words
        else if (candidates > 1) {
            // If we're already at the end of the range, we're done
            if (utext_getNativeIndex(text) >= rangeEnd) {
                goto foundBest;
            }
            do {
                int32_t wordsMatched = 1;
                if (words[(wordsFound + 1) % LAO_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) > 0) {
                    if (wordsMatched < 2) {
                        // Followed by another dictionary word; mark first word as a good candidate
                        words[wordsFound%LAO_LOOKAHEAD].markCurrent();
                        wordsMatched = 2;
                    }
                    
                    // If we're already at the end of the range, we're done
                    if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                        goto foundBest;
                    }
                    
                    // See if any of the possible second words is followed by a third word
                    do {
                        // If we find a third word, stop right away
                        if (words[(wordsFound + 2) % LAO_LOOKAHEAD].candidates(text, fDictionary, rangeEnd)) {
                            words[wordsFound % LAO_LOOKAHEAD].markCurrent();
                            goto foundBest;
                        }
                    }
                    while (words[(wordsFound + 1) % LAO_LOOKAHEAD].backUp(text));
                }
            }
            while (words[wordsFound % LAO_LOOKAHEAD].backUp(text));
foundBest:
            cuWordLength = words[wordsFound % LAO_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % LAO_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        
        // We come here after having either found a word or not. We look ahead to the
        // next word. If it's not a dictionary word, we will combine it withe the word we
        // just found (if there is one), but only if the preceding word does not exceed
        // the threshold.
        // The text iterator should now be positioned at the end of the word we found.
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd && cpWordLength < LAO_ROOT_COMBINE_THRESHOLD) {
            // if it is a dictionary word, do nothing. If it isn't, then if there is
            // no preceding word, or the non-word shares less than the minimum threshold
            // of characters with a dictionary word, then scan to resynchronize
            if (words[wordsFound % LAO_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                  && (cuWordLength == 0
                      || words[wordsFound%LAO_LOOKAHEAD].longestPrefix() < LAO_PREFIX_COMBINE_THRESHOLD)) {
                // Look for a plausible word boundary
                int32_t remaining = rangeEnd - (current + cuWordLength);
                UChar32 pc;
                UChar32 uc;
                int32_t chars = 0;
                for (;;) {
                    int32_t pcIndex = (int32_t)utext_getNativeIndex(text);
                    pc = utext_next32(text);
                    int32_t pcSize = (int32_t)utext_getNativeIndex(text) - pcIndex;
                    chars += pcSize;
                    remaining -= pcSize;
                    if (remaining <= 0) {
                        break;
                    }
                    uc = utext_current32(text);
                    if (fEndWordSet.contains(pc) && fBeginWordSet.contains(uc)) {
                        // Maybe. See if it's in the dictionary.
                        // TODO: this looks iffy; compare with old code.
                        int32_t candidates = words[(wordsFound + 1) % LAO_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
                        utext_setNativeIndex(text, current + cuWordLength + chars);
                        if (candidates > 0) {
                            break;
                        }
                    }
                }
                
                // Bump the word count if there wasn't already one
                if (cuWordLength <= 0) {
                    wordsFound += 1;
                }
                
                // Update the length with the passed-over characters
                cuWordLength += chars;
            }
            else {
                // Back up to where we were for next iteration
                utext_setNativeIndex(text, current + cuWordLength);
            }
        }
        
        // Never stop before a combining mark.
        int32_t currPos;
        while ((currPos = (int32_t)utext_getNativeIndex(text)) < rangeEnd && fMarkSet.contains(utext_current32(text))) {
            utext_next32(text);
            cuWordLength += (int32_t)utext_getNativeIndex(text) - currPos;
        }
        
        // Look ahead for possible suffixes if a dictionary word does not follow.
        // We do this in code rather than using a rule so that the heuristic
        // resynch continues to function. For example, one of the suffix characters
        // could be a typo in the middle of a word.
        // NOT CURRENTLY APPLICABLE TO LAO

        // Did we find a word on this iteration? If so, push it on the break stack
        if (cuWordLength > 0) {
            foundBreaks.push((current+cuWordLength), status);
        }
    }

    // Don't return a break for the end of the dictionary range if there is one there.
    if (foundBreaks.peeki() >= rangeEnd) {
        (void) foundBreaks.popi();
        wordsFound -= 1;
    }

    return wordsFound;
}

/*
 ******************************************************************
 * BurmeseBreakEngine
 */

// How many words in a row are "good enough"?
static const int32_t BURMESE_LOOKAHEAD = 3;

// Will not combine a non-word with a preceding dictionary word longer than this
static const int32_t BURMESE_ROOT_COMBINE_THRESHOLD = 3;

// Will not combine a non-word that shares at least this much prefix with a
// dictionary word, with a preceding word
static const int32_t BURMESE_PREFIX_COMBINE_THRESHOLD = 3;

// Minimum word size
static const int32_t BURMESE_MIN_WORD = 2;

// Minimum number of characters for two words
static const int32_t BURMESE_MIN_WORD_SPAN = BURMESE_MIN_WORD * 2;

BurmeseBreakEngine::BurmeseBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status)
    : DictionaryBreakEngine((1<<UBRK_WORD) | (1<<UBRK_LINE)),
      fDictionary(adoptDictionary)
{
    fBurmeseWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Mymr:]&[:LineBreak=SA:]]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fBurmeseWordSet);
    }
    fMarkSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Mymr:]&[:LineBreak=SA:]&[:M:]]"), status);
    fMarkSet.add(0x0020);
    fEndWordSet = fBurmeseWordSet;
    fBeginWordSet.add(0x1000, 0x102A);      // basic consonants and independent vowels

    // Compact for caching.
    fMarkSet.compact();
    fEndWordSet.compact();
    fBeginWordSet.compact();
}

BurmeseBreakEngine::~BurmeseBreakEngine() {
    delete fDictionary;
}

int32_t
BurmeseBreakEngine::divideUpDictionaryRange( UText *text,
                                                int32_t rangeStart,
                                                int32_t rangeEnd,
                                                UStack &foundBreaks ) const {
    if ((rangeEnd - rangeStart) < BURMESE_MIN_WORD_SPAN) {
        return 0;       // Not enough characters for two words
    }

    uint32_t wordsFound = 0;
    int32_t cpWordLength = 0;
    int32_t cuWordLength = 0;
    int32_t current;
    UErrorCode status = U_ZERO_ERROR;
    PossibleWord words[BURMESE_LOOKAHEAD];
    
    utext_setNativeIndex(text, rangeStart);
    
    while (U_SUCCESS(status) && (current = (int32_t)utext_getNativeIndex(text)) < rangeEnd) {
        cuWordLength = 0;
        cpWordLength = 0;

        // Look for candidate words at the current position
        int32_t candidates = words[wordsFound%BURMESE_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
        
        // If we found exactly one, use that
        if (candidates == 1) {
            cuWordLength = words[wordsFound % BURMESE_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % BURMESE_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        // If there was more than one, see which one can take us forward the most words
        else if (candidates > 1) {
            // If we're already at the end of the range, we're done
            if (utext_getNativeIndex(text) >= rangeEnd) {
                goto foundBest;
            }
            do {
                int32_t wordsMatched = 1;
                if (words[(wordsFound + 1) % BURMESE_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) > 0) {
                    if (wordsMatched < 2) {
                        // Followed by another dictionary word; mark first word as a good candidate
                        words[wordsFound%BURMESE_LOOKAHEAD].markCurrent();
                        wordsMatched = 2;
                    }
                    
                    // If we're already at the end of the range, we're done
                    if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                        goto foundBest;
                    }
                    
                    // See if any of the possible second words is followed by a third word
                    do {
                        // If we find a third word, stop right away
                        if (words[(wordsFound + 2) % BURMESE_LOOKAHEAD].candidates(text, fDictionary, rangeEnd)) {
                            words[wordsFound % BURMESE_LOOKAHEAD].markCurrent();
                            goto foundBest;
                        }
                    }
                    while (words[(wordsFound + 1) % BURMESE_LOOKAHEAD].backUp(text));
                }
            }
            while (words[wordsFound % BURMESE_LOOKAHEAD].backUp(text));
foundBest:
            cuWordLength = words[wordsFound % BURMESE_LOOKAHEAD].acceptMarked(text);
            cpWordLength = words[wordsFound % BURMESE_LOOKAHEAD].markedCPLength();
            wordsFound += 1;
        }
        
        // We come here after having either found a word or not. We look ahead to the
        // next word. If it's not a dictionary word, we will combine it withe the word we
        // just found (if there is one), but only if the preceding word does not exceed
        // the threshold.
        // The text iterator should now be positioned at the end of the word we found.
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd && cpWordLength < BURMESE_ROOT_COMBINE_THRESHOLD) {
            // if it is a dictionary word, do nothing. If it isn't, then if there is
            // no preceding word, or the non-word shares less than the minimum threshold
            // of characters with a dictionary word, then scan to resynchronize
            if (words[wordsFound % BURMESE_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                  && (cuWordLength == 0
                      || words[wordsFound%BURMESE_LOOKAHEAD].longestPrefix() < BURMESE_PREFIX_COMBINE_THRESHOLD)) {
                // Look for a plausible word boundary
                int32_t remaining = rangeEnd - (current + cuWordLength);
                UChar32 pc;
                UChar32 uc;
                int32_t chars = 0;
                for (;;) {
                    int32_t pcIndex = (int32_t)utext_getNativeIndex(text);
                    pc = utext_next32(text);
                    int32_t pcSize = (int32_t)utext_getNativeIndex(text) - pcIndex;
                    chars += pcSize;
                    remaining -= pcSize;
                    if (remaining <= 0) {
                        break;
                    }
                    uc = utext_current32(text);
                    if (fEndWordSet.contains(pc) && fBeginWordSet.contains(uc)) {
                        // Maybe. See if it's in the dictionary.
                        // TODO: this looks iffy; compare with old code.
                        int32_t candidates = words[(wordsFound + 1) % BURMESE_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
                        utext_setNativeIndex(text, current + cuWordLength + chars);
                        if (candidates > 0) {
                            break;
                        }
                    }
                }
                
                // Bump the word count if there wasn't already one
                if (cuWordLength <= 0) {
                    wordsFound += 1;
                }
                
                // Update the length with the passed-over characters
                cuWordLength += chars;
            }
            else {
                // Back up to where we were for next iteration
                utext_setNativeIndex(text, current + cuWordLength);
            }
        }
        
        // Never stop before a combining mark.
        int32_t currPos;
        while ((currPos = (int32_t)utext_getNativeIndex(text)) < rangeEnd && fMarkSet.contains(utext_current32(text))) {
            utext_next32(text);
            cuWordLength += (int32_t)utext_getNativeIndex(text) - currPos;
        }
        
        // Look ahead for possible suffixes if a dictionary word does not follow.
        // We do this in code rather than using a rule so that the heuristic
        // resynch continues to function. For example, one of the suffix characters
        // could be a typo in the middle of a word.
        // NOT CURRENTLY APPLICABLE TO BURMESE

        // Did we find a word on this iteration? If so, push it on the break stack
        if (cuWordLength > 0) {
            foundBreaks.push((current+cuWordLength), status);
        }
    }

    // Don't return a break for the end of the dictionary range if there is one there.
    if (foundBreaks.peeki() >= rangeEnd) {
        (void) foundBreaks.popi();
        wordsFound -= 1;
    }

    return wordsFound;
}

/*
 ******************************************************************
 * KhmerBreakEngine
 */

KhmerBreakEngine::KhmerBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status)
    : DictionaryBreakEngine((1 << UBRK_WORD) | (1 << UBRK_LINE)),
      fDictionary(adoptDictionary)
{

    clusterLimit = 3;

    fKhmerWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Khmr:]\\u2060\\u200C\\u200D]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fKhmerWordSet);
    }
    fMarkSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Khmr:]&[:LineBreak=SA:]&[:M:]]"), status);
    fIgnoreSet.add(0x2060);         // WJ
    fIgnoreSet.add(0x200C, 0x200D); // ZWJ, ZWNJ
    fBaseSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Khmr:]&[:lb=SA:]&[:^M:]]"), status);
    fPuncSet.applyPattern(UNICODE_STRING_SIMPLE("[\\u17D4\\u17D5\\u17D6\\u17D7\\u17D9:]"), status);

    // Compact for caching.
    fMarkSet.compact();
	fIgnoreSet.compact();
	fBaseSet.compact();
	fPuncSet.compact();
}

KhmerBreakEngine::~KhmerBreakEngine() {
    delete fDictionary;
}

int32_t
KhmerBreakEngine::divideUpDictionaryRange( UText *text,
                                                int32_t rangeStart,
                                                int32_t rangeEnd,
                                                UStack &foundBreaks ) const {
    uint32_t wordsFound = foundBreaks.size();
    UErrorCode status = U_ZERO_ERROR;
    int32_t before = 0;
    int32_t after = 0;
    int32_t finalBefore = 0;
    int32_t initAfter = 0;
    int32_t scanStart = rangeStart;
    int32_t scanEnd = rangeEnd;

    bool startZwsp = false;
    bool breakStart = false;
    bool breakEnd = false;

    if (rangeStart > 0) {
        --scanStart;
        startZwsp = scanBeforeStart(text, scanStart, breakStart);
    }
    utext_setNativeIndex(text, rangeStart);
    scanFwdClusters(text, rangeEnd, initAfter);
    bool endZwsp = scanAfterEnd(text, utext_nativeLength(text), scanEnd, breakEnd);
    utext_setNativeIndex(text, rangeEnd - 1);
    scanBackClusters(text, rangeStart, finalBefore);
    if (finalBefore < initAfter) {   // the whole run is tented so no breaks
        if (breakStart || fTypes < UBRK_LINE)
            foundBreaks.push(rangeStart, status);
        if (breakEnd || fTypes < UBRK_LINE)
            foundBreaks.push(rangeEnd, status);
        return foundBreaks.size() - wordsFound;
    }

    scanStart = rangeStart;
    scanWJ(text, scanStart, rangeEnd, before, after);
    if (startZwsp || initAfter >= before) {
        after = initAfter;
        before = 0;
    }
    if (!endZwsp && after > finalBefore && after < rangeEnd)
        endZwsp = true;
    if (endZwsp && before > finalBefore)
        before = finalBefore;

    utext_setNativeIndex(text, rangeStart);
    int32_t numCodePts = rangeEnd - rangeStart;
    // bestSnlp[i] is the snlp of the best segmentation of the first i
    // code points in the range to be matched.
    UVector32 bestSnlp(numCodePts + 1, status);
    bestSnlp.addElement(0, status);
    for(int32_t i = 1; i <= numCodePts; i++) {
        bestSnlp.addElement(kuint32max, status);
    }

    // prev[i] is the index of the last code point in the previous word in
    // the best segmentation of the first i characters. Note negative implies
	// that the code point is part of an unknown word.
    UVector32 prev(numCodePts + 1, status);
    for(int32_t i = 0; i <= numCodePts; i++) {
        prev.addElement(kuint32max, status);
    }

    const int32_t maxWordSize = 20;
    UVector32 values(maxWordSize, status);
    values.setSize(maxWordSize);
    UVector32 lengths(maxWordSize, status);
    lengths.setSize(maxWordSize);

    // Dynamic programming to find the best segmentation.

    // In outer loop, i  is the code point index,
    //                ix is the corresponding string (code unit) index.
    //    They differ when the string contains supplementary characters.
    int32_t ix = rangeStart;
    for (int32_t i = 0;  i < numCodePts;  ++i, utext_setNativeIndex(text, ++ix)) {
        if ((uint32_t)bestSnlp.elementAti(i) == kuint32max) {
            continue;
        }

        int32_t count;
        count = fDictionary->matches(text, numCodePts - i, maxWordSize,
                             NULL, lengths.getBuffer(), values.getBuffer(), NULL, &fIgnoreSet, 2);
                             // Note: lengths is filled with code point lengths
                             //       The NULL parameter is the ignored code unit lengths.

        for (int32_t j = 0; j < count; j++) {
            int32_t ln = lengths.elementAti(j);
            if (ln + i >= numCodePts)
                continue;
            utext_setNativeIndex(text, ln+ix);
            int32_t c = utext_current32(text);
            if (fMarkSet.contains(c) || c == 0x17D2) { // Coeng
                lengths.removeElementAt(j);
                values.removeElementAt(j);
                --j;
                --count;
            }
        }
        if (count == 0) {
            utext_setNativeIndex(text, ix);
            int32_t c = utext_current32(text);
            if (fPuncSet.contains(c) || fIgnoreSet.contains(c) || c == ZWSP) {
                values.setElementAt(0, count);
                lengths.setElementAt(1, count++);
            } else if (fBaseSet.contains(c)) {
                int32_t currix = utext_getNativeIndex(text);
                do {
                    utext_next32(text);
                    c = utext_current32(text);
                    if (utext_getNativeIndex(text) >= rangeEnd)
                        break;
                    if (c == 0x17D2) { // Coeng
                        utext_next32(text);
                        c = utext_current32(text);
                        if (!fBaseSet.contains(c) || utext_getNativeIndex(text) >= rangeEnd) {
                            break;
                        } else {
                            utext_next32(text);
                            c = utext_current32(text);
                            if (utext_getNativeIndex(text) >= rangeEnd)
                                break;
                        }
                    }
                } while (fMarkSet.contains(c) || fIgnoreSet.contains(c));
                values.setElementAt(BADSNLP, count);
                lengths.setElementAt(utext_getNativeIndex(text) - currix, count++);
            } else {
                values.setElementAt(BADSNLP, count);
                lengths.setElementAt(1, count++);
            }
        }

        for (int32_t j = 0; j < count; j++) {
            uint32_t v = values.elementAti(j);
            int32_t newSnlp = bestSnlp.elementAti(i) + v;
            int32_t ln = lengths.elementAti(j);
            utext_setNativeIndex(text, ln+ix);
            int32_t c = utext_current32(text);
            while ((fPuncSet.contains(c) || fIgnoreSet.contains(c)) && ln + i < numCodePts) {
                ++ln;
                utext_next32(text);
                c = utext_current32(text);
            }
            int32_t ln_j_i = ln + i;   // yes really i!
            if (newSnlp < bestSnlp.elementAti(ln_j_i)) {
                if (v == BADSNLP) {
                    int32_t p = prev.elementAti(i);
                    if (p < 0)
                        prev.setElementAt(p, ln_j_i);
                    else
                        prev.setElementAt(-i, ln_j_i);
                }
                else
                    prev.setElementAt(i, ln_j_i);
                bestSnlp.setElementAt(newSnlp, ln_j_i);
            }
        }
    }
    // Start pushing the optimal offset index into t_boundary (t for tentative).
    // prev[numCodePts] is guaranteed to be meaningful.
    // We'll first push in the reverse order, i.e.,
    // t_boundary[0] = numCodePts, and afterwards do a swap.
    UVector32 t_boundary(numCodePts+1, status);

    int32_t numBreaks = 0;
    // No segmentation found, set boundary to end of range
    while (numCodePts >= 0 && (uint32_t)bestSnlp.elementAti(numCodePts) == kuint32max) {
        --numCodePts;
    }
    if (numCodePts < 0) {
        t_boundary.addElement(numCodePts, status);
        numBreaks++;
    } else {
        for (int32_t i = numCodePts; (uint32_t)i != kuint32max; i = prev.elementAti(i)) {
            if (i < 0) i = -i;
            t_boundary.addElement(i, status);
            numBreaks++;
        }
        U_ASSERT(prev.elementAti(t_boundary.elementAti(numBreaks - 1)) == 0);
    }

    // Now that we're done, convert positions in t_boundary[] (indices in
    // the normalized input string) back to indices in the original input UText
    // while reversing t_boundary and pushing values to foundBreaks.
    for (int32_t i = numBreaks-1; i >= 0; i--) {
        int32_t cpPos = t_boundary.elementAti(i);
        if (cpPos == 0 && !breakStart && fTypes >= UBRK_LINE) continue;
        int32_t utextPos = cpPos + rangeStart;
        while (utextPos > after && scanWJ(text, utextPos, scanEnd, before, after));
        if (utextPos < before) {
        // Boundaries are added to foundBreaks output in ascending order.
            U_ASSERT(foundBreaks.size() == 0 ||foundBreaks.peeki() < utextPos);
            foundBreaks.push(utextPos, status);
        }
    }

    // Don't return a break for the end of the dictionary range if there is one there.
    if (!breakEnd && fTypes >= UBRK_LINE && foundBreaks.peeki() >= rangeEnd) {
        (void) foundBreaks.popi();
    }
    return foundBreaks.size() - wordsFound;
}

#if !UCONFIG_NO_NORMALIZATION
/*
 ******************************************************************
 * CjkBreakEngine
 */
static const uint32_t kuint32max = 0xFFFFFFFF;
CjkBreakEngine::CjkBreakEngine(DictionaryMatcher *adoptDictionary, LanguageType type, UErrorCode &status)
: DictionaryBreakEngine(1 << UBRK_WORD), fDictionary(adoptDictionary) {
    // Korean dictionary only includes Hangul syllables
    fHangulWordSet.applyPattern(UNICODE_STRING_SIMPLE("[\\uac00-\\ud7a3]"), status);
    fHanWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Han:]"), status);
    fKatakanaWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Katakana:]\\uff9e\\uff9f]"), status);
    fHiraganaWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Hiragana:]"), status);
    nfkcNorm2 = Normalizer2::getNFKCInstance(status);

    if (U_SUCCESS(status)) {
        // handle Korean and Japanese/Chinese using different dictionaries
        if (type == kKorean) {
            setCharacters(fHangulWordSet);
        } else { //Chinese and Japanese
            UnicodeSet cjSet;
            cjSet.addAll(fHanWordSet);
            cjSet.addAll(fKatakanaWordSet);
            cjSet.addAll(fHiraganaWordSet);
            cjSet.add(0xFF70); // HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK
            cjSet.add(0x30FC); // KATAKANA-HIRAGANA PROLONGED SOUND MARK
            setCharacters(cjSet);
        }
    }
}

CjkBreakEngine::~CjkBreakEngine(){
    delete fDictionary;
}

// The katakanaCost values below are based on the length frequencies of all
// katakana phrases in the dictionary
static const int32_t kMaxKatakanaLength = 8;
static const int32_t kMaxKatakanaGroupLength = 20;
static const uint32_t maxSnlp = 255;

static inline uint32_t getKatakanaCost(int32_t wordLength){
    //TODO: fill array with actual values from dictionary!
    static const uint32_t katakanaCost[kMaxKatakanaLength + 1]
                                       = {8192, 984, 408, 240, 204, 252, 300, 372, 480};
    return (wordLength > kMaxKatakanaLength) ? 8192 : katakanaCost[wordLength];
}

static inline bool isKatakana(uint16_t value) {
    return (value >= 0x30A1u && value <= 0x30FEu && value != 0x30FBu) ||
            (value >= 0xFF66u && value <= 0xFF9fu);
}


// Function for accessing internal utext flags.
//   Replicates an internal UText function.

static inline int32_t utext_i32_flag(int32_t bitIndex) {
    return (int32_t)1 << bitIndex;
}

       
/*
 * @param text A UText representing the text
 * @param rangeStart The start of the range of dictionary characters
 * @param rangeEnd The end of the range of dictionary characters
 * @param foundBreaks Output of C array of int32_t break positions, or 0
 * @return The number of breaks found
 */
int32_t 
CjkBreakEngine::divideUpDictionaryRange( UText *inText,
        int32_t rangeStart,
        int32_t rangeEnd,
        UStack &foundBreaks ) const {
    if (rangeStart >= rangeEnd) {
        return 0;
    }

    // UnicodeString version of input UText, NFKC normalized if necessary.
    UnicodeString inString;

    // inputMap[inStringIndex] = corresponding native index from UText inText.
    // If NULL then mapping is 1:1
    LocalPointer<UVector32>     inputMap;

    UErrorCode     status      = U_ZERO_ERROR;


    // if UText has the input string as one contiguous UTF-16 chunk
    if ((inText->providerProperties & utext_i32_flag(UTEXT_PROVIDER_STABLE_CHUNKS)) &&
         inText->chunkNativeStart <= rangeStart &&
         inText->chunkNativeLimit >= rangeEnd   &&
         inText->nativeIndexingLimit >= rangeEnd - inText->chunkNativeStart) {

        // Input UText is in one contiguous UTF-16 chunk.
        // Use Read-only aliasing UnicodeString.
        inString.setTo(FALSE,
                       inText->chunkContents + rangeStart - inText->chunkNativeStart,
                       rangeEnd - rangeStart);
    } else {
        // Copy the text from the original inText (UText) to inString (UnicodeString).
        // Create a map from UnicodeString indices -> UText offsets.
        utext_setNativeIndex(inText, rangeStart);
        int32_t limit = rangeEnd;
        U_ASSERT(limit <= utext_nativeLength(inText));
        if (limit > utext_nativeLength(inText)) {
            limit = (int32_t)utext_nativeLength(inText);
        }
        inputMap.adoptInsteadAndCheckErrorCode(new UVector32(status), status);
        if (U_FAILURE(status)) {
            return 0;
        }
        while (utext_getNativeIndex(inText) < limit) {
            int32_t nativePosition = (int32_t)utext_getNativeIndex(inText);
            UChar32 c = utext_next32(inText);
            U_ASSERT(c != U_SENTINEL);
            inString.append(c);
            while (inputMap->size() < inString.length()) {
                inputMap->addElement(nativePosition, status);
            }
        }
        inputMap->addElement(limit, status);
    }


    if (!nfkcNorm2->isNormalized(inString, status)) {
        UnicodeString normalizedInput;
        //  normalizedMap[normalizedInput position] ==  original UText position.
        LocalPointer<UVector32> normalizedMap(new UVector32(status), status);
        if (U_FAILURE(status)) {
            return 0;
        }
        
        UnicodeString fragment;
        UnicodeString normalizedFragment;
        for (int32_t srcI = 0; srcI < inString.length();) {  // Once per normalization chunk
            fragment.remove();
            int32_t fragmentStartI = srcI;
            UChar32 c = inString.char32At(srcI);
            for (;;) {
                fragment.append(c);
                srcI = inString.moveIndex32(srcI, 1);
                if (srcI == inString.length()) {
                    break;
                }
                c = inString.char32At(srcI);
                if (nfkcNorm2->hasBoundaryBefore(c)) {
                    break;
                }
            }
            nfkcNorm2->normalize(fragment, normalizedFragment, status);
            normalizedInput.append(normalizedFragment);

            // Map every position in the normalized chunk to the start of the chunk
            //   in the original input.
            int32_t fragmentOriginalStart = inputMap.isValid() ?
                    inputMap->elementAti(fragmentStartI) : fragmentStartI+rangeStart;
            while (normalizedMap->size() < normalizedInput.length()) {
                normalizedMap->addElement(fragmentOriginalStart, status);
                if (U_FAILURE(status)) {
                    break;
                }
            }
        }
        U_ASSERT(normalizedMap->size() == normalizedInput.length());
        int32_t nativeEnd = inputMap.isValid() ?
                inputMap->elementAti(inString.length()) : inString.length()+rangeStart;
        normalizedMap->addElement(nativeEnd, status);

        inputMap.moveFrom(normalizedMap);
        inString.moveFrom(normalizedInput);
    }

    int32_t numCodePts = inString.countChar32();
    if (numCodePts != inString.length()) {
        // There are supplementary characters in the input.
        // The dictionary will produce boundary positions in terms of code point indexes,
        //   not in terms of code unit string indexes.
        // Use the inputMap mechanism to take care of this in addition to indexing differences
        //    from normalization and/or UTF-8 input.
        UBool hadExistingMap = inputMap.isValid();
        if (!hadExistingMap) {
            inputMap.adoptInsteadAndCheckErrorCode(new UVector32(status), status);
            if (U_FAILURE(status)) {
                return 0;
            }
        }
        int32_t cpIdx = 0;
        for (int32_t cuIdx = 0; ; cuIdx = inString.moveIndex32(cuIdx, 1)) {
            U_ASSERT(cuIdx >= cpIdx);
            if (hadExistingMap) {
                inputMap->setElementAt(inputMap->elementAti(cuIdx), cpIdx);
            } else {
                inputMap->addElement(cuIdx+rangeStart, status);
            }
            cpIdx++;
            if (cuIdx == inString.length()) {
               break;
            }
        }
    }
                
    // bestSnlp[i] is the snlp of the best segmentation of the first i
    // code points in the range to be matched.
    UVector32 bestSnlp(numCodePts + 1, status);
    bestSnlp.addElement(0, status);
    for(int32_t i = 1; i <= numCodePts; i++) {
        bestSnlp.addElement(kuint32max, status);
    }


    // prev[i] is the index of the last CJK code point in the previous word in 
    // the best segmentation of the first i characters.
    UVector32 prev(numCodePts + 1, status);
    for(int32_t i = 0; i <= numCodePts; i++){
        prev.addElement(-1, status);
    }

    const int32_t maxWordSize = 20;
    UVector32 values(numCodePts, status);
    values.setSize(numCodePts);
    UVector32 lengths(numCodePts, status);
    lengths.setSize(numCodePts);

    UText fu = UTEXT_INITIALIZER;
    utext_openUnicodeString(&fu, &inString, &status);

    // Dynamic programming to find the best segmentation.

    // In outer loop, i  is the code point index,
    //                ix is the corresponding string (code unit) index.
    //    They differ when the string contains supplementary characters.
    int32_t ix = 0;
    bool is_prev_katakana = false;
    for (int32_t i = 0;  i < numCodePts;  ++i, ix = inString.moveIndex32(ix, 1)) {
        if ((uint32_t)bestSnlp.elementAti(i) == kuint32max) {
            continue;
        }

        int32_t count;
        utext_setNativeIndex(&fu, ix);
        count = fDictionary->matches(&fu, maxWordSize, numCodePts,
                             NULL, lengths.getBuffer(), values.getBuffer(), NULL);
                             // Note: lengths is filled with code point lengths
                             //       The NULL parameter is the ignored code unit lengths.

        // if there are no single character matches found in the dictionary 
        // starting with this character, treat character as a 1-character word 
        // with the highest value possible, i.e. the least likely to occur.
        // Exclude Korean characters from this treatment, as they should be left
        // together by default.
        if ((count == 0 || lengths.elementAti(0) != 1) &&
                !fHangulWordSet.contains(inString.char32At(ix))) {
            values.setElementAt(maxSnlp, count);   // 255
            lengths.setElementAt(1, count++);
        }

        for (int32_t j = 0; j < count; j++) {
            uint32_t newSnlp = (uint32_t)bestSnlp.elementAti(i) + (uint32_t)values.elementAti(j);
            int32_t ln_j_i = lengths.elementAti(j) + i;
            if (newSnlp < (uint32_t)bestSnlp.elementAti(ln_j_i)) {
                bestSnlp.setElementAt(newSnlp, ln_j_i);
                prev.setElementAt(i, ln_j_i);
            }
        }

        // In Japanese,
        // Katakana word in single character is pretty rare. So we apply
        // the following heuristic to Katakana: any continuous run of Katakana
        // characters is considered a candidate word with a default cost
        // specified in the katakanaCost table according to its length.

        bool is_katakana = isKatakana(inString.char32At(ix));
        int32_t katakanaRunLength = 1;
        if (!is_prev_katakana && is_katakana) {
            int32_t j = inString.moveIndex32(ix, 1);
            // Find the end of the continuous run of Katakana characters
            while (j < inString.length() && katakanaRunLength < kMaxKatakanaGroupLength &&
                    isKatakana(inString.char32At(j))) {
                j = inString.moveIndex32(j, 1);
                katakanaRunLength++;
            }
            if (katakanaRunLength < kMaxKatakanaGroupLength) {
                uint32_t newSnlp = bestSnlp.elementAti(i) + getKatakanaCost(katakanaRunLength);
                if (newSnlp < (uint32_t)bestSnlp.elementAti(j)) {
                    bestSnlp.setElementAt(newSnlp, j);
                    prev.setElementAt(i, i+katakanaRunLength);  // prev[j] = i;
                }
            }
        }
        is_prev_katakana = is_katakana;
    }
    utext_close(&fu);

    // Start pushing the optimal offset index into t_boundary (t for tentative).
    // prev[numCodePts] is guaranteed to be meaningful.
    // We'll first push in the reverse order, i.e.,
    // t_boundary[0] = numCodePts, and afterwards do a swap.
    UVector32 t_boundary(numCodePts+1, status);

    int32_t numBreaks = 0;
    // No segmentation found, set boundary to end of range
    if ((uint32_t)bestSnlp.elementAti(numCodePts) == kuint32max) {
        t_boundary.addElement(numCodePts, status);
        numBreaks++;
    } else {
        for (int32_t i = numCodePts; i > 0; i = prev.elementAti(i)) {
            t_boundary.addElement(i, status);
            numBreaks++;
        }
        U_ASSERT(prev.elementAti(t_boundary.elementAti(numBreaks - 1)) == 0);
    }

    // Add a break for the start of the dictionary range if there is not one
    // there already.
    if (foundBreaks.size() == 0 || foundBreaks.peeki() < rangeStart) {
        t_boundary.addElement(0, status);
        numBreaks++;
    }

    // Now that we're done, convert positions in t_boundary[] (indices in 
    // the normalized input string) back to indices in the original input UText
    // while reversing t_boundary and pushing values to foundBreaks.
    for (int32_t i = numBreaks-1; i >= 0; i--) {
        int32_t cpPos = t_boundary.elementAti(i);
        int32_t utextPos =  inputMap.isValid() ? inputMap->elementAti(cpPos) : cpPos + rangeStart;
        // Boundaries are added to foundBreaks output in ascending order.
        U_ASSERT(foundBreaks.size() == 0 ||foundBreaks.peeki() < utextPos);
        foundBreaks.push(utextPos, status);
    }

    // inString goes out of scope
    // inputMap goes out of scope
    return numBreaks;
}
#endif

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

