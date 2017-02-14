#include <chrono>
#include <thread>

#include <cerrno>

#include <sys/mman.h>

#include "gtest/gtest.h"
#include "atlas/atlas.h"

using namespace std::chrono;

TEST(AtlasNextNoJob, HandleNullptr) {
  const auto ret = atlas_next(nullptr);
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret == 0) {
      return ::testing::AssertionSuccess();
    } else if (ret < -1 && errno_ == EFAULT) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure()
             << "call did not fail and claimed to return a result";
    }
  };
  EXPECT_TRUE(validResult());
}

TEST(AtlasNextNoJob, HandleValid) {
  uint64_t id;
  const auto ret = atlas_next(&id);
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret == 0) {
      return ::testing::AssertionSuccess();
    } else if (ret < 0) {
      return ::testing::AssertionFailure() << "call failed with (" << errno_
                                           << "): " << strerror(errno_);
    } else {
      return ::testing::AssertionFailure() << "call returned " << ret
                                           << " job(s).";
    }
  };
  EXPECT_TRUE(validResult());
}

class AtlasNextJob : public ::testing::Test {
protected:
  uint64_t id{0};
  virtual void SetUp() {
    atlas::np::submit(std::this_thread::get_id(), ++id, 1s,
                      steady_clock::now() + 1s);
  }
  virtual void TearDown() {
    /* grab the job, if not done by the testcase */
    uint64_t id_;
    atlas::next(id_);
  }
};

TEST_F(AtlasNextJob, HandleValid) {
  uint64_t id;
  const auto ret = atlas_next(&id);
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret == 1 && id == 1) {
      return ::testing::AssertionSuccess();
    } else if (ret == 1) {
      return ::testing::AssertionFailure() << "returned wrong job id " << id;
    } else if (ret < 0) {
      return ::testing::AssertionFailure() << "call failed with (" << errno_
                                           << "): " << strerror(errno_);
    } else {
      return ::testing::AssertionFailure() << "call returned " << ret
                                           << " jobs.";
    }
  };
  EXPECT_TRUE(validResult());
}

TEST_F(AtlasNextJob, HandleNullptr) {
  const auto ret = atlas_next(nullptr);
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret < 0 && errno_ == EFAULT) {
      return ::testing::AssertionSuccess();
    } else if (ret < 0) {
      return ::testing::AssertionFailure() << "call failed with (" << errno_
                                           << "): " << strerror(errno_);
    } else {
      return ::testing::AssertionFailure() << "call returned " << ret
                                           << " jobs.";
    }
  };
  EXPECT_TRUE(validResult());
}

class page {
  static constexpr size_t page_size = 4096;
  void *addr;

public:
  page(int prot = 0)
      : addr(mmap(NULL, page_size, PROT_READ | prot,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) {}
  ~page() { munmap(addr, page_size); }

  template <typename T> auto get() const { return static_cast<T *>(addr); }
  /* let the object straddle a page boundary to an (hopefully) unmapped page */
  template <typename T> auto get_invalid() const {
    auto char_ptr = static_cast<char *>(addr);
    return static_cast<T *>(static_cast<void *>(--char_ptr));
  }
};

TEST_F(AtlasNextJob, HandleRO) {
  auto p = std::make_unique<page>();
  const auto ret = atlas_next(p->get<uint64_t>());
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret < 0 && errno_ == EFAULT) {
      return ::testing::AssertionSuccess();
    } else if (ret < 0) {
      return ::testing::AssertionFailure() << "call failed with (" << errno_
                                           << "): " << strerror(errno_);
    } else {
      return ::testing::AssertionFailure() << "call returned " << ret
                                           << " jobs.";
    }
  };
  EXPECT_TRUE(validResult());
}

TEST_F(AtlasNextJob, HandleInvalid) {
  auto p = std::make_unique<page>(PROT_WRITE);
  const auto ret = atlas_next(p->get_invalid<uint64_t>());
  const auto errno_ = errno;
  auto validResult = [=] {
    if (ret < 0 && errno_ == EFAULT) {
      return ::testing::AssertionSuccess();
    } else if (ret < 0) {
      return ::testing::AssertionFailure() << "call failed with (" << errno_
                                           << "): " << strerror(errno_);
    } else {
      return ::testing::AssertionFailure() << "call returned " << ret
                                           << " jobs.";
    }
  };
  EXPECT_TRUE(validResult());
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
