add_executable (wordtool wordtool_main.cpp ../tk_wordlist.c)
target_include_directories( wordtool PUBLIC ${CMAKE_SOURCE_DIR} )
