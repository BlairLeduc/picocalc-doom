# check_sram.cmake — the build's check of §6.2's invariant that everything
# on the audio refill path is in SRAM (design.md §8.2).
#
# Run by CMake after the firmware links (POST_BUILD, CMakeLists.txt), with
#   -DELF=<the image> -DOBJDUMP=<arm-none-eabi-objdump> -DFUNCS=<names, comma-separated>
# From each function named, it follows every direct branch and call in
# the disassembly to the functions they reach, and fails if any of them is
# outside SRAM, or is a linker veneer (a long call from SRAM into flash).
# Calls through a pointer cannot be followed: name their targets in FUNCS.
#
# SPDX-License-Identifier: GPL-2.0-or-later

foreach(var ELF OBJDUMP FUNCS)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "check_sram.cmake: ${var} not given")
    endif()
endforeach()

string(REPLACE "," ";" todo "${FUNCS}")
set(seen)
set(bad)

while (todo)
    list(POP_FRONT todo fn)
    if (fn IN_LIST seen)
        continue()
    endif()
    list(APPEND seen ${fn})

    if (fn MATCHES "_veneer$")
        list(APPEND bad "${fn}: a veneer, a long call out of SRAM")
        continue()
    endif()

    execute_process(COMMAND ${OBJDUMP} -d --no-show-raw-insn --disassemble=${fn} ${ELF}
        OUTPUT_VARIABLE dis RESULT_VARIABLE rc ERROR_QUIET)
    if (NOT rc EQUAL 0)
        message(FATAL_ERROR "check_sram.cmake: objdump failed on ${ELF}")
    endif()
    string(REGEX MATCH "\n([0-9a-f]+) <${fn}>:" head "${dis}")
    if (NOT head)
        list(APPEND bad "${fn}: not in the image")
        continue()
    endif()
    set(addr ${CMAKE_MATCH_1})
    if (NOT addr MATCHES "^2")
        list(APPEND bad "${fn}: at 0x${addr}, not in SRAM")
        continue()
    endif()

    # Every reference to another function: "<hex> <name>" or "<hex> <name+0x..>".
    string(REGEX MATCHALL "[0-9a-f]+ <[^>+]+(\\+0x[0-9a-f]+)?>" refs "${dis}")
    foreach(ref ${refs})
        string(REGEX MATCH "^([0-9a-f]+) <([^>+]+)" _ "${ref}")
        set(to_addr ${CMAKE_MATCH_1})
        set(to ${CMAKE_MATCH_2})
        if (to STREQUAL fn)
            continue()
        endif()
        if (NOT to_addr MATCHES "^2")
            list(APPEND bad "${fn} reaches ${to} at 0x${to_addr}, not in SRAM")
        elseif (NOT to IN_LIST seen)
            list(APPEND todo ${to})
        endif()
    endforeach()
endwhile()

if (bad)
    list(JOIN bad "\n  " text)
    message(FATAL_ERROR "check_sram.cmake: the audio refill path leaves SRAM:\n  ${text}")
endif()
list(JOIN seen ", " text)
message(STATUS "check_sram: in SRAM: ${text}")
