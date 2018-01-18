
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR msp430)
set(CMAKE_CROSSCOMPILING 1)

function(define_cache_var varname default desc)
    if(DEFINED ENV{_${varname}})
        set(tmp "$ENV{_${varname}}")
    elseif(${varname})
        set(tmp "${${varname}}")
    else()
        if(default)
            set(tmp "${default}")
        else()
            message(FATAL_ERROR "${varname} not set")
        endif()
    endif()
    set(${varname} "${tmp}" CACHE STRING "${desc}")
    set(ENV{_${varname}} "${tmp}")
endfunction()

define_cache_var(MSP430_COMPILER_PREFIX "msp430-elf-" "Compiler prefix")
define_cache_var(MSP430_MCU "" "Target MCU name")

set(CMAKE_C_COMPILER "${MSP430_COMPILER_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${MSP430_COMPILER_PREFIX}g++")

if(NOT GCC_FLAGS_SET)
    set(GCC_FLAGS "-mmcu=${MSP430_MCU}")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_FLAGS}" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_FLAGS}" CACHE STRING "" FORCE)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${GCC_FLAGS}" CACHE STRING "" FORCE)

    set(GCC_FLAGS_SET "true" CACHE INTERNAL "" FORCE)
endif()

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)
