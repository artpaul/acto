
#include <memory.h>
#include <stdio.h>

#include <acto.h>
#include <remote/libsocket/libsocket.h>

#define CLIENTPORT    25121

struct msg_get : public acto::msg_t {
    std::string     content;

    msg_get(const std::string c) : content(c) { }
};

/**
 */
class Server : public acto::implementation_t {
    void do_get(acto::actor_t& sender, const msg_get& msg) {
        printf("MSG GET: %s\n", msg.content.c_str());
    }

    static void read_data(int s, SOEVENT* const ev) {
        switch (ev->type) {
        case SOEVENT_CLOSED:
            printf("closed\n");
            return;
        case SOEVENT_TIMEOUT:
            printf("time out\n");
            break;
        case SOEVENT_READ:
            {
                struct D {
                    uint64_t    id;
                    uint64_t    len;
                } data;

                acto::actor_t* serv = (acto::actor_t*)ev->param;


                if (int rval = so_readsync(s, &data, sizeof(data), 5)) {
                    printf("%i\n", rval);
                    if (data.len > 0) {
                        printf("%Zu\n", data.len);
                        char* str = new char[data.len + 1];

                        so_readsync(s, str, data.len, 5);
                        str[data.len] = '\0';

                        if (data.id == 500) {
                            std::string s(str, data.len);
                            serv->send(msg_get(s));
                        }
                        delete [] str;
                    }
                }
            }
            break;
        }
        so_pending(s, SOEVENT_READ, 10, &read_data, ev->param);
    }

    static void doConnected(int s, SOEVENT* const ev) {
        switch (ev->type) {
        case SOEVENT_ACCEPTED:
            so_pending(((SORESULT_LISTEN*)ev->data)->client, SOEVENT_READ, 10, &read_data, ev->param);
            break;
        case SOEVENT_TIMEOUT:
            printf("accept timeout\n");
            break;
        }
    }

private:
    int m_fd;

public:
    Server() {
        Handler< msg_get >(&Server::do_get);

        m_fd = so_socket(SOCK_STREAM);

        if (m_fd != -1) {
            so_listen(m_fd, inet_addr("127.0.0.1"), CLIENTPORT,  5, &doConnected, &self);
        }
        else
            printf("create error\n");
    }
};


/**
 */
class Gate : public acto::implementation_t {
    void do_send_get(acto::actor_t& sender, const msg_get& msg) {
        if (m_fd != 0) {
            uint64_t    id = 500;

            so_sendsync(m_fd, &id, sizeof(id));
            id = msg.content.size();
            so_sendsync(m_fd, &id, sizeof(id));
            so_sendsync(m_fd, msg.content.c_str(), msg.content.size());
        }
    }

private:
    int m_fd;

public:
    Gate() {
        Handler< msg_get >(&Gate::do_send_get);

        m_fd = so_socket(SOCK_STREAM);

        if (m_fd != -1) {
           if (so_connect(m_fd, inet_addr("127.0.0.1"), CLIENTPORT) != 0) {
                so_close(m_fd);
                m_fd = 0;
           }
        }
        else
            m_fd = 0;
    }
};

inline acto::actor_t connect(const std::string name) {
    if (name == "Server") {
        return acto::instance< Gate >();
    }
    return acto::actor_t();
}

/**
 */
class Client : public acto::implementation_t {
public:
    Client() {
        acto::actor_t serv = connect("Server");

        if (serv.assigned()) {
            serv.send< msg_get >("test message data via message");
        }
    }
};

int main(int argc, char* argv[]) {
    acto::startup();
    so_init();

    acto::actor_t serv = acto::instance< Server >();
    acto::actor_t cl   = acto::instance< Client >();

    acto::join(serv);

    so_terminate();
    acto::shutdown();
    return 0;
}
