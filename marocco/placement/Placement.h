#pragma once

#include <type_traits>
#include <stdexcept>
#include <memory>

#include "marocco/Algorithm.h"
#include "marocco/graph.h"
#include "marocco/placement/Result.h"
#include "pymarocco/PyMarocco.h"

namespace marocco {
namespace placement {

class ReverseMapping; // fwd dcl

/*! \class abstract placement interface
 */
class Placement :
	public Algorithm
{
public:
	typedef BaseResult result_type;

	virtual ~Placement() {}
	template<typename ... Args>
	Placement(Args&& ... args) :
		Algorithm(std::forward<Args>(args)...) {}

	// placement start interface
	virtual std::unique_ptr<result_type> run() = 0;

	// TODO: hacky reverse mapping for preliminary reverse channel
	typedef ReverseMapping rev_map_t;

	std::shared_ptr<rev_map_t> getRevMapping() const;

protected:
	std::shared_ptr<rev_map_t> mRevMapping;
};

class DefaultPlacement :
	public Placement
{
public:
	virtual ~DefaultPlacement();
	template<typename ... Args>
	DefaultPlacement(pymarocco::PyMarocco& pymarocco, Args&& ... args) :
		Placement(std::forward<Args>(args)...),
		mPyMarocco(pymarocco)
	{}

	virtual std::unique_ptr<typename Placement::result_type> run();

private:
	pymarocco::PyMarocco& mPyMarocco;
};

} // namespace placement
} // namespace marocco