// This file uses code from http://www.gnu.org/software/autoconf-archive/ax_cxx_compile_stdcxx_11.html (serial 5)

// Copyright (c) 2008 Benjamin Kosnik <bkoz@redhat.com>
// Copyright (c) 2012 Zack Weinberg <zackw@panix.com>
// Copyright (c) 2013 Roy Stogner <roystgnr@ices.utexas.edu>
// Copyright (c) 2014, 2015 Google Inc.; contributed by Alexey Sokolov <sokolov@google.com>
//
// Copying and distribution of this file, with or without modification, are
// permitted in any medium without royalty provided the copyright notice
// and this notice are preserved. This file is offered as-is, without any
// warranty.

#include <map>

template <typename T>
struct check {
    static_assert(sizeof(int) <= sizeof(T), "not big enough");
};

struct Base {
    virtual void f() {}
};
struct Child : public Base {
    virtual void f() override {}
};

typedef check<check<bool>> right_angle_brackets;

int a;
decltype(a) b;

typedef check<int> check_type;
check_type c;
check_type&& cr = static_cast<check_type&&>(c);

auto d = a;
auto l = []() {};

// http://stackoverflow.com/questions/13728184/template-aliases-and-sfinae
// Clang 3.1 fails with headers of libstd++ 4.8.3 when using std::function
// because of this
namespace test_template_alias_sfinae {
struct foo {};

template <typename T>
using member = typename T::member_type;

template <typename T>
void func(...) {}

template <typename T>
void func(member<T>*) {}

void test();

void test() { func<foo>(0); }
}

int main() {
    std::map<int, int> m;
    m.emplace(2, 4);
    return 0;
}
