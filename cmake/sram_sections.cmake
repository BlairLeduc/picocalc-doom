# sram_sections.cmake — the gate's third lever (design.md §16 M5): Doom's
# column and span drawers in SRAM, with no patch to upstream (§5.3).
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
# SPDX-License-Identifier: GPL-2.0-or-later

foreach(var OBJDIR OBJCOPY FILES FUNCS)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "sram_sections.cmake: ${var} not given")
    endif()
endforeach()

string(REPLACE "," ";" FILES "${FILES}")
string(REPLACE "," ";" FUNCS "${FUNCS}")

set(renames)
foreach(fn ${FUNCS})
    list(APPEND renames --rename-section .text.${fn}=.time_critical.${fn})
endforeach()

foreach(name ${FILES})
    file(GLOB_RECURSE objs "${OBJDIR}/${name}.o" "${OBJDIR}/${name}.obj")
    list(LENGTH objs n)
    if (NOT n EQUAL 1)
        message(FATAL_ERROR "sram_sections.cmake: ${n} objects named ${name} under ${OBJDIR}")
    endif()
    execute_process(COMMAND ${OBJCOPY} ${renames} ${objs} RESULT_VARIABLE rc)
    if (NOT rc EQUAL 0)
        message(FATAL_ERROR "sram_sections.cmake: objcopy failed on ${objs}")
    endif()
endforeach()
