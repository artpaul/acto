#include "acto.h"
#include "catch.hpp"

#include <iostream>
#include <vector>

TEST_CASE("Finalize library") {
  acto::shutdown();
}

TEST_CASE("Spawn actor") {
  struct A : acto::actor {
    struct M {
      int x;
    };

    A() {
    }

    A(int x)
      : x_(x) {
      actor::handler<M>(
        [this](acto::actor_ref, const M& m) { REQUIRE(m.x == x_); });
    }

  private:
    int x_;
  };

  {
    auto a = acto::spawn<A>();

    std::vector<acto::actor_ref> actors = {
      a,
      acto::spawn<A>(acto::actor_thread::shared),
      acto::spawn<A>(acto::actor_thread::shared, 3),
      acto::spawn<A>(a),
      acto::spawn<A>(a, 5),
      acto::spawn<A>(a, acto::actor_thread::shared),
      acto::spawn<A>(a, acto::actor_thread::shared, 7)};
    // Validate actors.
    for (const auto& x : actors) {
      REQUIRE(x);
    }
    // Check values.
    actors[2].send(A::M{3});
    actors[4].send(A::M{5});
    actors[6].send(A::M{7});
    // Cleanup actors.
    for (auto& x : actors) {
      acto::destroy(x);
    }
  }
  // Shutdown.
  acto::shutdown();
}

TEST_CASE("Handler with nullptr") {
  struct A : acto::actor {
    struct M { };

    A() {
      actor::handler<M>(0);
    }
  };

  A a;
}

TEST_CASE("Process binded actors at exit") {
  struct A : acto::actor {
    struct M {
      std::shared_ptr<size_t> counter;
    };

    A() {
      actor::handler<M>([](acto::actor_ref, const M& m) { (*m.counter)++; });
    }
  };

  auto counter = std::make_shared<size_t>(0);
  std::thread t([&]() {
    auto a = acto::spawn<A>(acto::actor_thread::bind);
    a.send(A::M{counter});
  });

  t.join();
  // The sent message should be processed at thread exit.
  REQUIRE(*counter == 1);
}
