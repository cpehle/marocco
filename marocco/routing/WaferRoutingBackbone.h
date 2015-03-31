#pragma once

#include <limits>
#include <memory>
#include <vector>
#include <unordered_set>

#include "marocco/Algorithm.h"
#include "marocco/placement/Result.h"
#include "marocco/routing/Result.h"
#include "marocco/routing/SystemGraph.h"
#include "marocco/routing/WaferGraph.h"
#include "marocco/routing/WaferRouting.h"
#include "marocco/routing/SynapseLoss.h"
#include "pymarocco/PyMarocco.h"

namespace marocco {
namespace routing {

struct WaferRoutingBackbone :
	public WaferRouting
{
public:
	template<typename ... Args>
	WaferRoutingBackbone(
		HMF::Coordinate::Wafer const& wafer,
		boost::shared_ptr<SynapseLoss> const& sl,
		pymarocco::PyMarocco& pymarocco,
		routing_graph& rgraph,
		Args&& ... args);
	virtual ~WaferRoutingBackbone();

private:
	typedef HMF::Coordinate::HICANNGlobal HICANNGlobal;

	Route::Segments
	allocateRoute(
		Route::BusSegment const source,
		std::unordered_set<HICANNGlobal> targets,
		std::unordered_set<HICANNGlobal>& unreachable) override;
};


template<typename ... Args>
WaferRoutingBackbone::WaferRoutingBackbone(
		HMF::Coordinate::Wafer const& wafer,
		boost::shared_ptr<SynapseLoss> const& sl,
		pymarocco::PyMarocco& pymarocco,
		routing_graph& rgraph,
		Args&& ... args) :
	WaferRouting(wafer, sl, pymarocco, rgraph, std::forward<Args>(args)...)
{}

} // namespace routing
} // namespace marocco