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

#ifndef acto_h_73A5B4AE25EE4295BD7B080FBD29FFC0
#define acto_h_73A5B4AE25EE4295BD7B080FBD29FFC0

#pragma once

// Реализация делегатов
#include "generic/delegates.h"

// Обертка над API операционных систем
#include "system/platform.h"

// Ядро библиотеки
#include "core/core.h"

// Интерфейс библиотеки (пользовательский уровень)
#include "act_user.h"
// Дополнительные сервисы (пользовательский уровень)
#include "extension/services.h"

#endif // acto_h_73A5B4AE25EE4295BD7B080FBD29FFC0
