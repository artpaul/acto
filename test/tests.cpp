#include "catch.hpp"
#include <acto/acto.h>
#include <acto/util.h>

#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

TEST_CASE("Finalize library") {
  acto::shutdown();
}

TEST_CASE("Uninitialized") {
  acto::destroy(acto::actor_ref());
  acto::destroy_and_wait(acto::actor_ref());
  acto::join(acto::actor_ref());
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

TEST_CASE("Function handlers") {
  struct A : acto::actor {
    struct A1 { };

    struct M0 { };

    struct M1 {
      std::string s;
    };

    struct MR {
      std::string s;
    };

    struct M2 {
      std::string s;
    };

    using MU = std::unique_ptr<std::string>;

    A(std::atomic<int>& x)
      : x_(x) {
      actor::handler<A1>([this](acto::actor_ref sender) {
        REQUIRE(!bool(sender));
        x_++;
      });

      actor::handler<M0>([this]() { x_++; });

      actor::handler<M1>([this](const M1& m) {
        REQUIRE(m.s == "M1");
        x_++;
      });

      actor::handler<MR>([this](MR&& m) {
        REQUIRE(m.s == "MR");
        x_++;
      });

      actor::handler<M2>([this](acto::actor_ref sender, const M2& m) {
        REQUIRE(!bool(sender));
        REQUIRE(m.s == "M2");
        x_++;
      });

      actor::handler<MU>([this](acto::actor_ref sender, MU m) {
        REQUIRE(!bool(sender));
        REQUIRE(*m == "MU");
        x_++;
      });
    }

  private:
    std::atomic<int>& x_;
  };

  std::atomic<int> x{0};
  acto::actor_ref a = acto::spawn<A>(x);

  CHECK(a.send<A::A1>());
  CHECK(a.send<A::M0>());
  CHECK(a.send<A::M1>(A::M1{.s = "M1"}));
  CHECK(a.send<A::M2>(A::M2{.s = "M2"}));
  CHECK(a.send<A::MR>(A::MR{.s = "MR"}));
  CHECK(a.send<A::MU>(std::make_unique<std::string>("MU")));
  // Cleanup actor.
  acto::destroy_and_wait(a);

  // Check values.
  REQUIRE(x.load() == 6);

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
      actor::handler<M>([](const M& m) { (*m.counter)++; });
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

TEST_CASE("Key for containers") {
  struct A : acto::actor { };

  std::map<acto::actor_ref, int> map;
  std::unordered_map<acto::actor_ref, int> hash_map;
  auto a = acto::spawn<A>();
  auto b = acto::spawn<A>();

  map.emplace(a, 1);
  map.emplace(b, 2);
  hash_map.emplace(a, 1);
  hash_map.emplace(b, 2);

  REQUIRE(map.find(a) != map.end());
  REQUIRE(hash_map.find(a) != hash_map.end());

  CHECK(map.find(a)->second == 1);
  CHECK(hash_map.find(a)->second == 1);

  acto::destroy(a);
  acto::destroy(b);
}

TEST_CASE("Move only") {
  struct A : acto::actor {
    struct M : acto::util::move_only {
      std::string s;
    };

    A() {
      actor::handler<M>([this](M e) {
        handle(std::move(e));
        actor::die();
      });
    }

    void handle(M&& m) {
      REQUIRE(m.s == "test");
    }
  };

  A::M ev{.s = "test"};
  auto a = acto::spawn<A>();
  a.send(std::move(ev));
  acto::join(a);
}

TEST_CASE("Send from bootstrap") {
  struct A : acto::actor {
    struct M { };

    A(bool& flag)
      : flag_(flag) {
      actor::handler<M>([this](acto::actor_ref sender, const M&) {
        flag_ = bool(sender);
        actor::die();
      });
    }

    void bootstrap() override {
      self().send<M>();
    }

    bool& flag_;
  };

  bool valid_sender = false;
  acto::join(acto::spawn<A>(valid_sender));

  CHECK(valid_sender);
}
