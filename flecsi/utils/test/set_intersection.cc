/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */

#include <flecsi/utils/ftest.hh>
#include <flecsi/utils/set_intersection.hh>

template<class CONTAINER>
inline bool
intersects(const CONTAINER & one, const CONTAINER & two) {
  return flecsi::utils::intersects(
    one.begin(), one.end(), two.begin(), two.end());
}

int
set_intersection(int, char **) {

  FTEST();

  std::vector<int> a = {1, 3, 5, 7, 10, 11};
  std::vector<int> b = {2, 4, 6, 8, 10, 12};
  std::vector<int> c = {0, 20};
  std::vector<int> d = {10, 20, 30};
  std::vector<int> e = {};
  std::vector<int> f = {};

  EXPECT_EQ(intersects(a, a), true);
  EXPECT_EQ(intersects(a, b), true);
  EXPECT_EQ(intersects(a, c), false);
  EXPECT_EQ(intersects(a, d), true);
  EXPECT_EQ(intersects(b, a), true);
  EXPECT_EQ(intersects(b, b), true);
  EXPECT_EQ(intersects(b, c), false);
  EXPECT_EQ(intersects(b, d), true);
  EXPECT_EQ(intersects(c, a), false);
  EXPECT_EQ(intersects(c, b), false);
  EXPECT_EQ(intersects(c, c), true);
  EXPECT_EQ(intersects(c, d), true);
  EXPECT_EQ(intersects(d, a), true);
  EXPECT_EQ(intersects(d, b), true);
  EXPECT_EQ(intersects(d, c), true);
  EXPECT_EQ(intersects(d, d), true);
  EXPECT_EQ(intersects(a, e), false);
  EXPECT_EQ(intersects(e, f), false);

  return 0;
}

ftest_register_driver(set_intersection);
