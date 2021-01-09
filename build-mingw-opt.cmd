call clean.cmd

cd freetype
call build-mingw-opt.cmd
cd ..

g++ -c -Ofast -DWIN32 -DNDEBUG -D_WINDOWS -I.\include -I.\freetype\include -I.\freetype\include\freetype ^
.\src\hardware\timer.cpp ^
.\src\hardware\vga.cpp ^
.\src\hardware\vga_attr.cpp ^
.\src\hardware\vga_crtc.cpp ^
.\src\hardware\vga_dac.cpp ^
.\src\hardware\vga_gfx.cpp ^
.\src\hardware\vga_memory.cpp ^
.\src\hardware\vga_misc.cpp ^
.\src\hardware\vga_seq.cpp ^
.\src\ints\bios.cpp ^
.\src\ints\bios_keyboard.cpp ^
.\src\cpu\callback.cpp ^
.\src\cpu\core_normal.cpp ^
.\src\cpu\cpu.cpp ^
.\src\dos\devicePRT.cpp ^
.\src\dos\dos.cpp ^
.\src\dos\dos_classes.cpp ^
.\src\dos\dos_devices.cpp ^
.\src\dos\dos_execute.cpp ^
.\src\dos\dos_files.cpp ^
.\src\dos\dos_ioctl.cpp ^
.\src\dos\dos_memory.cpp ^
.\src\dos\dos_misc.cpp ^
.\src\dos\dos_tables.cpp ^
.\src\dos\drives.cpp ^
.\src\ints\ems.cpp ^
.\src\gui\events.cpp ^
.\src\dos\fcb_files.cpp ^
.\src\cpu\flags.cpp ^
.\src\gui\freetype.cpp ^
.\src\gui\gui.cpp ^
.\src\ints\int10.cpp ^
.\src\ints\int10_char.cpp ^
.\src\ints\int10_memory.cpp ^
.\src\ints\int10_modes.cpp ^
.\src\ints\int10_pal.cpp ^
.\src\ints\int10_put_pixel.cpp ^
.\src\ints\int10_vptable.cpp ^
.\src\hardware\iohandler.cpp ^
.\src\hardware\keyboard.cpp ^
.\src\hardware\memory.cpp ^
.\src\ints\mouse.cpp ^
.\src\cpu\paging.cpp ^
.\src\hardware\parport.cpp ^
.\src\hardware\pic.cpp ^
.\src\hardware\serialport.cpp ^
.\src\shell\shell.cpp ^
.\src\misc\support.cpp ^
.\src\gui\video.cpp ^
.\src\ints\xms.cpp ^
.\src\vDos.cpp

windres ./src/winres.rc winres.o

g++ -v -L .\freetype -o vDosPlus-mingw-opt.exe *.o -lkernel32 -luser32 -lgdi32 -lshell32 -lshlwapi -lfreetype
