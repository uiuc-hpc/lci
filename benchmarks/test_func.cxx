#include <boost/function.hpp>
#include <functional>

struct X {
  void do_x(){};
};

int main() {
  X* x = new X;
  std::function<void(X*)> f;
  f = &X::do_x;
  for (int i = 100000000; i >= 0; --i) f(x);
  delete x;
  return 0;
}
