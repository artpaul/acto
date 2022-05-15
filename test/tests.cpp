#include "acto.h"
#include "catch.hpp"

#include <iostream>

TEST_CASE("Finalize library") {
  acto::shutdown();
}

TEST_CASE("Spawn actor") {
  class A : public acto::actor { };

  auto a = acto::spawn<A>();

  REQUIRE(a);

  acto::destroy(a);
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
      actor::handler<M>(
        [](acto::actor_ref, const M& m) { (*m.counter)++; });
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
