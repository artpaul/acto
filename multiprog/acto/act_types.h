﻿///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     The act_o Library                                         //
//                                                                                               //
//-----------------------------------------------------------------------------------------------//
// Copyright © 2007                                                                              //
//     Pavel A. Artemkin (acto.stan@gmail.com)                                                    //
// ----------------------------------------------------------------------------------------------//
// License:                                                                                      //
//     Code covered by the MIT License.                                                          //
//     The authors make no representations about the suitability of this software                //
//     for any purpose. It is provided "as is" without express or implied warranty.              //
//-----------------------------------------------------------------------------------------------//
// File: act_types.h                                                                             //
//     Общие типы для всей библиотеки.                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprogs__act_types_h__ )
#define __multiprogs__act_types_h__


namespace multiprog
{

namespace acto
{

#if defined ( _MSC_VER ) 

	/* Целые со знаком */
	typedef	__int8				int8;
	typedef	__int16				int16;
	typedef	__int32				int32;
	typedef	__int64				int64;

	/* Беззнаковые целые */
	typedef unsigned __int8		uint8;
	typedef unsigned __int16	uint16;
	typedef unsigned __int32	uint32;
	typedef unsigned __int64	uint64;
#else
	// -
#endif


// Тип уникального идентификатор для типов сообщений
typedef long        TYPEID;



///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

// -
class actor_t;


namespace core
{
	class base_t;

	struct i_handler;

	struct object_t;

	struct msg_t;

	struct package_t;

}; // namespace core

}; // namespace acto

}; // namespace multiprog

#endif // __multiprogs__act_types_h__
