import os
import shutil
import tempfile
import unittest

import pyhalbe.Coordinate as C
from pymarocco.results import Marocco
import pyhmf as pynn
import pylogging
import pymarocco

import utils


class TestResults(unittest.TestCase):
    def setUp(self):
        pylogging.reset()
        pylogging.default_config(pylogging.LogLevel.ERROR)
        pylogging.set_loglevel(
            pylogging.get("marocco"), pylogging.LogLevel.INFO)

        self.log = pylogging.get(__name__)
        self.temporary_directory = tempfile.mkdtemp(prefix="marocco-test-")

        self.marocco = pymarocco.PyMarocco()
        self.marocco.backend = pymarocco.PyMarocco.None
        self.marocco.persist = os.path.join(
            self.temporary_directory, "results.bin")

    def tearDown(self):
        shutil.rmtree(self.temporary_directory, ignore_errors=True)
        del self.marocco

    @utils.parametrize([".xml", ".bin", ".xml.gz", ".bin.gz"])
    def test_file_format(self, extension):
        self.marocco.persist = (
            os.path.splitext(self.marocco.persist)[0] + extension
        )
        pynn.setup(marocco=self.marocco)

        target = pynn.Population(1, pynn.IF_cond_exp, {})

        pynn.run(0)
        pynn.end()

        self.assertTrue(os.path.exists(self.marocco.persist))
        results = Marocco.from_file(self.marocco.persist)

        self.assertEqual(1, len(list(results.placement)))

    @utils.parametrize([2, 4, 6, 8])
    def test_small_network(self, neuron_size):
        self.marocco.neuron_placement.default_neuron_size(neuron_size)
        pynn.setup(marocco=self.marocco)

        target = pynn.Population(1, pynn.IF_cond_exp, {})
        p1 = pynn.Population(2, pynn.SpikeSourceArray, {'spike_times': [1.]})
        p2 = pynn.Population(5, pynn.IF_cond_exp, {})
        pops = [target, p1, p2]
        proj1 = pynn.Projection(
            p1, target, pynn.AllToAllConnector(weights=0.004))
        proj2 = pynn.Projection(
            p2, target, pynn.AllToAllConnector(weights=0.004))
        projections = [proj1, proj2]

        pynn.run(0)
        pynn.end()

        self.assertTrue(os.path.exists(self.marocco.persist))
        results = Marocco.from_file(self.marocco.persist)

        self.assertEqual(sum(map(len, pops)), len(list(results.placement)))
        for pop in pops:
            for n in xrange(len(pop)):
                items = list(results.placement.find(pop[n]))
                self.assertEqual(1, len(items))
                item = items[0]

                bio_neuron = item.bio_neuron()
                self.assertEqual(pop.euter_id(), bio_neuron.population())
                self.assertEqual(n, bio_neuron.neuron_index())

                logical_neuron = item.logical_neuron()
                if pop.celltype == pynn.SpikeSourceArray:
                    self.assertTrue(logical_neuron.is_external())
                    self.assertIsNone(item.neuron_block())
                else:
                    self.assertFalse(logical_neuron.is_external())
                    self.assertIsNotNone(item.neuron_block())
                    self.assertEqual(neuron_size, logical_neuron.size())

                # Every placed population should have an address.
                self.assertIsNotNone(item.dnc_merger())
                address = item.address()
                self.assertIsNotNone(address)

    @utils.parametrize([1, 2, 3])
    def test_analog_outputs(self, num_recorded_populations):
        """
        Test that analog outputs are correctly assigned and that
        mapping fails if per-HICANN constraints are broken.
        """
        pynn.setup(marocco=self.marocco)
        hicann = C.HICANNOnWafer(C.Enum(210))

        pops = []
        for i in range(num_recorded_populations):
            pop = pynn.Population(1, pynn.IF_cond_exp, {})
            self.marocco.manual_placement.on_hicann(pop, hicann)
            pop.record_v()
            pops.append(pop)

        if num_recorded_populations > 2:
            with self.assertRaises(RuntimeError):
                pynn.run(0)
                pynn.end()
            return

        pynn.run(0)
        pynn.end()

        results = Marocco.from_file(self.marocco.persist)
        aouts = list(results.analog_outputs)
        self.assertEqual(num_recorded_populations, len(aouts))

        for pop in pops:
            placement_item, = list(results.placement.find(pop[0]))

            logical_neuron = placement_item.logical_neuron()
            for aout_item in aouts:
                if aout_item.logical_neuron() == logical_neuron:
                    break
            else:
                self.fail("logical neuron not found in analog outputs result")

            aout_item_ = results.analog_outputs.record(logical_neuron)
            self.assertEqual(aout_item.analog_output(), aout_item_.analog_output())


if __name__ == '__main__':
    unittest.main()
