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

#ifndef remote_h_23b53cd0cc8f428ba1db96556463e0f3
#define remote_h_23b53cd0cc8f428ba1db96556463e0f3

namespace acto {

class actor_ref;

namespace remote {

/** */
class remote_hook_t {
public:
    virtual void on_connecting(const char* /*target*/) { }
};

/// Получить ссылку на удаленный объект
ACTO_API actor_ref  connect(const char* path, unsigned int port);

/// Активировать возможность подключения с других хотов
ACTO_API void       enable();

/// Зарегистрировать объект в глобальном каталоге
ACTO_API void       register_actor(const actor_ref& actor, const char* path);

///
ACTO_API void    register_hook(remote_hook_t* const hook);

} // namespace remote

} // namespace acto

#endif // remote_h_23b53cd0cc8f428ba1db96556463e0f3

