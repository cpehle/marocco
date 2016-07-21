#include "marocco/parameter/HICANNParameters.h"

#include "HMF/NeuronCalibration.h"
#include "HMF/SynapseRowCalibration.h"
#include "calibtic/backend/Backend.h"
#include "calibtic/backend/Library.h"
#include "hal/Coordinate/iter_all.h"

#include "marocco/Logger.h"
#include "marocco/parameter/CMVisitor.h"
#include "marocco/parameter/NeuronVisitor.h"
#include "marocco/parameter/SpikeInputVisitor.h"

using namespace HMF::Coordinate;

namespace marocco {
namespace parameter {

HICANNParameters::HICANNParameters(
    BioGraph const& bio_graph,
    chip_type& chip,
    pymarocco::PyMarocco const& pymarocco,
    placement::Result const& placement,
    routing::Result const& routing,
    double duration)
    : m_bio_graph(bio_graph),
      m_chip(chip),
      m_pymarocco(pymarocco),
      m_placement(placement),
      m_routing(routing),
      m_duration(duration)
{
}

HICANNParameters::~HICANNParameters()
{
	// finally, we need to give sthal the spikes. Note, that we don't have to
	// sort them before hand, because they have to be reordered at the sthal
	// level anyway. This is also why it makes no sense to move (by-rvalue) the
	// spikes to sthal.
	for (auto const& merger : iter_all<DNCMergerOnHICANN>())
	{
		auto const& spikes = m_spikes[merger];
		if (!spikes.empty()) {
			m_chip.sendSpikes(GbitLinkOnHICANN(merger), spikes);
		}
	}
}

void HICANNParameters::run()
{
	auto const& neuron_placement = m_placement.neuron_placement;

	// assuming that neurons are always read out
	bool const local_neurons =
		m_placement.internal.address_assignment.at(m_chip.index()).has_output();

	bool const local_routes = m_routing.crossbar_routing.exists(m_chip.index());

	// spike input sources
	spike_input(neuron_placement);

	// switch on BackgroundGenerators for locking
	background_generators(m_pymarocco.bkg_gen_isi);

	// load calibration data from DB
	// const auto calib = getCalibrationData();
	// FIXME: get const calibration not possible, because we need to set speedup. see #1543
	auto calib = getCalibrationData();

	// v reset for all FG blocks in bio mV
	double v_reset = 0;

	if (local_neurons)
	{
		//const auto& neuron_calib = calib->atNeuronCollection();
		// FIXME: get const calibration not possible, because we need to set speedup. see #1543
		auto neuron_calib = calib->atNeuronCollection();
		neuron_calib->setSpeedup(m_pymarocco.speedup);
		auto const& synapse_routing = m_routing.synapse_routing.at(m_chip.index());

		{
			// 3. transform individual analog parameters
			v_reset =
			    neurons(*neuron_calib, neuron_placement, synapse_routing.synapse_target_mapping);

			if(local_routes) {
				// transform synapses
				auto synapse_row_calib = calib->atSynapseRowCollection();

				// FIXME: remove next lines when synapse calibration exists for real hardware (#1584)
				if ( m_pymarocco.param_trafo.use_ess_synapse_trafo )
					synapse_row_calib->setEssDefaults();
				else
					synapse_row_calib->setDefaults();

				synapses(*synapse_row_calib, synapse_routing, neuron_placement);
			}
		}
	}

	if (local_neurons || local_routes)
	{
		const auto& shared_calib = calib->atBlockCollection();

		// transforms global analog parameters
		// this needs to be done, for any HICANN used in any way. For
		// example, shared parameter also control L1.
		//
		// v_reset is in pynn units, i.e. mV, shared_parameters takes
		// HW units, i.e. Volts, shift_v is also given in Volts

		double const mV_to_V = 1/1000.;

		shared_parameters(*shared_calib,
		                  v_reset * m_pymarocco.param_trafo.alpha_v * mV_to_V +
						  m_pymarocco.param_trafo.shift_v);
	}
}

void HICANNParameters::neuron_config(neuron_calib_type const& /*unused*/)
{

	m_chip.use_big_capacitors(m_pymarocco.param_trafo.use_big_capacitors);

	// use defaults so far
	//auto& config = m_chip.neurons.config;
	//config.bigcap         = 0x0;
	//config.slow_I_radapt  = 0x0;
	//config.fast_I_radapt  = 0x0;
	//config.slow_I_gladapt = 0x0;
	//config.fast_I_gladapt = 0x0;
	//config.slow_I_gl      = 0x0;
	//config.fast_I_gl      = 0x0;
}

double HICANNParameters::neurons(
	neuron_calib_type const& calib,
	placement::results::Placement const& neuron_placement,
	routing::SynapseTargetMapping const& synapse_target_mapping)
{
	// GLOBAL DIGITAL Neuron Paramets
	neuron_config(calib);

	auto const hicann = m_chip.index();
	auto const& graph = m_bio_graph.graph();

	/* SHARED Analog Neuron Parameteres
	 * For each group of neurons that share analog values we have to agree
	 * on common values.  This has to happen prior to the configuration of
	 * individual neuron parameters as these depend on the shared values.
	 *
	 */
	NeuronSharedParameterRequirements shared_parameter_visitor;
	for (auto const& item : neuron_placement.find(hicann)) {
		auto const& params = graph[item.population()]->parameters();
		for (NeuronOnHICANN nrn : item.logical_neuron()) {
			visitCellParameterVector(
				params, shared_parameter_visitor, item.neuron_index(), nrn);
		}
	}

	// INDIVIDUAL Neuron Parameters
	TransformNeurons visitor{m_pymarocco.param_trafo.alpha_v, m_pymarocco.param_trafo.shift_v};

	MAROCCO_INFO("Configuring neuron parameters");
	for (auto const& item : neuron_placement.find(hicann)) {
		auto const& pop = *(graph[item.population()]);
		auto const& logical_neuron = item.logical_neuron();

		// Configure ANALOG neuron parameters.
		for (NeuronOnWafer nrn : logical_neuron) {
			MAROCCO_DEBUG("configuring analog parameters for " << nrn);

			transform_analog_neuron(
				calib, pop, item.neuron_index(), nrn, synapse_target_mapping,
				visitor, m_chip);
		}

		// As all denmems of a logical neuron will be connected,
		// DIGITAL neuron parameters are only configured for the first denmem.

		NeuronOnHICANN const nrn = logical_neuron.front();
		HMF::HICANN::Neuron& neuron = m_chip.neurons[nrn];

		// Set L1 address
		auto const& address = item.address();
		assert(address != boost::none);
		MAROCCO_DEBUG(nrn << " has sending address " << address->toL1Address());
		neuron.address(address->toL1Address());
		neuron.activate_firing(true);
		neuron.enable_spl1_output(true);

		// Connect all denmems belonging to this logical neuron.
		assert(logical_neuron.is_rectangular());
		connect_denmems(nrn, logical_neuron.size());
	}

	auto const v_resets = shared_parameter_visitor.get_v_resets();
	auto const mean_v_reset = shared_parameter_visitor.get_mean_v_reset();

	if(v_resets.size() != 1) {
		MAROCCO_WARN("more than one value for V_reset requested on " << m_chip);
		MAROCCO_WARN("only the mean v_reset will be used: " << mean_v_reset << " mV");
		for(auto v_reset : v_resets) {
			MAROCCO_DEBUG("individual v_reset values: " << v_reset << " mV");
		}

	}

	return mean_v_reset;
}

void HICANNParameters::connect_denmems(
	NeuronOnHICANN const& topleft_neuron,
	size_t hw_neurons_size)
{
	size_t const xwidth = hw_neurons_size/2;
	size_t const xmin   = topleft_neuron.x();
	m_chip.connect_denmems(X(xmin), X(xmin+xwidth-1));
}

void HICANNParameters::background_generators(uint32_t isi)
{
	// configure ALL BackgroundGenerators for Repeater & SynapseDriver locking.
	// They are NOT use for production neuron stimulation.
	for (auto const addr : iter_all<BackgroundGeneratorOnHICANN>()) {
		HMF::HICANN::BackgroundGenerator bg;
		bg.enable(true);
		bg.seed(0);
		bg.address(HMF::HICANN::L1Address(0));
		bg.set_mode(false /*random*/, isi /*isi*/);

		m_chip.layer1[addr] = bg;
	}
}

void HICANNParameters::spike_input(
	placement::results::Placement const& neuron_placement)
{
	HICANNOnWafer const hicann = m_chip.index();
	auto const& graph = m_bio_graph.graph();
	for (auto const dnc_merger : iter_all<DNCMergerOnHICANN>()) {
		for (auto const& item : neuron_placement.find(DNCMergerOnWafer(dnc_merger, hicann))) {
			if (!is_source(item.population(), graph)) {
				continue;
			}
			auto const& address = item.address();
			assert(address != boost::none);

			SpikeInputVisitor visitor(
			    m_pymarocco, m_spikes[dnc_merger], int(dnc_merger) * 209823 /*seed*/, m_duration);

			Population const& pop = *(graph[item.population()]);

			// configure input spike parameters
			transform_input_spikes(
			    pop, address->toL1Address(), item.neuron_index(), m_chip, visitor);
		}
	}
}

void HICANNParameters::shared_parameters(
	shared_calib_type const& calib,
	double v_reset)
{
	using namespace HMF;

	for (size_t ii=0; ii<HMF::Coordinate::FGBlockOnHICANN::enum_type::size; ++ii)
	{
		// default values for other parameters are also retrieved
		auto hwparams = calib.applySharedCalibration(v_reset, ii);
		FGBlockOnHICANN fgb{Enum{ii}};
		auto& fg = m_chip.floating_gates;
		hwparams.toHW(fgb, fg);
	}
}

void HICANNParameters::synapses(
	synapse_row_calib_type const& calib,
	routing::synapse_driver_mapping_t::result_type const&
		synapse_routing, // TODO change this to pass std::vector<DriverResult>
	placement::results::Placement const& neuron_placement)
{
	HMF::Coordinate::typed_array<double, NeuronOnHICANN> const weight_scale =
		weight_scale_array(neuron_placement);

	for (routing::DriverResult const& driver_res : synapse_routing.driver_result) {
		for (auto synrow : driver_res.rows() ) {
			auto const & synrow_addr = synrow.first;
			auto const & synrow_source = synrow.second;
			auto const & synapse_mapping = synrow_source.synapses();  // array of synapse mapping (1 row)
			auto synrow_proxy = m_chip.synapses[synrow_addr];

			std::array<double , NeuronOnHICANN::x_type::size> scaled_weights{{}}; // array storing scaled weights in nanoSiemens, 0. if not used
#ifndef MAROCCO_NDEBUG
			std::array<double , NeuronOnHICANN::x_type::size> bio_weights{{}}; // array storing bio weights in micro Siemens, 0. if not used
#endif // MAROCCO_NDEBUG

			// individual synapses
			for (size_t col = 0; col < synapse_mapping.size(); ++col) {
				routing::SynapseSource const& synapse_source = synapse_mapping[col];
				if (!synapse_source.empty()) {
					// get bio weight
					auto & proj_view = synapse_source.projection_view();
					double bio_weight = proj_view->getWeights()(synapse_source.src(), synapse_source.tgt()); // getWeights returns just a view, no copy

					// get weight scale
					SynapseOnHICANN syn_addr(synrow_addr, SynapseColumnOnHICANN(col));
					double const w_scale = weight_scale[ syn_addr.toNeuronOnHICANN() ];
					assert ( w_scale > 0. ); // check for inconsistency between routing and placement

					// scale and transform
					scaled_weights[col] = bio_weight*w_scale*1000. /*uS to nS*/;

#ifndef MAROCCO_NDEBUG
					// DEBUG
					bio_weights[col] = bio_weight;
#endif // MAROCCO_NDEBUG
				}
			} // all synapses of a row

			// compute max weight and find best gmax configuration
			double max_weight =  * (std::max_element(scaled_weights.cbegin(), scaled_weights.cend()));

			// get copy and not const ref, because findBestGmaxConfig is not const.
			HMF::SynapseRowCalibration row_calib = * boost::dynamic_pointer_cast< HMF::SynapseRowCalibration const > ( calib.at(synrow_addr) );

			HMF::GmaxConfig gc = row_calib.findBestGmaxConfig(max_weight);
			auto const& synapse_trafo = row_calib.at(gc);

			for (size_t col = 0; col < scaled_weights.size(); ++col) {
				if (scaled_weights[col] > 0.) {
					const double scaled_weight = scaled_weights[col];
					const HMF::HICANN::SynapseWeight hwweight = synapse_trafo->getDigitalWeight(scaled_weight);
					// store weight
					synrow_proxy.weights [col] = hwweight;

#ifndef MAROCCO_NDEBUG
					SynapseOnHICANN syn_addr(synrow_addr, SynapseColumnOnHICANN(col));
					double clipped_weight = synapse_trafo->getAnalogWeight(hwweight);
					clipped_weight = clipped_weight/weight_scale[syn_addr.toNeuronOnHICANN()]/1000. /*nS to uS*/;
					MAROCCO_DEBUG("synapse weight of " << syn_addr << " set to " << hwweight
						<< ", bio weight " << bio_weights[col] << ", clipped bio weight " << clipped_weight);
#endif // MAROCCO_NDEBUG
				}
			}

			// store gmax config
			auto& driver = m_chip.synapses[synrow_addr.toSynapseDriverOnHICANN()];
			HMF::HICANN::RowConfig& config = driver[synrow_addr.toRowOnSynapseDriver()];

			// selects 1 of 4 V_max values from global FGs
			config.set_gmax(gc.get_sel_Vgmax());

			// gmax divider [1..15]
			// use same config for left and right synaptic input
			config.set_gmax_div(left,  gc.get_gmax_div() );
			config.set_gmax_div(right, gc.get_gmax_div() );
		} // all rows assigned to an input
	} // all inputs
	// TODO: store V_gmax, depending on ESS or not ESS
}

HMF::Coordinate::typed_array<double, NeuronOnHICANN> HICANNParameters::weight_scale_array(
	placement::results::Placement const& neuron_placement) const
{
	CMVisitor const cm_visitor{};
	HMF::Coordinate::typed_array<double, NeuronOnHICANN> rv;

	//initialize all values to 0.
	for (auto const& noh : iter_all<NeuronOnHICANN>())
		rv[noh] = 0.;

	auto const& use_bigcap = m_chip.neurons.config.bigcap;

	auto const& graph = m_bio_graph.graph();

	// We need to calculate the scaling factor for each logical neuron.
	for (auto const& item : neuron_placement.find(m_chip.index())) {
		auto const& params = graph[item.population()]->parameters();
		auto const& logical_neuron = item.logical_neuron();

		// Sum up the capacity of the connected denmems on the hardware.
		double cm_hw = 0.;
		std::vector<NeuronOnHICANN> connected_neurons;
		connected_neurons.reserve(logical_neuron.size());
		for (NeuronOnHICANN nrn : item.logical_neuron()) {
			// We have to consider different capacitor choices on top / bottom neuron blocks.
			cm_hw += use_bigcap[nrn.y()] ? HMF::NeuronCalibration::big_cap
			                             : HMF::NeuronCalibration::small_cap;
			connected_neurons.push_back(nrn);
		}

		double const cm_bio = visitCellParameterVector(params, cm_visitor, item.neuron_index());
		double const scale = m_pymarocco.speedup * cm_hw / cm_bio;

		for (auto cnrn : connected_neurons) {
			rv[cnrn] = scale;
		}
	}
	return rv;
}

boost::shared_ptr<HICANNParameters::calib_type>
HICANNParameters::getCalibrationData()
{
	using pymarocco::PyMarocco;

	MAROCCO_DEBUG("Hardware backend: " << int(m_pymarocco.backend));
	MAROCCO_DEBUG("Calibration backend: " << int(m_pymarocco.calib_backend));

	boost::shared_ptr<calib_type> calib(new calib_type);

	if (m_pymarocco.backend == PyMarocco::Backend::ESS &&
	    m_pymarocco.calib_backend != PyMarocco::CalibBackend::Default)
		throw std::runtime_error(
		    "Using the ESS with calib_backend != CalibBackend::Default is currently not supported");

	switch(m_pymarocco.calib_backend) {

		case PyMarocco::CalibBackend::XML: {
			calibtic::MetaData md;
			auto backend = getCalibticBackend();

			const int hicann_id = m_chip.index().toHICANNOnWafer().id().value();
			std::stringstream calib_file;
			calib_file << "w" << int(m_chip.index().toWafer()) << "-h";
			calib_file << hicann_id;
			const std::string calib_file_string = calib_file.str();

			MAROCCO_INFO("loading calibration file: " << m_pymarocco.calib_path+"/"+calib_file_string << ".xml");
			backend->load(calib_file_string, md, *calib);

			break;
		}

		case PyMarocco::CalibBackend::Default: {
			calib->setDefaults();
			break;
		}

		default:
			throw std::runtime_error("unknown calibration backend type");

	} // switch calib backend


	if(calib->getPLLFrequency() != m_pymarocco.pll_freq) {
		MAROCCO_WARN("PLL stored in HICANNCollection "
		             << int(calib->getPLLFrequency()/1e6) << " MHz != "
		             << int(m_pymarocco.pll_freq/1e6) << " MHz set here.");
	}

	return calib;
}

boost::shared_ptr<calibtic::backend::Backend>
HICANNParameters::getCalibticBackend()
{
	using namespace calibtic;
	using namespace calibtic::backend;

	switch(m_pymarocco.calib_backend) {

	case pymarocco::PyMarocco::CalibBackend::XML: {
		auto lib = loadLibrary("libcalibtic_xml.so");
		auto backend = loadBackend(lib);

		if (!backend) {
			throw std::runtime_error("unable to load xml backend");
		}

		std::string calib_path = m_pymarocco.calib_path;
		if (std::getenv("MAROCCO_CALIB_PATH") != nullptr) {
			if (!calib_path.empty())
				// we break hard, if the user specified via both ways...
				throw std::runtime_error(
					"colliding settings: environment variable and pymarocco.calib_path both set");
			calib_path = std::string(std::getenv("MAROCCO_CALIB_PATH"));
		}

		backend->config("path", calib_path); // search in calib_path for calibration xml files
		backend->init();
		return backend;
	}

	default:

		throw std::runtime_error("unknown backend type");

	}

}

} // namespace parameter
} // namespace marocco