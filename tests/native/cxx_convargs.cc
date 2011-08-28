#include "test.hh"

#define settest(fmt, v, val) assert(convertArguments(array, fmt, &v, 0).to<int>() == 1 && v == val);
#define deftest(fmt, v, val) assert(convertArguments(array, fmt, &v, val).to<int>() == 1 && v == val);

int
doTest(Value& global)
{
  Value array = global.newArray();

  Value obj = global.newObject();
  obj.setPrivateName("private", (void*) 0x1234);
  arrayBuilder(array, global.newNumber('x'));
  arrayBuilder(array, global.newString("test"));
  arrayBuilder(array, obj);
  assert(array.get("length").to<int>() == 3);

  // No conversions
  assert(convertArguments(array, "").to<int>() == 0);
  assert(convertArguments(array, "aosimfoais").to<int>() == 0);

  unsigned char a = 0;
  signed char b = 0;
  unsigned short int c = 0;
  signed short int d = 0;
  unsigned int e = 0;
  signed int f = 0;
  unsigned long int g = 0;
  signed long int h = 0;
  unsigned long long i = 0;
  signed long long j = 0;
  float k = 0;
  double l = 0;
  long double m = 0;
  uintmax_t n = 0;
  intmax_t o = 0;
  ptrdiff_t p = 0;
  size_t q = 0;
  ssize_t r = 0;
  char s = 0;
  char* t = 0;
  Char u = 0;
  Char* v = 0;
  void* w = 0;

  settest("%hhn", a, 0);
  settest("%hho", a, 'x');
  settest("%hhu", a, 'x');
  settest("%hhx", a, 'x');
  settest("%hhX", a, 'x');
  settest("%hhd", b, 'x');
  settest("%hhi", b, 'x');

  settest("%hn", c, 0);
  settest("%ho", c, 'x');
  settest("%hu", c, 'x');
  settest("%hx", c, 'x');
  settest("%hX", c, 'x');
  settest("%hd", d, 'x');
  settest("%hi", d, 'x');

  settest("%n", e, 0);
  settest("%o", e, 'x');
  settest("%u", e, 'x');
  settest("%x", e, 'x');
  settest("%X", e, 'x');
  settest("%d", f, 'x');
  settest("%i", f, 'x');

  settest("%ln", g, 0);
  settest("%lo", g, 'x');
  settest("%lu", g, 'x');
  settest("%lx", g, 'x');
  settest("%lX", g, 'x');
  settest("%ld", h, 'x');
  settest("%li", h, 'x');

  settest("%lln", i, 0);
  settest("%llo", i, 'x');
  settest("%llu", i, 'x');
  settest("%llx", i, 'x');
  settest("%llX", i, 'x');
  settest("%lld", j, 'x');
  settest("%lli", j, 'x');
  i = 0;
  settest("%Ln", i, 0);
  settest("%Lo", i, 'x');
  settest("%Lu", i, 'x');
  settest("%Lx", i, 'x');
  settest("%LX", i, 'x');
  settest("%Ld", j, 'x');
  settest("%Li", j, 'x');

  settest("%e", k, 'x');
  settest("%f", k, 'x');
  settest("%g", k, 'x');
  settest("%E", k, 'x');
  settest("%a", k, 'x');

  settest("%le", l, 'x');
  settest("%lf", l, 'x');
  settest("%lg", l, 'x');
  settest("%lE", l, 'x');
  settest("%la", l, 'x');

  settest("%lle", m, 'x');
  settest("%llf", m, 'x');
  settest("%llg", m, 'x');
  settest("%llE", m, 'x');
  settest("%lla", m, 'x');
  settest("%Le", m, 'x');
  settest("%Lf", m, 'x');
  settest("%Lg", m, 'x');
  settest("%LE", m, 'x');
  settest("%La", m, 'x');

  settest("%jn", n, 0);
  settest("%jo", n, 'x');
  settest("%ju", n, 'x');
  settest("%jx", n, 'x');
  settest("%jX", n, 'x');
  settest("%jd", o, 'x');
  settest("%ji", o, 'x');

  settest("%tn", p, 0);
  settest("%to", p, 'x');
  settest("%tu", p, 'x');
  settest("%tx", p, 'x');
  settest("%tX", p, 'x');
  settest("%td", p, 'x');
  settest("%ti", p, 'x');

  settest("%zn", q, 0);
  settest("%zo", q, 'x');
  settest("%zu", q, 'x');
  settest("%zx", q, 'x');
  settest("%zX", q, 'x');
  settest("%zd", r, 'x');
  settest("%zi", r, 'x');

  settest("%c", s, 'x');
  assert(convertArguments(array, "%n%c", &s).to<int>() == 2 && s == 't');
  assert(convertArguments(array, "%n%s", &t).to<int>() == 2);
  assert(!strcmp(t, "test"));
  free(t);

  settest("%lc", u, 'x');
  assert(convertArguments(array, "%n%lc", &u).to<int>() == 2 && u == 't');
  assert(convertArguments(array, "%n%ls", &v).to<int>() == 2);
  assert(v[0] == 't');
  assert(v[1] == 'e');
  assert(v[2] == 's');
  assert(v[3] == 't');
  assert(v[4] == '\0');
  free(v);

  assert(convertArguments(array, "%n%n%[foo]", &w).to<int>() == 3 && w == NULL);
  assert(convertArguments(array, "%n%n%[private]", &w).to<int>() == 3 && w == (void*) 0x1234);

  // Reset the values to test default value handling
  array = global.newArray();
  a = 0;
  b = 0;
  c = 0;
  d = 0;
  e = 0;
  f = 0;
  g = 0;
  h = 0;
  i = 0;
  j = 0;
  k = 0;
  l = 0;
  m = 0;
  n = 0;
  o = 0;
  p = 0;
  q = 0;
  r = 0;
  s = 0;
  t = 0;
  u = 0;
  v = 0;
  w = 0;

  deftest("%hhn", a, 0);
  deftest("%hho", a, 'x');
  deftest("%hhu", a, 'x');
  deftest("%hhx", a, 'x');
  deftest("%hhX", a, 'x');
  deftest("%hhd", b, 'x');
  deftest("%hhi", b, 'x');

  deftest("%hn", c, 0);
  deftest("%ho", c, 'x');
  deftest("%hu", c, 'x');
  deftest("%hx", c, 'x');
  deftest("%hX", c, 'x');
  deftest("%hd", d, 'x');
  deftest("%hi", d, 'x');

  deftest("%n", e, 0);
  deftest("%o", e, 'x');
  deftest("%u", e, 'x');
  deftest("%x", e, 'x');
  deftest("%X", e, 'x');
  deftest("%d", f, 'x');
  deftest("%i", f, 'x');

  deftest("%ln", g, 0);
  deftest("%lo", g, 'x');
  deftest("%lu", g, 'x');
  deftest("%lx", g, 'x');
  deftest("%lX", g, 'x');
  deftest("%ld", h, 'x');
  deftest("%li", h, 'x');

  deftest("%lln", i, 0);
  deftest("%llo", i, 'x');
  deftest("%llu", i, 'x');
  deftest("%llx", i, 'x');
  deftest("%llX", i, 'x');
  deftest("%lld", j, 'x');
  deftest("%lli", j, 'x');
  i = 0;
  deftest("%Ln", i, 0);
  deftest("%Lo", i, 'x');
  deftest("%Lu", i, 'x');
  deftest("%Lx", i, 'x');
  deftest("%LX", i, 'x');
  deftest("%Ld", j, 'x');
  deftest("%Li", j, 'x');

  deftest("%e", k, 'x');
  deftest("%f", k, 'x');
  deftest("%g", k, 'x');
  deftest("%E", k, 'x');
  deftest("%a", k, 'x');

  deftest("%le", l, 'x');
  deftest("%lf", l, 'x');
  deftest("%lg", l, 'x');
  deftest("%lE", l, 'x');
  deftest("%la", l, 'x');

  deftest("%lle", m, 'x');
  deftest("%llf", m, 'x');
  deftest("%llg", m, 'x');
  deftest("%llE", m, 'x');
  deftest("%lla", m, 'x');
  deftest("%Le", m, 'x');
  deftest("%Lf", m, 'x');
  deftest("%Lg", m, 'x');
  deftest("%LE", m, 'x');
  deftest("%La", m, 'x');

  deftest("%jn", n, 0);
  deftest("%jo", n, 'x');
  deftest("%ju", n, 'x');
  deftest("%jx", n, 'x');
  deftest("%jX", n, 'x');
  deftest("%jd", o, 'x');
  deftest("%ji", o, 'x');

  deftest("%tn", p, 0);
  deftest("%to", p, 'x');
  deftest("%tu", p, 'x');
  deftest("%tx", p, 'x');
  deftest("%tX", p, 'x');
  deftest("%td", p, 'x');
  deftest("%ti", p, 'x');

  deftest("%zn", q, 0);
  deftest("%zo", q, 'x');
  deftest("%zu", q, 'x');
  deftest("%zx", q, 'x');
  deftest("%zX", q, 'x');
  deftest("%zd", r, 'x');
  deftest("%zi", r, 'x');

  deftest("%c", s, 'x');
  assert(convertArguments(array, "%s", &t, "test").to<int>() == 1);
  assert(!strcmp(t, "test"));
  free(t);

  Char testchars[] =
    { 't', 'e', 's', 't', '\0' };
  deftest("%lc", u, 'x');
  assert(convertArguments(array, "%ls", &v, testchars).to<int>() == 1);
  assert(v[0] == 't');
  assert(v[1] == 'e');
  assert(v[2] == 's');
  assert(v[3] == 't');
  assert(v[4] == '\0');
  free(v);

  assert(convertArguments(array, "%[foo]", &w, NULL).to<int>() == 1 && w == NULL);
  assert(convertArguments(array, "%[private]", &w, 0x1234).to<int>() == 1 && w == (void*) 0x1234);

  return 0;
}
