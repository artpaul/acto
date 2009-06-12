///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright � 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
//---------------------------------------------------------------------------//
// File: acto.h                                                              //
//     ������� ���� ����������.                                              //
///////////////////////////////////////////////////////////////////////////////

#ifndef acto_h_scvw1123i9014opu5
#define acto_h_scvw1123i9014opu5

#pragma once

#if defined _WIN32 || _WIN64
    // ���������� ��� Windows
#   define MSWINDOWS    1
#else
#   error "Undefined target platform"
#endif

///////////////////////////////////////////////////////////////////////////////
//               ��������� ���������� ��� ������� ����������                 //
///////////////////////////////////////////////////////////////////////////////

// ������������ ���������� Microsoft, ���� �����������
#if defined( _MSC_VER )

#   if !defined _MT
#       error "Multithreaded mode not defined. Use /MD or /MT compiler options."
#   endif

    //  ���� ������� ����� - ����������
#   if defined ( _DEBUG )
#       if !defined ( DEBUG )
#           define DEBUG    1
#       endif
#   endif

// ��������� �������� ��� ������ DLL
#   ifdef MULTI_EXPORT
#       define ACTO_API         __declspec( dllexport )
    #elif MULTI_IMPORT
#       define ACTO_API         __declspec( dllimport )
#   else
#       define ACTO_API
#   endif

#endif


///////////////////////////////////////////////////////////////////////////////
//               ��������� ���������� ��� ������� ���������                  //
///////////////////////////////////////////////////////////////////////////////

#if defined ( MSWINDOWS )
#   if !defined ( _WIN32_WINNT )
        // ������� ��������� Windows 2000 ��� ����
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


// ���������� ���������
#include "system/delegates.h"

// ������� ��� API ������������ ������
#ifdef MSWINDOWS
#	include "system/act_mswin.h"
#endif


// ����� ���� ��� ����������
#include "acto/act_types.h"

// -
#include "core/act_struct.h"
// ���� ����������
#include "core/act_core.h"

// ��������� ���������� (���������������� �������)
#include "acto/act_user.h"
// �������������� ������� (���������������� �������)
#include "extension/act_services.h"

// ��������� ���������� ������������ � ��������� �������
#include "acto/methods.inl"

#endif // acto_h_scvw1123i9014opu5
