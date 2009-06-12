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

// Первым идет конфигурационный заголовок
#include "config.h"

// Реализация делегатов
#include "system/delegates.h"

// Обертка над API операционных систем
#ifdef MSWINDOWS
#	include "system/act_mswin.h"
#endif


// Первым идет конфигурационный заголовок
#include "acto/act_config.h"

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
