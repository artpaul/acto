An actor-based framework for C++. Actors are light-weight objects, and they interact fully asynchronously with messages.

# Example

```cpp
#include <acto.h>

static constexpr int BALLS = 1000;
static constexpr int DURATION = 2000;
static constexpr int PLAYERS = 10000;

struct msg_start {
  int balls;

  msg_start(int balls_)
    : balls(balls_) {
  }
};

struct msg_finish { };

struct msg_ball { };

class Player : public acto::actor {
public:
  Player() {
    // Send a ball back toward the wall.
    actor::handler<msg_ball>(
      [](acto::actor_ref sender, const msg_ball& msg) { sender.send(msg); });
  }
};

class Wall : public acto::actor {
public:
  Wall() {
    actor::handler<msg_ball>(&Wall::do_ball);
    actor::handler<msg_finish>(&Wall::do_finish);
    actor::handler<msg_start>(&Wall::do_start);

    for (int i = 0; i < PLAYERS; i++)
      players_[i] = acto::spawn<Player>();
  }

  ~Wall() {
    for (int i = 0; i < PLAYERS; i++)
      // Actors should be destroyed explicitly.
      acto::destroy(players_[i]);
  }

private:
  void do_ball(acto::actor_ref, msg_ball msg) {
    players_[(rand() % PLAYERS)].send(std::move(msg));
  }

  void do_finish(acto::actor_ref, const msg_finish&) {
    actor::die();
  }

  void do_start(acto::actor_ref, const msg_start& msg) {
    for (int i = 0; i < msg.balls; i++)
      players_[(rand() % PLAYERS)].send(msg_ball());
  }

private:
  acto::actor_ref players_[PLAYERS];
};

int main() {
  // Create main actor.
  acto::actor_ref wall = acto::spawn<Wall>();
  // Run ping-pong loop.
  wall.send<msg_start>(BALLS);
  // Wait a few seconds.
  acto::core::sleep(std::chrono::milliseconds(DURATION));
  // Stop the main actor.
  wall.send(msg_finish());
  // Waits until the actor stops.
  acto::join(wall);

  return 0;
}
```
