// =============================================================================
// fq-compressor - Backpressure Controller Regression Tests
// =============================================================================

#include "fqc/pipeline/node_common.h"

#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

namespace fqc::pipeline::test {

TEST(BackpressureControllerTest, ResetWakesBlockedAcquireCallers) {
    BackpressureController controller(1);
    controller.acquire();

    std::promise<void> acquired;
    auto acquiredFuture = acquired.get_future();

    std::thread waiter([&controller, &acquired] {
        controller.acquire();
        acquired.set_value();
    });

    ASSERT_EQ(acquiredFuture.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    controller.reset();

    const auto status = acquiredFuture.wait_for(std::chrono::milliseconds(200));
    if (status != std::future_status::ready) {
        ASSERT_TRUE(controller.tryAcquire());
        controller.release();
    }

    EXPECT_EQ(status, std::future_status::ready);
    waiter.join();
}

}  // namespace fqc::pipeline::test
