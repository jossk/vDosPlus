call clean.cmd 

cl /nologo /c /O2 /MP /W3 /Qpar /GS- /guard:cf- /GA /MT /EHsc /DWIN32 /DNDEBUG /D_WINDOWS /DFT2_BUILD_LIBRARY /D_CRT_SECURE_NO_DEPRECATE ^
/Iinclude /Iinclude\freetype ^
.\src\autofit\autofit.c ^
.\src\bdf\bdf.c ^
.\src\cff\cff.c ^
.\src\base\ftbase.c ^
.\src\base\ftbitmap.c ^
.\src\cache\ftcache.c ^
.\src\base\ftfstype.c ^
.\src\base\ftgasp.c ^
.\src\base\ftglyph.c ^
.\src\gzip\ftgzip.c ^
.\src\base\ftinit.c ^
.\src\lzw\ftlzw.c ^
.\src\base\ftstroke.c ^
.\src\base\ftsystem.c ^
.\src\smooth\smooth.c ^
.\src\base\ftbbox.c ^
.\src\base\ftbdf.c ^
.\src\base\ftcid.c ^
.\src\base\ftgxval.c ^
.\src\base\ftmm.c ^
.\src\base\ftotval.c ^
.\src\base\ftpatent.c ^
.\src\base\ftpfr.c ^
.\src\base\ftsynth.c ^
.\src\base\fttype1.c ^
.\src\base\ftdebug.c ^
.\src\base\ftwinfnt.c ^
.\src\pcf\pcf.c ^
.\src\pfr\pfr.c ^
.\src\psaux\psaux.c ^
.\src\pshinter\pshinter.c ^
.\src\psnames\psmodule.c ^
.\src\raster\raster.c ^
.\src\sfnt\sfnt.c ^
.\src\truetype\truetype.c ^
.\src\type1\type1.c ^
.\src\cid\type1cid.c ^
.\src\type42\type42.c ^
.\src\winfonts\winfnt.c

lib /nologo /out:freetype.lib *.obj
