#include "acto.h"
#include "catch.hpp"

TEST_CASE("Finalize library") {
  acto::shutdown();
}
