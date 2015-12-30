cmake_minimum_required(VERSION 2.7)

if(NOT ${CMAKE_ARGC} EQUAL 5)
  message(FATAL_ERROR
    "Usage: cmake -P unix2dos.cmake <input file> <output file>"
    )
endif()

file(READ ${CMAKE_ARGV3} FILE_CONTENTS)

string(REGEX REPLACE "(^|[^\r])\n($|[^\r])" "\\1\r\n\\2"
  FILE_CONTENTS_FIXED
  ${FILE_CONTENTS}
  )

file(WRITE ${CMAKE_ARGV4} ${FILE_CONTENTS_FIXED})
