///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     The act-o Library                                         //
//                                 (part of multiprog library)                                   //
//-----------------------------------------------------------------------------------------------//
// Copyright © 2007 - 2008                                                                       //
//     Pavel A. Artemkin (acto.stan@gmail.com)                                                   //
// ----------------------------------------------------------------------------------------------//
// License:                                                                                      //
//     Code covered by the MIT License.                                                          //
//     The authors make no representations about the suitability of this software                //
//     for any purpose. It is provided "as is" without express or implied warranty.              //
//-----------------------------------------------------------------------------------------------//
// File: act_o.h                                                                                 //
//     Главный файл библиотеки.                                                                  //
///////////////////////////////////////////////////////////////////////////////////////////////////


#if !defined( __multiprog__act_o_h__ )
#define __multiprog__act_o_h__

#pragma once

// Первым идет конфигурационный заголовок
#include "act_config.h"

// Общие типы для библиотеки
#include "act_types.h"

// -
#include "act_struct.h"
// Ядро библиотеки
#include "act_core.h"

// Интерфейс библиотеки (пользовательский уровень)
#include "act_user.h"
// Дополнительные сервисы (пользовательский уровень)
#include "act_services.h"

// Подключить предопределенные типы сообщений
#include "messages.inl"

// Включение реализации встраиваемых и шаблонных методов
#include "methods.inl"


#endif // __multiprog__act_o_h__