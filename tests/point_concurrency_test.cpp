#include "fourq.hpp"

#include <atomic>
#include <barrier>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using Curve::FourQ::Point;
using Curve::FourQ::Scalar;

constexpr std::ptrdiff_t kThreadCount = 16;
constexpr int kRoundCount = 64;
constexpr int kOperationCount = 32;

Point makeProjectivePoint() {
  Point point = Point::getBase();
  point += Point::mulBase(Scalar(2));
  return point;
}

bool concurrentMultiplyByOnePreservesPoint() {
  const Scalar one(1);
  const auto expected = Point::mulBase(Scalar(3)).getRaw();

  for (int round = 0; round < kRoundCount; ++round) {
    Point shared_point = makeProjectivePoint();
    std::barrier start_line(kThreadCount);
    std::atomic<int> failure_count{0};
    std::mutex failure_message_mutex;
    std::string first_failure_message;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(kThreadCount));

    for (std::ptrdiff_t thread_index = 0; thread_index < kThreadCount;
         ++thread_index) {
      workers.emplace_back([&] {
        start_line.arrive_and_wait();
        try {
          shared_point *= one;
        } catch (const std::exception &error) {
          failure_count.fetch_add(1, std::memory_order_relaxed);
          const std::lock_guard lock(failure_message_mutex);
          if (first_failure_message.empty()) {
            first_failure_message = error.what();
          }
        }
      });
    }

    for (auto &worker : workers) {
      worker.join();
    }

    if (failure_count.load(std::memory_order_relaxed) != 0) {
      std::cerr << "round " << round << ": " << first_failure_message << '\n';
      return false;
    }

    try {
      if (shared_point.getRaw() != expected) {
        std::cerr << "round " << round
                  << ": multiplication changed the point\n";
        return false;
      }
    } catch (const std::exception &error) {
      std::cerr << "round " << round
                << ": final point is invalid: " << error.what() << '\n';
      return false;
    }
  }

  return true;
}

bool concurrentReadsAndIdentityWritesPreservePoint() {
  const Scalar one(1);
  const Point zero = Point::getZero();
  const auto expected = Point::mulBase(Scalar(3)).getRaw();
  Point shared_point = makeProjectivePoint();
  std::barrier start_line(kThreadCount);
  std::atomic<bool> failed{false};
  std::vector<std::thread> workers;
  workers.reserve(static_cast<std::size_t>(kThreadCount));

  for (std::ptrdiff_t thread_index = 0; thread_index < kThreadCount;
       ++thread_index) {
    workers.emplace_back([&, thread_index] {
      start_line.arrive_and_wait();
      try {
        for (int operation = 0; operation < kOperationCount; ++operation) {
          switch (thread_index % 4) {
          case 0:
            shared_point *= one;
            break;
          case 1:
            shared_point += zero;
            break;
          case 2:
            if (shared_point.getRaw() != expected) {
              failed.store(true, std::memory_order_relaxed);
            }
            break;
          default: {
            const Point copied(shared_point);
            Point assigned;
            assigned = shared_point;
            if (copied.getRaw() != expected || assigned.getRaw() != expected) {
              failed.store(true, std::memory_order_relaxed);
            }
            break;
          }
          }
        }
      } catch (const std::exception &error) {
        std::cerr << "mixed concurrent operation failed: " << error.what()
                  << '\n';
        failed.store(true, std::memory_order_relaxed);
      }
    });
  }

  for (auto &worker : workers) {
    worker.join();
  }

  if (failed.load(std::memory_order_relaxed) ||
      shared_point.getRaw() != expected) {
    std::cerr << "mixed concurrent operations changed the point\n";
    return false;
  }

  return true;
}

} // namespace

int main() {
  if (!concurrentMultiplyByOnePreservesPoint()) {
    return 1;
  }

  if (!concurrentReadsAndIdentityWritesPreservePoint()) {
    return 1;
  }

  return 0;
}
