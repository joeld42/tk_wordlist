
add_executable (tk_wordlist_sample tk_wordlist_sample.c ../tk_wordlist.c)
target_include_directories( tk_wordlist_sample PUBLIC ${CMAKE_SOURCE_DIR} )