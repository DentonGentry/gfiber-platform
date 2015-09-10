#include <iostream>
#include <random>

int rand_between(int min, int max)
{
  std::random_device rd;
  std::default_random_engine gen(rd());
  std::uniform_int_distribution<int> dist(min, max);
  return dist(gen);
}

void usage_and_die(const char *argv0)
{
  std::cerr << "Usage: " << argv0 << " [minval] <maxval>" << std::endl;
  exit(1);
}

int main(int argc, char **argv)
{
  int min = -1;
  int max = -1;

  switch (argc) {
  case 2:
    min = 0;
    max = atoi(argv[1]);
    break;
  case 3:
    min = atoi(argv[1]);
    max = atoi(argv[2]);
    break;
  default:
    usage_and_die(argv[0]);
  }

  std::cout << rand_between(min, max) << std::endl;
  return 0;
}
