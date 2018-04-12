# tk_wordlist
Simple API and packed datafile to lookup words for games, spellcheck, etc.

## Introduction

`tk_wordlist` is a compact encoding of a list of words that can be looked up quickly. This project
contains a tiny C API to lookup the words from a pre-packed data structure, and a tool to pack the
words from a text file. The packed data is compact and can be looked up directly, it's intended to
`fread` the data directly into memory and lookup from there. With the included dictionary that I am
using has 81537 words, and would be 1115kb unpacked (including an list of pointers to index 
the words), the packed data is 349kb, and the lookup is at least as fast as a binary search.

This was created as part of making a word game, I am open sourcing in case it's useful to other folks
who want to make word games or something like that. It's probably overkill for most things, a simple 
uncompressed list and a binary search would work fine, but it was a fun "yak-shaving" exercise to 
work to see how compact I could get this. It might be more useful on a larger data set such as the 3M
word2vec sample (but I haven't tested it on anything that large). Note that if you're using it
on a larger dataset you'll have to tweak some things, see the header file for details...

## API Usage

The API leaves it up to you to load the packed data from file or whatever resource your project uses. 
Once you have the data, cast the buffer to a WordListNode array (`WordListNode*`) and you can lookup 
from that. There is no other initialization and the code doesn't do any heap allocation.

	WordListData *worddata = (WordListData*)data;
    int result = WordList_Lookup( worddata, "scalawag" );

This will return 1 if the word is found in the dictionary and 0 otherwise. 

That's pretty much all there is to it, but there's also an API to enumerate all of the words in
the dataset. To use that:

    WordList_Enumerator ee = WordList_MakeEnumerator( worddata );
    char *word;
	while ((word = WordList_NextWord( &ee )) ) {
		printf("%s\n", word );
	}

There is no API to get metadata like the number of words or size of the dataset, as that information 
is not stored in the packed data since it's not needed for lookup. 

The lookup is very small and straightforward, and shouldn't be to hard if you need to modify it for
partial lookup or autocomplete or something like that.

## Sample Word List

I'm using the "2of12inf.txt" [word list from Alan Beale](http://wordlist.aspell.net/12dicts-readme/) which
is an amazing resource for word puzzles and games. Huge thanks to Alan for compiling and releasing these lists.

The packed version of this list is included in the `datafiles/` directory, you can use this directly without 
having to worry about building and packing the list yourself.


## Word Packing Tool

The packing tool is pretty messy and probably buggy, and it (intentionally) leaks memory. This shouldn't
be a problem because it only needs to be run beforehand to prepare the datafile, and not intended to
ship to the runtime. It's reasonably fast, but might need some more optimization for very large lists.

The wordtool code is pretty messy, apologies if you have to actually do anything with it, but I figured it was
better to release it as-is than letting the cleanup be a roadblock.

## How it Works

It's basically a [Radix Trie](https://en.wikipedia.org/wiki/Radix_tree) with four (or less) character
blocks at each node.

### Building the Trie

The starting trie is constructed by splitting the words into a tree. If you follow any path down the
tree, concatenating the strings (called "labels") as you go, when you hit a terminator ("*") then you have
a valid word. (The terminator character makes it easier to build the tree lets us end a
word in the middle, e.g. "zoo" and "zookeeper".)

![The Starting Trie](imgs/trie_orig.png?raw=true "Starting Trie")

### Splitting

Next, we split any node with a label that is longer than four characters. This lets us 
build the packed data with each node having a fixed length for the label. With a large 
word list, most of the nodes are smaller than this so this doesn't create too many extra nodes.

![Trie After Splitting](imgs/trie_split4.png?raw=true "After Splitting")

### Duplicate Subtrees

English words have a lot of common suffixes. Especially things like plurals where very many words are the
same with only an 's' or 'es' added, and common suffixes like 'ing', and 'ed'. To compact these, we identify 
duplicate subtrees and collapse them. This turns the tree into a DAG, but it can be traversed identically as
before. 

![Collapse Duplicate Subtrees](imgs/trie_final.png?raw=true "Collapsed Duplicate Subtrees")

This is where most of the savings comes from. We search for duplicated subtrees by hashing all the subtrees, 
and then collapse them. This isn't very clever and it just keeps trying, replacing whatever we find is the 
most duplicated subtree, until it can't find any more duplicates. 

## Future work

I'm not planning on doing too much more with this, other than using it in some games. Some cleanup I hope to do:

* Remove the few cstdlib string functions used in tk_wordlist.c
* Convert to a proper "header only" library

Please send any comments or questions to `joeld42@gmail.com`, and let me know if you use this for anything.
I love word games and would love to see more of them.

If you want to check out my games, or the check out the word game I'm using this for, have a look 
at [www.tapnik.com](http://www.tapnik.com), which will be announced soonly. 

