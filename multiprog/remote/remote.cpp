
#include <act_user.h>

#include "client.h"

namespace acto {

namespace remote {

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Подключиться к серверу
// Вызывается на стороне клиента
actor_t connect(const char* path, unsigned int port) {
    return remote_module_t::instance()->connect(path, port);
}

void enable() {
    remote_module_t::instance()->enable_server();
}

// Зарегистрировать актера в словаре.
// Вызывается на стороне сервера
void register_actor(const actor_t& actor, const char* path) {
    remote_module_t::instance()->register_actor(actor, path);
}

} // namespace remote

} // namespace acto
