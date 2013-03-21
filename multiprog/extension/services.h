///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2013                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
//---------------------------------------------------------------------------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef acto_services_h_86A045F6D3CC4790A6E798C909C88279
#define acto_services_h_86A045F6D3CC4790A6E798C909C88279

#include <act_user.h>
#include <util/system/platform.h>

namespace acto {


// Дополнительные сервисы
namespace services {

#if !defined (ACTO_WIN)

// -
inline void finalize() {
    ;
}

// -
inline void initialize() {
    ;
}

#else

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
	void cancel(acto::actor_ref& actor);

	// Установить таймер для указанного актера
	void setup(acto::actor_ref& actor, const int time, const bool once);

private:
	// -
	actor_ref   m_actor;
};


// Сервис таймера
extern timer_t  timer;

#endif // ACTO_WIN

}; // namespace services

}; // namespace acto

#endif // acto_services_h_86A045F6D3CC4790A6E798C909C88279
