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
// File: acto.h                                                              //
//     Главный файл библиотеки.                                              //
///////////////////////////////////////////////////////////////////////////////

#ifndef acto_h_scvw1123i9014opu5
#define acto_h_scvw1123i9014opu5

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
#       define ACTO_API         __declspec( dllexport )
    #elif MULTI_IMPORT
#       define ACTO_API         __declspec( dllimport )
#   else
#       define ACTO_API
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


// Реализация делегатов
#include "system/delegates.h"

// Обертка над API операционных систем
#ifdef MSWINDOWS
#	include "system/act_mswin.h"
#endif


// Общие типы для библиотеки
#include "acto/act_types.h"

// -
#include "core/act_struct.h"
// Ядро библиотеки
#include "core/act_core.h"

// Интерфейс библиотеки (пользовательский уровень)
#include "acto/act_user.h"
// Дополнительные сервисы (пользовательский уровень)
#include "extension/act_services.h"

// Включение реализации встраиваемых и шаблонных методов
#include "acto/methods.inl"

#endif // acto_h_scvw1123i9014opu5
