# sram_sections.cmake — upstream functions moved to SRAM after they are
# compiled, with no patch to upstream (§5.3): the gate's third lever, Doom's
# column and span drawers (design.md §16 M5), and the audio refill path's
# callback in i_picosound.c (§8.2).
#
# Run by CMake before the firmware links (PRE_LINK, CMakeLists.txt), with
#   -DOBJDIR=<the target's object directory> -DOBJCOPY=<arm-none-eabi-objcopy>
#   -DFILES=<object names, comma-separated> -DFUNCS=<function names, comma-separated>
# It renames each function's section, .text.<name> (the SDK compiles with
# -ffunction-sections), to .time_critical.<name> in the objects named, in
# place. The SDK's linker script puts .time_critical* in .data, which crt0
# copies to SRAM at boot. Renaming a section that is not there does
# nothing, so running again on an object already renamed is harmless, and
# an object CMake recompiles is renamed again before the next link.
#
# FUNCS of ALL renames every .text section in the objects named (a C++
# file's template instances, say); the sections are listed with the
# objdump beside OBJCOPY.
#
# With -DSYMS=<old=new, comma-separated>, it also redefines those symbols
# in the objects named, so their calls to, say, memcpy go to a function of
# the port's in SRAM. Again harmless on an object already done.
#
# SPDX-License-Identifier: GPL-2.0-or-later

foreach(var OBJDIR OBJCOPY FILES FUNCS)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "sram_sections.cmake: ${var} not given")
    endif()
endforeach()

string(REPLACE "," ";" FILES "${FILES}")
string(REPLACE "," ";" FUNCS "${FUNCS}")

set(syms)
if (DEFINED SYMS)
    string(REPLACE "," ";" SYMS "${SYMS}")
    foreach(sym ${SYMS})
        list(APPEND syms --redefine-sym ${sym})
    endforeach()
endif()

foreach(name ${FILES})
    file(GLOB_RECURSE objs "${OBJDIR}/${name}.o" "${OBJDIR}/${name}.obj")
    list(LENGTH objs n)
    if (NOT n EQUAL 1)
        message(FATAL_ERROR "sram_sections.cmake: ${n} objects named ${name} under ${OBJDIR}")
    endif()
    set(fns ${FUNCS})
    if (FUNCS STREQUAL "ALL")
        string(REGEX REPLACE "objcopy([^/]*)$" "objdump\\1" objdump "${OBJCOPY}")
        execute_process(COMMAND ${objdump} -h ${objs} OUTPUT_VARIABLE heads RESULT_VARIABLE rc)
        if (NOT rc EQUAL 0)
            message(FATAL_ERROR "sram_sections.cmake: ${objdump} -h failed on ${objs}")
        endif()
        string(REGEX MATCHALL " \\.text\\.[^ \n]+" secs "${heads}")
        set(fns)
        foreach(sec ${secs})
            string(REGEX REPLACE "^ \\.text\\." "" fn "${sec}")
            list(APPEND fns ${fn})
        endforeach()
    endif()
    set(renames ${syms})
    foreach(fn ${fns})
        list(APPEND renames --rename-section .text.${fn}=.time_critical.${fn})
    endforeach()
    execute_process(COMMAND ${OBJCOPY} ${renames} ${objs} RESULT_VARIABLE rc)
    if (NOT rc EQUAL 0)
        message(FATAL_ERROR "sram_sections.cmake: objcopy failed on ${objs}")
    endif()
endforeach()
