#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

// TODO: Remove dependenacies on string.h and stdlib.h, don't really need these
// TODO: Make actually a single-header file...

#include "tk_wordlist.h"


static int WordList_StartsWith( const char *prefix, const char *str )
{
    int prefixLen = strnlen( prefix, 4 );
    return strncmp( prefix, str, prefixLen ) == 0;
}

static int WordList_LookupImpl( WordListNode *base, WordListNode *curr, char *target)
{
    
    //printf("target %s curr->label '%s'\n", target, curr->label );
    target = target + strnlen( curr->label, 4 );
    //printf("target after '%s' strnlen() %d\n", target, strnlen( curr->label, 4) );
    if (*target==0) {
        return 1; // Found the whole string
    }
    
    for (int i=0; i < curr->numEdges; i++) {
        int ndx = curr->edge[i];
        WordListNode *edgeNode = base + ndx;
        char buff[5] = {};
        strncpy( buff, edgeNode->label, 4 );
        //printf("Check Edge '%s', '%s'\n", buff, target );

        if (WordList_StartsWith( edgeNode->label, target)) {
            //printf("Found prefix... target is now %s\n", target );
            return WordList_LookupImpl( base, edgeNode, target );
        }
    }
    return 0;
}

int WordList_Lookup( WordListNode *worddata, char *target)
{
    char buff[MAX_WORD_LENGTH];
    
    // add the terminator
    strcpy( buff, target );
    strcat( buff, "*" );
    int result = WordList_LookupImpl( worddata, worddata, buff );
    //printf ("WordList_Lookup: result %d\n", result );
    return result;
}

WordList_Enumerator WordList_MakeEnumerator( WordListNode *worddata )
{
    WordList_Enumerator enumerator ={0};
    enumerator.worddata = worddata;
    enumerator.stack[enumerator.stacktop].node = worddata;    
    return enumerator;
}

char *WordList_NextWord( WordList_Enumerator *enumerator )
{        
    while (1) {
        if (enumerator->stacktop < 0) {
            return NULL;
        }

        WordListNode *curr = enumerator->stack[ enumerator->stacktop ].node;
        //printf("curr %p (%d)\n", curr , enumerator->stacktop);
        enumerator->currword[ enumerator->stack[ enumerator->stacktop ].currWordLen] = '\0';
        enumerator->stacktop--;

        strncat( enumerator->currword, curr->label, 4 );
        int wordLen = strlen( enumerator->currword );
        if (enumerator->currword[wordLen-1]=='*') {
            enumerator->currword[wordLen-1] = '\0';
        }

        // printf ("NextWord: curr word %s, wordLen %d\n", enumerator->currword, wordLen );

        if (curr->numEdges == 0) {
            return enumerator->currword;
        } else {
            for (int i=curr->numEdges-1; i >= 0; i--) {
                enumerator->stacktop++;

                enumerator->stack[enumerator->stacktop].node = enumerator->worddata + curr->edge[i];
                enumerator->stack[enumerator->stacktop].currWordLen = wordLen;                
            }
        }
        
        // for (int i=0; i < curr->numEdges; i++) {
        //     TrieNode_WordList( curr->edge[i], buff );
        // }
    }
}


