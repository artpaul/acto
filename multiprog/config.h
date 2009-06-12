///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
//---------------------------------------------------------------------------//
// File: config.h                                                            //
//     Конфигурационный файл для библиотеки acto.                            //
///////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprog__config_h__ )
#define __multiprog__config_h__

#pragma once

#if defined _WIN32 || _WIN64
    // Реализация для Windows
#   define MSWINDOWS    1
#else
#   error "Undefined target platform"
#endif

///////////////////////////////////////////////////////////////////////////////
//               НАСТРОЙКА БИБЛИОТЕКИ ПОД ТЕКУЩЙИ КОМПИЛЯТОР                 //
///////////////////////////////////////////////////////////////////////////////

// Используется компилятор Microsoft, либо совместимый
#if defined( _MSC_VER )

#   if !defined _MT
#       error "Multithreaded mode not defined. Use /MD or /MT compiler options."
#   endif

    //  Если текущий режим - отладочный
#   if defined ( _DEBUG )
#       if !defined ( DEBUG )
#           define DEBUG    1
#       endif
#   endif

// Директивы экспорта для сборки DLL
#   ifdef MULTI_EXPORT
#       define MULTI_API        __declspec( dllexport )
    #elif MULTI_IMPORT
#       define MULTI_API        __declspec( dllimport )
#   else
#       define MULTI_API
#   endif

#endif


///////////////////////////////////////////////////////////////////////////////
//               НАСТРОЙКА БИБЛИОТЕКИ ПОД ЦЕЛЕВУЮ ПЛАТФОРМУ                  //
///////////////////////////////////////////////////////////////////////////////

#if defined ( MSWINDOWS )
#   if !defined ( _WIN32_WINNT )
        // Целевая платформа Windows 2000 или выше
#       define _WIN32_WINNT     0x0500
#   endif
    // -
#   include <windows.h>

#   define TLS_VARIABLE     __declspec (thread)

#elif
#   define TLS_VARIABLE
#endif


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cassert>
#include <ctime>
#include <deque>
#include <exception>
#include <list>
#include <map>
#include <new>
#include <queue>
#include <set>
#include <string>
#include <vector>


#endif // __multiprog__config_h__
