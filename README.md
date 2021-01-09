# vDosPlus
 vDosPlus 2015.11.01 fork (http://vdosplus.org/) with some enhancements:
 1. x64 build possible;
 2. Wine64 fix keyboard control keys : Up, Down etc. some mistake in source code was fixed;
 3. Default (included to binary) TTF font now is LiberationMono-Regular.ttf - more CHCP possible without external TTF;
 4. Speed up compile use precompiled headers;
 5. Fresh freetype lib now included and compiled;
 6. Directory bin included precompiled Windows x64/x32 vDosPlus.exe
 7. Include orignial sources and binaries.
 
For build need VS2015/VS2017/VS2019 command line compiler + winkit-10.0.10586.0
OR
Mingw/Clang distro from http://winlibs.com/

PS CMakeLists.txt configurer as CLion project.
