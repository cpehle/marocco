#include "experiment/Result.h"
#include "marocco/Logger.h"
#include "marocco/placement/ReverseMapping.h"
#include "hal/Coordinate/iter_all.h"

#include <stdexcept>
#include <array>
#include <cstdint>
#include <vector>

using namespace marocco;
using namespace marocco::placement;
using namespace HMF::Coordinate;
using HMF::HICANN::L1Address;

namespace experiment {

ReadResults::ReadResults(
	pymarocco::PyMarocco const& pymarocco,
	hardware_type const& hw,
	resource_manager_t const& mgr) :
		mPyMarocco(pymarocco),
		mHW(hw),
		mMgr(mgr)
{}

double ReadResults::translate(double hw_time_in_s) const
{
	static double const seconds_to_ms = 1000.;
	return (hw_time_in_s - mPyMarocco.experiment_time_offset ) * mPyMarocco.speedup * seconds_to_ms;
}

void ReadResults::insertRandomSpikes(ObjectStore& objectstore) const
{
	warn(this) << "inserting random spikes";

	for (auto& pop : objectstore.populations()) {
		for (size_t neuron=0; neuron<pop->size(); ++neuron) {
			pop->getSpikes(neuron).push_back(translate(rand()));
		}
	}
}

// FIXME: this is completely broken, because it assumes a fixed merger
// configuration. This Information can directly be extracted from the Hardware
// representation.
void ReadResults::run(ObjectStore& objectstore, rev_map_t const& rev) const
{
	std::unordered_map<int, ObjectStore::population_vector::const_iterator> popMap;

	auto& pops = objectstore.populations();
	for (auto it=pops.begin(); it<pops.end(); ++it) {
		popMap[(*it)->id()] = it;
	}



	// TODO: this iteration also includes chips where only routing resources are
	// used.
	for (auto const& hicann : mMgr.allocated())
	{
		auto const& chip = mHW[hicann];

		// first read spikes from hardware
		std::array<std::vector<sthal::Spike>, GbitLinkOnHICANN::end> spikes;
		for (auto const& gbl : iter_all<GbitLinkOnHICANN>())
		{
			auto const& received_spikes = chip.receivedSpikes(gbl);
			auto const& sent_spikes = chip.sentSpikes(gbl);

			spikes[gbl].reserve(received_spikes.size() + sent_spikes.size());
			spikes[gbl].insert(spikes[gbl].end(), received_spikes.begin(), received_spikes.end());
			spikes[gbl].insert(spikes[gbl].end(), sent_spikes.begin(), sent_spikes.end());

			size_t cnt = 0;
			size_t cnt_ev_0 = 0;
			for (auto const& spike : spikes[gbl])
			{
				if (spike.addr != L1Address(0)) {
					MAROCCO_TRACE(hicann << " " << gbl << " " << spike.addr << " " << spike.time);
					cnt++;
				} else {
					cnt_ev_0++;
				}
			}
		}

		// then insert them into the euter objectstore
		for (size_t outb=0; outb<GbitLinkOnHICANN::end; ++outb)
		{
			for (auto const spike : spikes[outb])
			{
				if (spike.addr != L1Address(0)) {
					RevKey const key { hicann, OutputBufferOnHICANN(outb), spike.addr };
					RevVal const val = rev.at(key);

					auto it = popMap[val.pop];
					(*it)->getSpikes(val.neuron).push_back(translate(spike.time));
				}
			}
		}
	}

	// insert random spikes, just for testing purposes
	//insertRandomSpikes(objectstore);
}

} // exp