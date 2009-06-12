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

// ������ ���� ���������������� ���������
#include "config.h"

// ���������� ���������
#include "system/delegates.h"

// ������� ��� API ������������ ������
#ifdef MSWINDOWS
#	include "system/act_mswin.h"
#endif


// ������ ���� ���������������� ���������
#include "acto/act_config.h"

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
