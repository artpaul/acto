///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     The act_o Library                                         //
//                                                                                               //
//-----------------------------------------------------------------------------------------------//
// Copyright © 2007 - 2008                                                                       //
//     Pavel A. Artemkin (acto.stan@gmail.com)                                                   //
// ----------------------------------------------------------------------------------------------//
// License:                                                                                      //
//     Code covered by the MIT License.                                                          //
//     The authors make no representations about the suitability of this software                //
//     for any purpose. It is provided "as is" without express or implied warranty.              //
//-----------------------------------------------------------------------------------------------//
// File: act_services.h                                                                          //
//     Дополнительные сервисы.                                                                   //
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprog__act_services_h__ )
#define __multiprog__act_services_h__


namespace multiprog {

namespace acto {

// Дополнительные сервисы
namespace services {

// -
void finalize();
// -
void initialize();


// Desc: Базовый класс для всех служб
class ACTO_API service_t {
};

///////////////////////////////////////////////////////////////////////////////
// Desc: 
//    Обеспечивает посылку уведомлений актерам через 
//    заданные промежутки времени. 
class ACTO_API timer_t : public service_t {
    friend void finalize();

public:
	 timer_t();
	~timer_t();

public:
	// Отключить таймер для указанного актера
	void cancel(acto::actor_t& actor);

	// Установить таймер для указанного актера
	void setup(acto::actor_t& actor, const int time, const bool once);

private:
	// -
	actor_t		m_actor;
};


// Сервис таймера
extern timer_t  timer;


///////////////////////////////////////////////////////////////////////////////////////////////////
//                        РЕАЛИЗАЦИЯ ВСТРАИВАЕМЫХ И ШАБЛОННЫХ МЕТОДОВ                            //
///////////////////////////////////////////////////////////////////////////////////////////////////

}; // namespace services

}; // namespace acto

}; // namespace multiprog

#endif // __multiprog__act_services_h__
