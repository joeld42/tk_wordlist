#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "tk_wordlist.h"
}
/*
Usage:
./wordtool 2of12inf.txt gamedata/wordfile.dat
*/

#define DEBUG_GRAPH (0)

#define MEGABYTE (1024*1024)
#define WORDBUFFSZ (10*MEGABYTE)

// This is our piggy radix-prefix-trie used for building, this gets packed into the
// much more space-efficient WordList
// https://en.wikipedia.org/wiki/Radix_tree
#define MAX_EDGES 30

struct TrieNode;

struct TrieNode {
    uint32_t nodeId;
    WordListNode *packNode;
	char label[MAX_WORD_LENGTH];
	int numEdges;
	TrieNode *edge[MAX_EDGES];
    
    uint32_t hashcode;
    int nodeCount;
    bool visited;
    int dupeCount;
};

#define HASHTABLESIZE (11177)
struct CountTableEntry {
    uint32_t hashcode;
    uint32_t nodeCount;
    TrieNode *node; // any one node that matches the hash code
    CountTableEntry *next;
};

CountTableEntry *CountTableEntry_Lookup( CountTableEntry **countHash, TrieNode *node, bool addIfNotPresent )
{
    int ndx = node->hashcode % HASHTABLESIZE;
    CountTableEntry *entry = countHash[ndx];
    CountTableEntry **whereToAdd = &(countHash[ndx]);
    
    // See if we find our hash code
    while (entry) {
        if (entry->hashcode == node->hashcode) {
            return entry;
        }
        whereToAdd = &(entry->next);
        entry = entry->next;
    }
    
    if (!addIfNotPresent) {
        return NULL;
    }
    
    // Add if not present
    entry = (CountTableEntry*)malloc(sizeof(CountTableEntry));
    memset( entry, 0, sizeof(CountTableEntry));
    entry->hashcode = node->hashcode;
    entry->node = node;
    *whereToAdd = entry;

    return entry;
}

bool startsWith( const char *prefix, const char *str )
{
	int prefixLen = strlen( prefix );
	return strncmp( prefix, str, prefixLen ) == 0;
}


TrieNode *TrieNode_Lookup( TrieNode *curr, char *remainingLetters, char *prefix )
{
//    printf("lookup: curr is [%s] remaining [%s] prefix [%s]\n",
//           curr->label, remainingLetters, prefix );
	strncat( prefix, curr->label, MAX_WORD_LENGTH );
	
    // trim our prefix from remaining letters
    remainingLetters = remainingLetters + strlen(curr->label);
    
	if (strlen(remainingLetters)==0) {
//        printf("  lookup: out of letters\n" );
		return curr;
	}
    
//    printf("  lookup: checking for edges that match '%s'\n", remainingLetters );
	for (int i=0; i < curr->numEdges; i++) {
//        char *remain = remainingLetters + strlen(curr->edge[i]->label);
//        printf("  lookup: test prefix [%s] [%s]\n", curr->edge[i]->label, remainingLetters );
        if (startsWith( curr->edge[i]->label, remainingLetters)) {
            // This edge matches
//            printf("   lookup: edge prefix match\n");
            return TrieNode_Lookup( curr->edge[i], remainingLetters, prefix );
        }
	}
    return curr;
}

void TrieNode_PrintWords( TrieNode *curr, char *word )
{
	char buff[MAX_WORD_LENGTH];	
	for (int i=0; i < curr->numEdges; i++) {
		strcpy( buff, word );
		strcat( buff, curr->edge[i]->label );
        if (curr->numEdges==0) {
			printf("%s\n", buff );
		} else {
			TrieNode_PrintWords( curr->edge[i], buff );
		}
	}	
}

int g_nodeCount = 0;

TrieNode *TrieNode_Alloc( const char *name )
{
    g_nodeCount++;
    
	TrieNode *node = (TrieNode*)malloc(sizeof(TrieNode));
	memset( node, 0, sizeof(TrieNode) );
	//strcpy( node->name, name );
	return node;
}

void TrieNode_Print( TrieNode *curr, int depth )
{
	const char *spc = "...........................";
	char *indent = (char *)(spc + (strlen(spc)-(depth)) );

    const char *leaf = "";
    if (curr->numEdges==0) {
        leaf = " --> *";
    }
    printf("%s%s%s #%08X\n", indent, curr->label, leaf, curr->hashcode );
	for (int i=0; i < curr->numEdges; i++) {
        TrieNode_Print( curr->edge[i], depth+1 );
	}
}

void TrieNode_Label( TrieNode *curr, uint32_t *counter )
{
    curr->nodeId = (*counter)++;
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_Label( curr->edge[i], counter );
    }
}

void TrieNode_Graphviz( TrieNode *curr, FILE *fp )
{
    //fprintf( fp, " %d [label=\"%s %X\"];\n", curr->nodeId,curr->label, curr->hashcode );
    fprintf( fp, " %d [label=\"%s\"];\n", curr->nodeId,curr->label );
    for (int i=0; i < curr->numEdges; i++) {
        fprintf( fp, "  %d -> %d\n", curr->nodeId, curr->edge[i]->nodeId );
    }
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_Graphviz( curr->edge[i], fp );
    }
}

// Dumps the word list
void TrieNode_WordList( TrieNode *curr, char *prefix )
{
    char buff[MAX_WORD_LENGTH];
    strcpy( buff, prefix );
    strcat( buff, curr->label );
    
    if (curr->numEdges == 0) {
        printf(">>>%s\n", buff );
    }
    
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_WordList( curr->edge[i], buff );
    }
}

void TrieNode_Insert( TrieNode *root, char *origWord )
{
    //printf("---- Insert: %s ----\n", origWord  );
    
    // add a sentinal to the word
    char word[MAX_WORD_LENGTH];
    sprintf( word, "%s*", origWord );
    
    char prefix[MAX_WORD_LENGTH] = {};
    TrieNode *node = TrieNode_Lookup( root, word, prefix );
    char *suffix = word + strlen(prefix);
    
    //printf("Found node '%s' [%s][%s] node '%s' has %d edges\n", node->label, prefix, suffix, node->label, node->numEdges );
    
    // Find the longest prefix
    int longestSharedPrefixLen = 0;
    int longestSharedPrefixIndex = 0;
    for (int i=0; i < node->numEdges; i++) {
        int sharedPrefixLen = 0;
        char *chA = suffix;
        char *chB = node->edge[i]->label;
//        printf("chA [%s] chB [%s]\n", chA, chB );
        while (*chA && *chB && (*chA==*chB)) {
            sharedPrefixLen++;
            chA++;
            chB++;
        }
        
        if (sharedPrefixLen > longestSharedPrefixLen) {
            longestSharedPrefixIndex = i;
            longestSharedPrefixLen = sharedPrefixLen;
        }
    }
//    printf("LongestSharedPrefix: %d..\n", longestSharedPrefixLen );
    
    if (longestSharedPrefixLen == 0) {
//        printf ("Make new edge...\n");
        
        // If this is a leaf, add a dummy to end the word
        if ((node!=root) && (node->numEdges==0)) {
//            printf ("Adding leaf\n");
            TrieNode *leafEdge = TrieNode_Alloc( "leaf" );
            node->edge[node->numEdges++] = leafEdge;
        }
        
        TrieNode *newEdge = TrieNode_Alloc( "nosplit" );
        strcpy(newEdge->label, suffix );
        node->edge[node->numEdges++] = newEdge;
    } else {
//        printf("Split Edge ...\n");
        TrieNode *splitEdge = TrieNode_Alloc( "split1" );

        // If the whole split edge is shared, add a leaf node to ensure this word is still counted
//        printf("Len suffix %zu longest %d\n", strlen(suffix), longestSharedPrefixLen);
//        if ( (int)strlen(suffix) == longestSharedPrefixLen) {
//            printf ("Adding leaf\n");
//            TrieNode *leafEdge = TrieNode_Alloc( "leaf" );
//            splitEdge->edge[splitEdge->numEdges++] = leafEdge;
//        }
        strncpy(splitEdge->label, suffix, longestSharedPrefixLen );
        
        TrieNode *oldEdge = node->edge[longestSharedPrefixIndex];
        node->edge[longestSharedPrefixIndex] = splitEdge;
        
        char buff[MAX_WORD_LENGTH];
        strcpy( buff, oldEdge->label + longestSharedPrefixLen );
        strcpy( oldEdge->label, buff );
        
        splitEdge->edge[splitEdge->numEdges++] = oldEdge;
        
        TrieNode *newEdge = TrieNode_Alloc( "split2" );
        strcpy( newEdge->label, suffix + longestSharedPrefixLen );
        splitEdge->edge[splitEdge->numEdges++] = newEdge;
    }
    
}

int nodeLabelCmp( const void *a,const void *b ) {
    TrieNode *nodeA = *((TrieNode**)a);
    TrieNode *nodeB = *((TrieNode**)b);
    const char * strA = nodeA->label;
    const char * strB = nodeB->label;
    return strcmp( strA, strB );
}

void TrieNode_NormalizeEdges( TrieNode *curr )
{
    if (curr->numEdges > 0) {
        qsort( curr->edge, curr->numEdges, sizeof(TrieNode*), nodeLabelCmp );
    }
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_NormalizeEdges( curr->edge[i]);
    }
}

void TrieNode_SplitLongNodes( TrieNode *curr, int maxLen )
{
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode *edge = curr->edge[i];
        //printf("split '%s' len %zu\n", edge->label, strlen( edge->label ) );
        if ((int)strlen(edge->label) > maxLen) {
            char prefix[MAX_WORD_LENGTH] = {};
            char suffix[MAX_WORD_LENGTH] = {};
            strncpy( prefix, edge->label, maxLen );
            strcpy( suffix, edge->label + maxLen );
          //  printf("SPLIT EDGE %s -> %s|%s\n", curr->label, prefix, suffix );
            
            strcpy( edge->label, suffix );
            
            // Replace the edge we split
            TrieNode *splitEdge = TrieNode_Alloc( "splitedge" );
            strcpy( splitEdge->label, prefix );
            splitEdge->edge[splitEdge->numEdges++] = edge;
            curr->edge[i] = splitEdge;
        }
    }
    
    // Split any child edges (including the one we just created)
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_SplitLongNodes( curr->edge[i], maxLen );
    }
}

struct TrieStats
{
    int labelLengthCount[5];
    int edgeCount[27];
    uint32_t nodeCount;
};

void TrieNode_GatherStats( TrieNode *curr, TrieStats *stats ) {
    
    stats->nodeCount++;
    int cc = curr->numEdges;
    if (cc>26) cc=26;
    stats->edgeCount[cc]++;
    
    int llen = strlen(curr->label);
    stats->labelLengthCount[llen]++;
    
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_GatherStats( curr->edge[i], stats );
    }
}

int calcNumPackNodes(int numEdges ) {
    int extraEdges = 0;
    if (numEdges > EDGE_LIMIT) {
        extraEdges = numEdges - EDGE_LIMIT;
    }
    // 1 for the edge node, plus extra if it needs more space for edges
    int numPackNodes = 1 + (((sizeof(u32)*(extraEdges)) + (sizeof(WordListNode)-1)) / sizeof(WordListNode));
    return numPackNodes;
}

// Assumes buffer is prefilled with zeros
WordListNode *_baseNode;
void PackWordList( TrieNode *node, WordListNode *packNode, WordListNode **nextPackNode )
{
    // Pack this node
    strncpy( packNode->label, node->label, 4 );
    packNode->numEdges = node->numEdges;
    
    // Now allocate space for our edge node
    for (int i=0; i < node->numEdges; i++) {
        if (node->edge[i]->packNode == NULL) {
            node->edge[i]->packNode = *nextPackNode;
            int numPackNodes = calcNumPackNodes(node->edge[i]->numEdges );
            //printf("Edges %d extra pack nodes: %d\n", numEdges, numPackNodes );
            *nextPackNode += numPackNodes;
        }
    }
    
    // Now pack child nodes
    for (int i=0; i < node->numEdges; i++) {
        PackWordList( node->edge[i], node->edge[i]->packNode, nextPackNode );
    }
}

void AssignWordlistIndices( WordListNode *base, TrieNode *curr, int *highestIndex )
{
    WordListNode *packNode = curr->packNode;
//    printf("FixupWordlistIndices: %d '%s' (packed %d) edges %d\n",
//           curr->nodeId, curr->label,
//           packNode - base, packNode->numEdges );
    for (int i=0; i < packNode->numEdges; i++) {
        // edge index for pack node
//        printf ("curr numedges %d pacnode numEdgs %d -- i %d\n",
//                curr->numEdges, packNode->numEdges, i );
        int ndx = curr->edge[i]->packNode - base;
        packNode->edge[i] = ndx;
        if (ndx > *highestIndex) {
            *highestIndex = ndx;
        }
    }
    
    for (int i=0; i < packNode->numEdges; i++) {
        AssignWordlistIndices( base, curr->edge[i], highestIndex );
    }
}

// Update the hash and count
uint32_t TrieNode_UpdateHash( TrieNode *curr )
{
    // Hash is kind of dumb, just djb-hash the label and then add all of the child nodes
    uint32_t hash = 5381;
    for (char *ch=curr->label; *ch; ch++) {
        hash = ((hash << 5) + hash) + *ch;
    }
    curr->nodeCount = 1; // count ourself
    for (int i=0; i < curr->numEdges; i++) {
        uint32_t edgeHash = TrieNode_UpdateHash( curr->edge[i] );
        curr->nodeCount += curr->edge[i]->nodeCount;
        hash += edgeHash;
    }
    curr->hashcode = hash;
    return hash;
}

void TrieNode_ClearVisited( TrieNode *curr ) {
    curr->visited = false;
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode_ClearVisited( curr->edge[i] );
    }
}

uint32_t TrieNode_CountDupes( TrieNode *curr, TrieNode *target ) {
    
    curr->visited = true;
    uint32_t count = 0;
    
    for (int i=0; i < curr->numEdges; i++) {
        if (!curr->edge[i]->visited) {
            count += TrieNode_CountDupes( curr->edge[i], target );
        }
    }
    
    
    if (curr->hashcode == target->hashcode) {
        count++;
        //printf("CountDupes: %d ( %d/%0X, %d/%0X)\n", count, curr->nodeId, curr->hashcode, target->nodeId,target->hashcode );
    }
    
    
    return count;
}

TrieNode *TrieNode_FindMostDuplicated( TrieNode *root, TrieNode *curr ) {
    
    static int blah = 0;
    blah++;
    if ((blah % 100) == 0) {
        printf("]] %d\n", blah );
    }
    // How much is this node duplicated?
    TrieNode_ClearVisited(root);
    uint32_t bestCount = TrieNode_CountDupes( root, curr );
    TrieNode *bestNode = curr;
    
    //printf("Node %d (%s) dupes %d\n", curr->nodeId, curr->label, bestCount );
    for (int i=0; i < curr->numEdges; i++) {
        //uint32_t edgeCount = TrieNode_CountDupes( root, curr->edge[i] );
        TrieNode *edgeMostDup = TrieNode_FindMostDuplicated( root, curr->edge[i] );
      //  printf("EdgeCount %d\n", edgeMostDup->dupeCount );
        if (edgeMostDup->dupeCount > bestCount) {
            bestNode = edgeMostDup;
            bestCount = edgeMostDup->dupeCount;
        }
    }
    
    bestNode->dupeCount = bestCount;
    return bestNode;
}

void DoCountNodes( CountTableEntry **countTable, TrieNode * curr)
{
    CountTableEntry *ent = CountTableEntry_Lookup( countTable, curr, true );
    if (!ent) {
        printf("WARN: Lookup failed???\n");
    }
    
    // Count ourself
    curr->visited = true;
    ent->nodeCount++;
    //printf("DoCountNodes: node %d count is now %d\n", ent->node->nodeId, ent->nodeCount );
    
    for (int i=0; i < curr->numEdges; i++) {
        TrieNode *edgeNode = curr->edge[i];
        if (!edgeNode->visited) {
            DoCountNodes( countTable, edgeNode );
        }
    }
}

TrieNode *TrieNode_FindMostDuplicated2( CountTableEntry **countTable, TrieNode *root ) {
    
    // reset all counts in counttable
    for (int i=0; i < HASHTABLESIZE; i++) {
        CountTableEntry *ent = countTable[i];
        while (ent) {
            ent->nodeCount = 0;
            ent = ent->next;
        }
    }
    
    // Traverse and count nodes
    TrieNode_ClearVisited(root );
    DoCountNodes( countTable, root );
    
    // Get the node with the higest count
    CountTableEntry *bestEntry = NULL;
    for (int i=0; i < HASHTABLESIZE; i++) {
        CountTableEntry *ent = countTable[i];
        while (ent) {
            if ((!bestEntry) || (ent->nodeCount > bestEntry->nodeCount)) {
                bestEntry = ent;
            }
            ent = ent->next;
        }
    }
    
    if (bestEntry) {
        bestEntry->node->dupeCount = bestEntry->nodeCount;
        return bestEntry->node;
    } else {
        return NULL;
    }
}



int TrieNode_ReplaceSubtree( TrieNode *curr, TrieNode *replacement ) {
    
    int count = 0;
    for (int i=0; i < curr->numEdges; i++) {
        count += TrieNode_ReplaceSubtree( curr->edge[i], replacement );
        
        // self-replacment is harmless...
        if (curr->edge[i]->hashcode == replacement->hashcode) {
            curr->edge[i] = replacement;
            count++;
        }
    }
    return count;
}

// ======================================================================
// Word Tool main
// ======================================================================
int main( int argc, char *argv[] ) 
{  
    printf("...\n");
        if (argc < 3) {
        printf("Usage: wordtool <wordlist.txt> <wordfile.dat>\n");
        return 1;
    }
    
    const char *wordList = argv[1];
    const char *wordDataFile = argv[2];

    TrieNode *root = TrieNode_Alloc( "root" );

#if DEBUG_GRAPH
   // dot dbgtrie.dot -Tpng -o dbgtrie.png
    FILE *fpGraph = fopen("/Users/joeld/oprojects/crossroads/dbgtrie.dot", "wt" );
    fprintf( fpGraph,
            "digraph blarg {\n"
            "node [shape = circle];\n"
            );
#endif
    
    int count = 0;
    int rawsize = 0;

#if 1
    TrieNode_Insert( root, (char *)"team" );
    TrieNode_Insert( root, (char *)"test" );
    TrieNode_Insert( root, (char *)"teams" );
    TrieNode_Insert( root, (char *)"aardvark" );
    TrieNode_Insert( root, (char *)"aardwulf" );
    TrieNode_Insert( root, (char *)"zoo" );
    TrieNode_Insert( root, (char *)"zoos" );
    TrieNode_Insert( root, (char *)"zookeeper" );
    TrieNode_Insert( root, (char *)"zookeepers" );

    TrieNode_Insert( root, (char *)"cling" );
    TrieNode_Insert( root, (char *)"clinger" );
    TrieNode_Insert( root, (char *)"clings" );
    
    TrieNode_Insert( root, (char *)"claw" );

    TrieNode_Insert( root, (char *)"zomg" );
    TrieNode_Insert( root, (char *)"zinger" );
    TrieNode_Insert( root, (char *)"zings" );
    TrieNode_Insert( root, (char *)"zing" );

    TrieNode_Insert( root, (char *)"troubador" );

    TrieNode_Insert( root, (char *)"bank" );
    TrieNode_Insert( root, (char *)"bing" );
    TrieNode_Insert( root, (char *)"bings" );
    TrieNode_Insert( root, (char *)"binger" );

    TrieNode_Insert( root, (char *)"rodeo" );
    TrieNode_Insert( root, (char *)"ring" );
    TrieNode_Insert( root, (char *)"ringer" );
    TrieNode_Insert( root, (char *)"rings" );

    TrieNode_SplitLongNodes( root, 4 );
    TrieNode_NormalizeEdges( root );
    TrieNode_UpdateHash( root );
    TrieNode_Print( root, 0 );
    TrieNode_PrintWords( root, "" );
    // TrieNode_Insert( root, (char *)"abaci" );
    // TrieNode_Print( root, 0 );
    
    TrieNode_WordList( root, "");
    
    
    
#else
    
    
    int wlCount[MAX_WORD_LENGTH] = {};

    // Read the word list
    FILE *fp = fopen( wordList, "rt");
    if (!fp) {
    	printf("ERROR Could not open word list '%s'\n", wordList );
    	return 1;
    }


    char line[1024];
    while (!feof(fp)) {
    	fgets( line, 1024, fp );
    	

    	// Strip CR and newlines, also strip % which the word list
    	// uses to indicate plurals
    	char *ch = line + strlen(line)-1;
    	while ((ch > line) && ((*ch=='\n') || (*ch=='\r') || (*ch=='%') ) ) {
    		*ch = '\0';
    		ch--;
    	}

    	
    	int len = strlen(line);
    	if (len >= MAX_WORD_LENGTH) {
    		printf("Word '%s' len %d, max %d\n", line, len, MAX_WORD_LENGTH );
    		return 1;
    	}
    	wlCount[len]++;

    	// TODO: sanitize words, eg. replace Qu with Q

    	TrieNode_Insert( root, line );

    	count++;
        rawsize += strlen(line) + 1;

//        if (count==1000) {
//            break;
//        }
    }
	
    TrieNode_SplitLongNodes( root, 4 );
    TrieNode_NormalizeEdges( root );
    TrieNode_UpdateHash( root );

//    TrieNode_Print( root, 0 );
    printf("%d nodes total...\n", g_nodeCount );
//    printf("-------\n");
//    //TrieNode_WordList( root, "" );
    
    printf("--- %d words total ---\n", count );
    for (int i=0; i < MAX_WORD_LENGTH; i++) {
        if (wlCount[i]>0) {
            printf("LEN: %2d - %d words.\n", i, wlCount[i] );
        }
    }
#endif
    
    uint32_t counter = 1;
    TrieNode_Label( root, &counter );
    
    CountTableEntry ** countTable = (CountTableEntry**)malloc(sizeof(CountTableEntry*)*HASHTABLESIZE );
    memset( countTable, 0, sizeof(CountTableEntry*)*HASHTABLESIZE );
    
    for (int i=0; i < 100; i++)
    {
        printf("FindMostDupes iter %d\n", i );
        //TrieNode *mostDupes = TrieNode_FindMostDuplicated( root, root );
        TrieNode *mostDupes = TrieNode_FindMostDuplicated2( countTable, root );
        printf("Most Dupes %d(%s), count %d\n", mostDupes->nodeId, mostDupes->label, mostDupes->dupeCount);
        if (mostDupes->dupeCount <= 1) {
            break;
        }
        
        int result = TrieNode_ReplaceSubtree( root, mostDupes );
        printf("Replace dupes result %d\n", result );
    }
    
#if DEBUG_GRAPH
    TrieNode_Graphviz( root, fpGraph );
    fprintf(fpGraph, "}\n");
    fclose(fpGraph );
#endif
    
    TrieStats stats = {};
    TrieNode_GatherStats( root, &stats );
    printf("Total Nodes: %d\n", stats.nodeCount );
    for (int i=0; i < 5;i++ ) {
        printf("Label %d: %d\n", i, stats.labelLengthCount[i] );
    }
    printf("----------------\n");
    int totalEdges = 0;
    for (int i=0; i < 27; i++ ) {
        totalEdges += stats.edgeCount[i];
    }
    int percentileEdges = 0;
    for (int i=0; i < 27; i++ ) {
        percentileEdges += stats.edgeCount[i];
        if (stats.edgeCount[i] > 0) {
            printf("edges %d: %d (%3.2f%%, %3.2f%% incl)\n", i, stats.edgeCount[i],
                   ((float)stats.edgeCount[i] / (float)totalEdges) * 100.0f,
                   ((float)percentileEdges/ (float)totalEdges) * 100.0f
                   );
        }
    }
    
    // Pack word data
    WordListNode *worddata = (WordListNode*)malloc(WORDBUFFSZ);
    memset( worddata, 0, WORDBUFFSZ );
    root->packNode = worddata;
    _baseNode = worddata;
    WordListNode *nextPackNode = worddata;
    nextPackNode += calcNumPackNodes(root->numEdges);
    PackWordList( root, worddata, &nextPackNode );
    //printf("After packWordList, nextPackNode is %d\n", nextPackNode - worddata );
    int numPackNodes = 0;
    AssignWordlistIndices( worddata, root, &numPackNodes );
    
    int indexSize = sizeof(uint32_t) * count;
    printf("\n\nPacked %d Words (%d bytes), %d packNodes, %zu bytes.\n",
           count, rawsize + indexSize,
           numPackNodes, numPackNodes * sizeof(WordListNode));

    // Test it
    const char *testWords[] = {
        "claw", "aardvark", "test", "rhino", "claws",
        "zzyzhags", "blarg", "teams", "troubadour",
    };
    for (int i=0; i < sizeof(testWords) / sizeof(testWords[0]); i++ ) {
        int result = WordList_Lookup( worddata, (char *)testWords[i] );
        printf("%20s ... %s\n", testWords[i], result?"FOUND":"Not Found" );
    }

    // Write output file
    FILE *fpDatafile = fopen( wordDataFile, "wb" );
    size_t result = fwrite( worddata, sizeof(WordListNode), numPackNodes, fpDatafile );
    fclose( fpDatafile);
    printf("Wrote %zu bytes to %s .\n", result*sizeof(WordListNode), wordDataFile );

}
