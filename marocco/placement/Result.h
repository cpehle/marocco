#pragma once

#include "marocco/AssociativeResult.h"
#include "marocco/Result.h"
#include "marocco/config.h"
#include "marocco/placement/MergerRoutingResult.h"
#include "marocco/placement/NeuronPlacementResult.h"
#include "marocco/placement/L1AddressAssignment.h"


namespace marocco {
namespace placement {

class LookupTable;

typedef std::unordered_map<HMF::Coordinate::HICANNOnWafer, L1AddressAssignment>
	WaferL1AddressAssignment;

struct Result : public BaseResult {
	NeuronPlacementResult neuron_placement;
	WaferL1AddressAssignment address_assignment;
	MergerRoutingResult merger_routing;
	// reverse mapping for result data (only neuron address translation for now)
	std::shared_ptr<LookupTable> reverse_mapping;
};

} // namespace placement
} // namespace marocco
