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
///////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprogs__act_core_h__ )
#define __multiprogs__act_core_h__

#include "types.h"
#include "runtime.h"

namespace acto {

namespace core {

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Инициализация
void initialize();
void initializeThread(const bool isInternal);

// -
void finalize();

void finalizeThread();

void processBindedActors();

}; // namespace core

}; // namespace acto

#endif // __multiprogs__act_core_h__
