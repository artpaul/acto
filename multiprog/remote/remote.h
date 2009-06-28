
#ifndef acto_remote_h_5723c0e88a2c48d5b3a09a246fd2ea34
#define acto_remote_h_5723c0e88a2c48d5b3a09a246fd2ea34

#include <string>

#include <act_user.h>

namespace acto {

namespace remote {


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


/// Получить ссылку на удаленный объект
ACTO_API actor_t connect(const char* path, unsigned int port);

/// Зарегистрировать объект в глобальном каталоге
ACTO_API void register_actor(const actor_t& actor, const char* path);

} // namespace remote

} // namespace acto

#endif // acto_remote_h_5723c0e88a2c48d5b3a09a246fd2ea34
