

#include <acto.h>
#include <generic/memory.h>

#ifdef ACTO_WIN
#   include <conio.h>
#else
#   include <stdio.h>
#endif

#define CLIENTPORT    25121

struct msg_get : public acto::msg_t {
    std::string     content;

    msg_get() { }
    msg_get(const std::string c) : content(c) { }

public:
    class metainfo_t : public acto::serializer_t {
    public:
        virtual void read(msg_t* const msg, acto::stream_t* const s) {
            msg_get* const ptr = static_cast< msg_get* >(msg);
            ui32           len = 0;

            s->read(&len, sizeof(len));
            {
                acto::generics::array_ptr< char > buf(new char[len + 1]);

                s->read(buf.get(), len);
                buf[len] = '\0';

                ptr->content = std::string(buf.get(), len);
            }
        }

        virtual void write(const msg_t* const msg, acto::stream_t* const s) {
            const msg_get* const ptr = static_cast< const msg_get* >(msg);
            // -
            ui32 len = ptr->content.size();
            s->write(&len, sizeof(len));
            s->write(ptr->content.c_str(), len);
        }
    };
};

struct msg_answer : public acto::msg_t {
public:
    class metainfo_t : public acto::serializer_t {
    public:
        virtual void read(msg_t* const msg, acto::stream_t* const s) {
            // -
        }

        virtual void write(const msg_t* const msg, acto::stream_t* const s) {
            // -
        }
    };
};

struct msg_start : public acto::msg_t {
};


acto::message_class_t< msg_get,    msg_get::metainfo_t >    msg_get_class;
acto::message_class_t< msg_answer, msg_answer::metainfo_t > msg_answer_class;

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
        terminate();
    }

    void do_start(acto::actor_t& sender, const msg_start& msg) {
        acto::actor_t serv = acto::remote::connect("acto://127.0.0.1/server", CLIENTPORT);

        if (serv.assigned()) {
            serv.send< msg_get >("test message data via message");
        }
    }

public:
    Client() {
        Handler< msg_answer >(&Client::do_answer);
        Handler< msg_start  >(&Client::do_start);
    }
};

int main(int argc, char* argv[]) {
    acto::startup();
    {
        acto::remote::enable();
        // Локальный объект
        acto::actor_t serv = acto::instance< Server >();
        // Зарегистрировать в словаре
        acto::remote::register_actor(serv, "server");

        // Эмуляция клиента
        acto::actor_t cl = acto::instance< Client >();

        cl.send< msg_start >();

        acto::join(cl);
    }
    printf("shutdown\n");
    acto::shutdown();
    return 0;
}
