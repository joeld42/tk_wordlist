#ifndef WORDLIST_H
#define WORDLIST_H




// This only affects storage in the wordtool, is only used
// at runtime to make the temporary buff with the terminator during lookup.
// Note this includes the terminator marker, and the nul, so a max
// word length of '30' here would really mean only 28 letters.
#define MAX_WORD_LENGTH (30)

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

// -----------------------------------------------------------------------

struct WordListNodeStruct;
typedef struct WordListNodeStruct WordListNode;

// Main Lookup function, takes a pointer to a packed, contiguous array of
// WordListNodes, and a target word, returns 1 if the word is found, 0 otherwise.
int WordList_Lookup( WordListNode *worddata, char *target);

// -----------------------------------------------------------------------
typedef struct WordList_EnumeratorStackStruct {
	WordListNode *node;
	int currWordLen;
} WordList_EnumeratorStackStruct;

typedef struct WordList_EnumeratorStruct {
	WordListNode *worddata;
	int stacktop;
	WordList_EnumeratorStackStruct stack[100];
	char currword[MAX_WORD_LENGTH];
} WordList_Enumerator;

WordList_Enumerator WordList_MakeEnumerator( WordListNode *worddata );
char *WordList_NextWord( WordList_Enumerator *enumerator );

// -----------------------------------------------------------------------

// Note: This only works because our node indices ends up being barely able to
// fit in a u16, we have 56k nodes and max u16 is 65k. So If you are using a larger
// word list then change this to u32
typedef u16 edgeIndex_t;

// This is how many edges we store by default with a node. If the node needs more edges it
// will grow to use up the next entry in the packed word list. The "sweet spot" for each
// dataset depends on how branchy it is, setting this to 2 had the smallest results for the
// dataset I was testing.
#define EDGE_LIMIT (2)

// Pragma pack to turn off padding. This optimizes for space but may make lookups significantly
// slower. If you care more about fast lookups you might want to remove the pramga pack and make
// numedges a u32 for alignment.
#pragma pack(push, 1)
typedef struct WordListNodeStruct {
    char label[4]; // NOTE: will NOT be null terminated if the length == 4
    u8 numEdges;
    edgeIndex_t edge[EDGE_LIMIT]; // NOTE: this will use the following wordlist entries if numEdges>EDGE_LIMIT
} WordListNode;
#pragma pack(pop)


#endif
