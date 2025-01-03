/**
 * Copyright 2018-2024, XGBoost contributors
 */
#include "test_span.h"

#include <gtest/gtest.h>
#include <xgboost/span.h>

#include <vector>

#include "../../../src/common/transform_iterator.h"  // for MakeIndexTransformIter

namespace xgboost::common {
namespace {
using ST = common::Span<int, dynamic_extent>;
static_assert(std::is_trivially_copyable_v<ST>);
static_assert(std::is_trivially_move_assignable_v<ST>);
static_assert(std::is_trivially_move_constructible_v<ST>);
static_assert(std::is_trivially_copy_assignable_v<ST>);
static_assert(std::is_trivially_copy_constructible_v<ST>);
}  // namespace

TEST(Span, TestStatus) {
  int status = 1;
  TestTestStatus {&status}();
  ASSERT_EQ(status, -1);

  std::vector<double> foo;
  auto bar = Span{foo};
  ASSERT_FALSE(bar.data());
  ASSERT_EQ(bar.size(), 0);
}

TEST(Span, DlfConstructors) {
  // Dynamic extent
  {
    Span<int> s;
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.data(), nullptr);

    Span<int const> cs;
    ASSERT_EQ(cs.size(), 0);
    ASSERT_EQ(cs.data(), nullptr);
  }

  // Static extent
  {
    Span<int, 0> s;
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.data(), nullptr);

    Span<int const, 0> cs;
    ASSERT_EQ(cs.size(), 0);
    ASSERT_EQ(cs.data(), nullptr);
  }

  // Init list.
  {
    Span<float> s {};
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.data(), nullptr);

    Span<int const> cs {};
    ASSERT_EQ(cs.size(), 0);
    ASSERT_EQ(cs.data(), nullptr);
  }
}

TEST(Span, FromNullPtr) {
  // dynamic extent
  {
    Span<float> s {nullptr, static_cast<Span<float>::index_type>(0)};
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.data(), nullptr);

    Span<float const> cs {nullptr, static_cast<Span<float>::index_type>(0)};
    ASSERT_EQ(cs.size(), 0);
    ASSERT_EQ(cs.data(), nullptr);
  }
  // static extent
  {
    Span<float, 0> s {nullptr, static_cast<Span<float>::index_type>(0)};
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.data(), nullptr);

    Span<float const, 0> cs {nullptr, static_cast<Span<float>::index_type>(0)};
    ASSERT_EQ(cs.size(), 0);
    ASSERT_EQ(cs.data(), nullptr);
  }
}

TEST(Span, FromPtrLen) {
  float arr[16];
  InitializeRange(arr, arr+16);

  // static extent
  {
    Span<float> s (arr, 16);
    ASSERT_EQ (s.size(), 16);
    ASSERT_EQ (s.data(), arr);

    for (Span<float>::index_type i = 0; i < 16; ++i) {
      ASSERT_EQ (s[i], arr[i]);
    }

    Span<float const> cs (arr, 16);
    ASSERT_EQ (cs.size(), 16);
    ASSERT_EQ (cs.data(), arr);

    for (Span<float const>::index_type i = 0; i < 16; ++i) {
      ASSERT_EQ (cs[i], arr[i]);
    }
  }

  // dynamic extent
  {
    Span<float, 16> s (arr, 16);
    ASSERT_EQ (s.size(), 16);
    ASSERT_EQ (s.data(), arr);

    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ (s[i], arr[i]);
    }

    Span<float const, 16> cs (arr, 16);
    ASSERT_EQ (cs.size(), 16);
    ASSERT_EQ (cs.data(), arr);

    for (Span<float const>::index_type i = 0; i < 16; ++i) {
      ASSERT_EQ (cs[i], arr[i]);
    }
  }
}

TEST(SpanDeathTest, FromPtrLen) {
  float arr[16];
  InitializeRange(arr, arr+16);
  {
    auto lazy = [=]() {Span<float const, 16> tmp (arr, 5);};
    EXPECT_DEATH(lazy(), "");
  }
}

TEST(Span, FromFirstLast) {
  float arr[16];
  InitializeRange(arr, arr+16);

  // dynamic extent
  {
    Span<float> s (arr, arr + 16);
    ASSERT_EQ (s.size(), 16);
    ASSERT_EQ (s.data(), arr);
    ASSERT_EQ (s.data() + s.size(), arr + 16);

    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ (s[i], arr[i]);
    }

    Span<float const> cs (arr, arr + 16);
    ASSERT_EQ (cs.size(), 16);
    ASSERT_EQ (cs.data(), arr);
    ASSERT_EQ (cs.data() + cs.size(), arr + 16);

    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ (cs[i], arr[i]);
    }
  }

  // static extent
  {
    Span<float, 16> s (arr, arr + 16);
    ASSERT_EQ (s.size(), 16);
    ASSERT_EQ (s.data(), arr);
    ASSERT_EQ (s.data() + s.size(), arr + 16);

    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ (s[i], arr[i]);
    }

    Span<float const> cs (arr, arr + 16);
    ASSERT_EQ (cs.size(), 16);
    ASSERT_EQ (cs.data(), arr);
    ASSERT_EQ (cs.data() + cs.size(), arr + 16);

    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ (cs[i], arr[i]);
    }
  }
}

TEST(Span, FromOther) {
  // convert constructor
  {
    Span<int> derived;
    Span<int const> base{derived};
    ASSERT_EQ(base.size(), derived.size());
    ASSERT_EQ(base.data(), derived.data());
  }

  float arr[16];
  InitializeRange(arr, arr + 16);

  // default copy constructor
  {
    Span<float> s0 (arr);
    Span<float> s1 (s0);
    ASSERT_EQ(s0.size(), s1.size());
    ASSERT_EQ(s0.data(), s1.data());
  }
}

TEST(Span, FromArray) {
  float arr[16];
  InitializeRange(arr, arr + 16);

  {
    Span<float> s (arr);
    ASSERT_EQ(&arr[0], s.data());
    ASSERT_EQ(s.size(), 16);
    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ(arr[i], s[i]);
    }
  }

  {
    Span<float, 16> s (arr);
    ASSERT_EQ(&arr[0], s.data());
    ASSERT_EQ(s.size(), 16);
    for (size_t i = 0; i < 16; ++i) {
      ASSERT_EQ(arr[i], s[i]);
    }
  }
}

TEST(Span, FromContainer) {
  std::vector<float> vec (16);
  InitializeRange(vec.begin(), vec.end());

  Span<float> s(vec);
  ASSERT_EQ(s.size(), vec.size());
  ASSERT_EQ(s.data(), vec.data());

  bool res = std::equal(vec.begin(), vec.end(), s.begin());
  ASSERT_TRUE(res);
}

TEST(Span, Assignment) {
  int status = 1;
  TestAssignment{&status}();
  ASSERT_EQ(status, 1);
}

TEST(SpanIter, Construct) {
  int status = 1;
  TestIterConstruct{&status}();
  ASSERT_EQ(status, 1);
}

TEST(SpanIter, Ref) {
  int status = 1;
  TestIterRef{&status}();
  ASSERT_EQ(status, 1);
}

TEST(SpanIter, Calculate) {
  int status = 1;
  TestIterCalculate{&status}();
  ASSERT_EQ(status, 1);
}

TEST(SpanIter, Compare) {
  int status = 1;
  TestIterCompare{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, BeginEnd) {
  int status = 1;
  TestBeginEnd{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, RBeginREnd) {
  int status = 1;
  TestRBeginREnd{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, ElementAccess) {
  float arr[16];
  InitializeRange(arr, arr + 16);

  Span<float> s (arr);
  size_t j = 0;
  for (auto i : s) {
    ASSERT_EQ(i, arr[j]);
    ++j;
  }
}

TEST(SpanDeathTest, ElementAccess) {
  float arr[16];
  InitializeRange(arr, arr + 16);

  Span<float> s (arr);
  EXPECT_DEATH(s[16], "");
  EXPECT_DEATH(s[-1], "");

  EXPECT_DEATH(s(16), "");
  EXPECT_DEATH(s(-1), "");
}

TEST(Span, Obversers) {
  int status = 1;
  TestObservers{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, FrontBack) {
  {
    float arr[4] {0, 1, 2, 3};
    Span<float, 4> s(arr);
    ASSERT_EQ(s.front(), 0);
    ASSERT_EQ(s.back(), 3);
  }
  {
    std::vector<double> arr {0, 1, 2, 3};
    Span<double> s(arr);
    ASSERT_EQ(s.front(), 0);
    ASSERT_EQ(s.back(), 3);
  }
}

TEST(SpanDeathTest, FrontBack) {
  {
    Span<float, 0> s;
    EXPECT_DEATH(s.front(), "");
    EXPECT_DEATH(s.back(), "");
  }
  {
    Span<float> s;
    EXPECT_DEATH(s.front(), "");
    EXPECT_DEATH(s.back(), "");
  }
}

TEST(Span, FirstLast) {
  // static extent
  {
    float arr[16];
    InitializeRange(arr, arr + 16);

    Span<float> s (arr);
    Span<float, 4> first = s.first<4>();

    ASSERT_EQ(first.size(), 4);
    ASSERT_EQ(first.data(), arr);

    for (size_t i = 0; i < first.size(); ++i) {
      ASSERT_EQ(first[i], arr[i]);
    }
  }

  {
    float arr[16];
    InitializeRange(arr, arr + 16);

    Span<float> s (arr);
    Span<float, 4> last = s.last<4>();

    ASSERT_EQ(last.size(), 4);
    ASSERT_EQ(last.data(), arr + 12);

    for (size_t i = 0; i < last.size(); ++i) {
      ASSERT_EQ(last[i], arr[i+12]);
    }
  }

  // dynamic extent
  {
    float *arr = new float[16];
    InitializeRange(arr, arr + 16);
    Span<float> s (arr, 16);
    Span<float> first = s.first(4);

    ASSERT_EQ(first.size(), 4);
    ASSERT_EQ(first.data(), s.data());

    for (size_t i = 0; i < first.size(); ++i) {
      ASSERT_EQ(first[i], s[i]);
    }

    delete [] arr;
  }

  {
    float *arr = new float[16];
    InitializeRange(arr, arr + 16);
    Span<float> s (arr, 16);
    Span<float> last = s.last(4);

    ASSERT_EQ(last.size(), 4);
    ASSERT_EQ(last.data(), s.data() + 12);

    for (size_t i = 0; i < last.size(); ++i) {
      ASSERT_EQ(s[12 + i], last[i]);
    }

    delete [] arr;
  }
}

TEST(SpanDeathTest, FirstLast) {
  // static extent
  {
    float arr[16];
    InitializeRange(arr, arr + 16);

    Span<float> s (arr);
    auto constexpr kOne = static_cast<Span<float, 4>::index_type>(-1);
    EXPECT_DEATH(s.first<kOne>(), "");
    EXPECT_DEATH(s.first<17>(), "");
    EXPECT_DEATH(s.first<32>(), "");
  }

  {
    float arr[16];
    InitializeRange(arr, arr + 16);

    Span<float> s (arr);
    auto constexpr kOne = static_cast<Span<float, 4>::index_type>(-1);
    EXPECT_DEATH(s.last<kOne>(), "");
    EXPECT_DEATH(s.last<17>(), "");
    EXPECT_DEATH(s.last<32>(), "");
  }

  // dynamic extent
  {
    float *arr = new float[16];
    InitializeRange(arr, arr + 16);
    Span<float> s (arr, 16);
    EXPECT_DEATH(s.first(-1), "");
    EXPECT_DEATH(s.first(17), "");
    EXPECT_DEATH(s.first(32), "");

    delete [] arr;
  }

  {
    float *arr = new float[16];
    InitializeRange(arr, arr + 16);
    Span<float> s (arr, 16);
    EXPECT_DEATH(s.last(-1), "");
    EXPECT_DEATH(s.last(17), "");
    EXPECT_DEATH(s.last(32), "");

    delete [] arr;
  }
}

TEST(Span, Subspan) {
  int arr[16] {0};
  Span<int> s1 (arr);
  auto s2 = s1.subspan<4>();
  ASSERT_EQ(s1.size() - 4, s2.size());

  auto s3 = s1.subspan(2, 4);
  ASSERT_EQ(s1.data() + 2, s3.data());
  ASSERT_EQ(s3.size(), 4);

  auto s4 = s1.subspan(2, dynamic_extent);
  ASSERT_EQ(s1.data() + 2, s4.data());
  ASSERT_EQ(s4.size(), s1.size() - 2);
}

TEST(SpanDeathTest, Subspan) {
  int arr[16] {0};
  Span<int> s1 (arr);
  EXPECT_DEATH(s1.subspan(-1, 0), "");
  EXPECT_DEATH(s1.subspan(17, 0), "");

  auto constexpr kOne = static_cast<Span<int, 4>::index_type>(-1);
  EXPECT_DEATH(s1.subspan<kOne>(), "");
  EXPECT_DEATH(s1.subspan<17>(), "");
}

TEST(Span, Compare) {
  int status = 1;
  TestCompare{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, AsBytes) {
  int status = 1;
  TestAsBytes{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, AsWritableBytes) {
  int status = 1;
  TestAsWritableBytes{&status}();
  ASSERT_EQ(status, 1);
}

TEST(Span, Empty) {
  {
    Span<float> s {nullptr, static_cast<Span<float>::index_type>(0)};
    auto res = s.subspan(0);
    ASSERT_EQ(res.data(), nullptr);
    ASSERT_EQ(res.size(), 0);

    res = s.subspan(0, 0);
    ASSERT_EQ(res.data(), nullptr);
    ASSERT_EQ(res.size(), 0);
  }

  {
    Span<float, 0> s {nullptr, static_cast<Span<float>::index_type>(0)};
    auto res = s.subspan(0);
    ASSERT_EQ(res.data(), nullptr);
    ASSERT_EQ(res.size(), 0);

    res = s.subspan(0, 0);
    ASSERT_EQ(res.data(), nullptr);
    ASSERT_EQ(res.size(), 0);
  }
}

TEST(SpanDeathTest, Empty) {
  std::vector<float> data(1, 0);
  ASSERT_TRUE(data.data());
  // ok to define 0 size span.
  Span<float> s{data.data(), static_cast<Span<float>::index_type>(0)};
  EXPECT_DEATH(s[0], "");  // not ok to use it.
}

TEST(IterSpan, Basic) {
  auto iter = common::MakeIndexTransformIter([](std::size_t i) { return i; });
  std::size_t n = 13;
  auto span = IterSpan{iter, n};
  ASSERT_EQ(span.size(), n);
  for (std::size_t i = 0; i < n; ++i) {
    ASSERT_EQ(span[i], i);
  }
  ASSERT_EQ(span.subspan(1).size(), n - 1);
  ASSERT_EQ(span.subspan(1)[0], 1);
  ASSERT_EQ(span.subspan(1, 2)[1], 2);
}
}  // namespace xgboost::common
