// (C) Copyright Renaud Detry   2007-2008.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//#define TRSL_USE_SYSTEMATIC_INTUITIVE_ALGORITHM
#include <trsl/is_picked_systematic.hpp>
#include <trsl/ppfilter_iterator.hpp>
#include <examples/Particle.hpp>

#include <vector>
#include <iostream>
#include <numeric> // accumulate
#include <cassert>

// Type definitions, once and for all.

using namespace trsl::example;

typedef trsl::is_picked_systematic<Particle> is_picked;

typedef trsl::ppfilter_iterator<
  is_picked, std::vector<Particle>::const_iterator
> sample_iterator;

int main()
{
  const size_t POPULATION_SIZE = 100;
  const size_t SAMPLE_SIZE = 10;

  //-----------------------//
  // Generate a population //
  //-----------------------//
  
  std::vector<Particle> population;
  double totalWeight = 0;
  for (size_t i = 0; i < POPULATION_SIZE; ++i)
  {
    Particle p(double(rand())/RAND_MAX,  // weight
               double(rand())/RAND_MAX,  // position (x)
               double(rand())/RAND_MAX); // position (y)
    totalWeight += p.getWeight();
    population.push_back(p);
  }
  // Normalize total weight.
  for (std::vector<Particle>::iterator i = population.begin();
       i != population.end(); ++i)
    i->setWeight(i->getWeight()/totalWeight);
  
  std::vector<Particle> const& const_pop = population;
  
  //----------------------------//
  // Sample from the population //
  //----------------------------//
  
  std::vector<Particle> sample;
  // Create the systemtatic sampling functor.
  is_picked predicate(SAMPLE_SIZE, 1.0, &Particle::getWeight);

  std::cout << "Mean weight: " << 1.0/POPULATION_SIZE << std::endl;
  for (sample_iterator
         sb = sample_iterator(predicate, const_pop.begin(), const_pop.end()),
         si = sb,
         se = sample_iterator(predicate, const_pop.end(),   const_pop.end());
       si != se; ++si)
  {
    std::cout << "sample_" << std::distance(sb, si) << "'s weight = " <<
      si->getWeight() << std::endl;

    Particle p = *si;
    p.setWeight(1);
    sample.push_back(p);

    // ... or do something else with *si ...
  }
  assert(sample.size() == SAMPLE_SIZE);
  return 0;
}
