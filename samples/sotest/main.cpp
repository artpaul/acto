
#include <memory.h>
#include <stdio.h>

#include <acto.h>

#define CLIENTPORT    25121

struct msg_get : public acto::msg_t {
    std::string     content;

    msg_get(const std::string c) : content(c) { }

public:
    class metainfo_t {
    public:
        metainfo_t() {
            printf("serializer_t\n");
        }

        void read(msg_get* const msg, void* s) {
            // -
        }
    };
};

struct msg_answer : public acto::msg_t {
};


acto::message_class_t< msg_get, msg_get::metainfo_t >    msg_get_class;


/**
 */
class Server : public acto::implementation_t {
    void do_get(acto::actor_t& sender, const msg_get& msg) {
        printf("MSG GET: %s\n", msg.content.c_str());

        sender.send< msg_answer >();
    }

public:
    Server() {
        Handler< msg_get >(&Server::do_get);
    }
};

/**
 */
class Client : public acto::implementation_t {
    void do_answer(acto::actor_t& sender, const msg_answer& msg) {
        printf("MSG ANSWER:\n");
    }

public:
    Client() {
        Handler< msg_answer >(&Client::do_answer);

        acto::actor_t serv = acto::remote::connect("acto://127.0.0.1/server", CLIENTPORT);

        if (serv.assigned()) {
            serv.send< msg_get >("test message data via message");
        }
    }
};

int main(int argc, char* argv[]) {
    acto::startup();

    acto::actor_t serv = acto::instance< Server >();

    acto::remote::register_actor(serv, "server");


    acto::actor_t cl = acto::instance< Client >();

    acto::join(serv);

    acto::shutdown();
    return 0;
}
