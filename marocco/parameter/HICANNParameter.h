#pragma once

#include <unordered_map>
#include "HMF/NeuronCollection.h"
#include "HMF/HICANNCollection.h"
#include "HMF/BlockCollection.h"
#include "calibtic/backend/Backend.h"

#include "marocco/util.h"
#include "marocco/placement/Result.h"
#include "marocco/routing/Result.h"
#include "marocco/parameter/ParameterTransformation.h"
#include "marocco/parameter/util.h"
#include "pymarocco/PyMarocco.h"

namespace marocco {
namespace parameter {

class HICANNParameter :
	public ParameterTransformation
{
public:
	typedef typename chip_type<hardware_system_t>::type  chip_type;
	typedef ParameterTransformation::result_type result_type;
	typedef typename ::chip_type<hardware_system_t>::calib_type  calib_t;

	typedef graph_t::vertex_descriptor Vertex;
	typedef std::unordered_map<Vertex,
			std::pair<size_t /*neuron offset*/, ConstCurrentSourcePtr> >
		CurrentSourceMap;

	virtual ~HICANNParameter() {}
	template<typename ... Args>
	HICANNParameter(pymarocco::PyMarocco& pym, CurrentSourceMap const& csm, Args&& ... args) :
		ParameterTransformation(std::forward<Args>(args)...),
		mCurrentSourceMap(csm),
		mPyMarocco(pym)
	{}

	virtual std::unique_ptr<result_type> run(
		result_type const& placement,
		result_type const& routing);

private:
	CurrentSourceMap const& mCurrentSourceMap;
	pymarocco::PyMarocco& mPyMarocco;
};


class HICANNTransformator
{
public:
	typedef typename chip_type<hardware_system_t>::type  chip_type;
	typedef ParameterTransformation::result_type result_type;
	typedef ::chip_type<hardware_system_t>::calib_type  calib_t;
	typedef HMF::NeuronCollection  neuron_calib_t;
	typedef HMF::BlockCollection  shared_calib_t;
	typedef HMF::SynapseRowCollection  synapse_row_calib_t;
	typedef HMF::Coordinate::GbitLinkOnHICANN dnc_merger_coord;

	typedef std::unordered_map<HMF::Coordinate::NeuronOnHICANN,
		boost::shared_ptr<StepCurrentSource const>> CurrentSources;

	HICANNTransformator(graph_t const& graph, chip_type& chip, pymarocco::PyMarocco& pym);
	~HICANNTransformator();

	virtual std::unique_ptr<result_type> run(
		CurrentSources const& cs,
		placement::Result const& placement,
		routing::Result const& routing);

private:
	graph_t const& getGraph() const;

	chip_type&       chip();
	chip_type const& chip() const;

	void neurons(
		neuron_calib_t const& calib,
		typename placement::neuron_placement_t::result_type const& neuron_placement,
		typename placement::output_mapping_t::result_type const& output_mapping);

	void connect_denmems(
		HMF::Coordinate::NeuronOnHICANN const& topleft_neuron,
		size_t hw_neurons_size);

	void neuron_config(neuron_calib_t const& calib);

	void analog_output(neuron_calib_t const& calib,
		typename placement::neuron_placement_t::result_type const& neuron_placement);

	void merger(
		typename placement::neuron_placement_t::result_type const& neuron_placement,
		typename placement::output_mapping_t::result_type const& output_mapping);

	void spike_input(placement::OutputBufferMapping const& output_mapping);

	void current_input(neuron_calib_t const& calib, CurrentSources const& cs);

	void background_generators(uint32_t isi=500);

	void shared_parameters(
		graph_t const& graph,
		shared_calib_t const& calib);

	void synapses(
		synapse_row_calib_t const& calib,
		typename routing::synapse_driver_mapping_t::result_type const& synapse_mapping,
		typename placement::neuron_placement_t::result_type const& neuron_placement
		);

	/// returns an array with the weight scale factor for each neuron on the hicann.
	/// The factor to scale biological to hardware weights is calculated as: speedup * cm_hw/ cm_bio
	/// where cm_hw is the sum of the capacitances of all interconnected hw-neurons
	NeuronOnHICANNPropertyArray<double> weight_scale_array(
		typename placement::neuron_placement_t::result_type const& neuron_placement
		) const;

	boost::shared_ptr<calib_t> getCalibrationData();

	boost::shared_ptr<calibtic::backend::Backend>
	getCalibticBackend();

	chip_type& mChip;
	graph_t const& mGraph;

	typedef std::vector<sthal::Spike> SpikeList;
	std::array<SpikeList, dnc_merger_coord::end> mSpikes;

	pymarocco::PyMarocco& mPyMarocco;
};

} // namespace parameter
} // namespace marocco