
#include "tk_wordlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
   Example of how to lookup words to see if they are in the
   dictionary. WordList_Lookup returns 1 if the word is
   included, 0 if not.
--------------------------------------------------------- */
void LookupSomeWords( WordListNode *worddata )
{
	// Test some lookups
    const char *testWords[] = {
        "claw", "aardvark", "test", "rhino", "claws",
        "zzyzhaggis", "blarg", "teams", "troubadour",
    };
    for (int i=0; i < sizeof(testWords) / sizeof(testWords[0]); i++ ) {
        
        int result = WordList_Lookup( worddata, (char *)testWords[i] );

        printf("%20s ... %s\n", testWords[i], result?"FOUND":"Not Found" );
    }
}
/* ---------------------------------------------------------
  Example of enumerating al the words in the word list
--------------------------------------------------------- */
void GatherWordStats( WordListNode *worddata )
{
	WordList_Enumerator ee = WordList_MakeEnumerator( worddata );

	int longestWordLen =0;
	char longestWord[MAX_WORD_LENGTH];

	int mostVowelsLen =0;
	char mostVowels[MAX_WORD_LENGTH];

	int longestWordNoVowelsLen =0;
	char longestWordNoVowels[MAX_WORD_LENGTH];

	int wordCount = 0;
	int wordsWithoutVowels = 0;

	char * word;
	int i=0;
	while ((word = WordList_NextWord( &ee )) ) {
		
		if (i++==20) {
			printf( "...etc\n");
		} else if (i < 20) {
			printf( ">>%s\n", word );
		}

		int vowelsCount = 0;
		int len = strlen(word);
		for (char *ch = word; *ch; ch++) {
			if ((*ch=='a')||(*ch=='e')||(*ch=='i')||(*ch=='o')||(*ch=='u')) {
				vowelsCount++;
			}
		}
		wordCount++;

		if (len > longestWordLen) {
			strcpy( longestWord, word );
			longestWordLen = len;
		}

		if (vowelsCount == 0) {
			wordsWithoutVowels++;
			if (len > longestWordNoVowelsLen) {
				strcpy( longestWordNoVowels, word );
				longestWordNoVowelsLen = len;
			}
		}

		if (vowelsCount > mostVowelsLen) {
			strcpy( mostVowels, word );
			mostVowelsLen = vowelsCount;
		}
	}

	printf("-----------------------------\n");
	printf("Total Words : %d\n", wordCount );
	printf("Words Without Vowels : %d\n", wordsWithoutVowels );
	printf("Longest Word : %d '%s'\n", longestWordLen, longestWord );
	printf("Longest Word Without Vowels : '%s'\n", longestWordNoVowels );
	printf("Word with most Vowels : '%s'\n", mostVowels );
}

/* ---------------------------------------------------------
   Main program -- load worddata and use it.
  --------------------------------------------------------- */
int main( int argc, char *argv[] )
{
	if (argc<2) {
		printf("Usage: tk_wordlist_sample <wordlist.dat>\n");
		return 1;		
	}

	// Read word data file
	FILE *fp = fopen(argv[1], "rb");
	if (!fp) {
		printf("Couldn't open word data file '%s'\n", argv[1] );
		return 1;
	}

	// Get file size
    fseek( fp, 0L, SEEK_END );
    size_t filesz = ftell(fp);
    fseek( fp, 0L, SEEK_SET );

    // Allocate space for word data
    WordListNode *worddata = (WordListNode*)malloc( filesz );            
    if (!worddata) {
    	printf("Couldn't allocate word data.\n");
    	return 1;
    }

    // Load the word data
    size_t result = fread( worddata, filesz, 1, fp );
    if (!result) {
    	printf("Couldn't read data from '%s'\n", argv[1] );
    	return 1;
    }

    // Use the word data for things...
	LookupSomeWords( worddata );
	GatherWordStats( worddata );

}