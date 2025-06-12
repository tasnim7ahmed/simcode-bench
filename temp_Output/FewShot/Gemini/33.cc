#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t packetSize = 1000;
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of packet generated", packetSize);
    cmd.AddValue("verbose", "turn on all WifiNetDevice logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG);
        LogComponentEnable("WifiMac", LOG_LEVEL_DEBUG);
    }

    std::vector<double> snrValues = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 22.0, 24.0, 26.0, 28.0, 30.0};
    std::vector<WifiMode> modes = {
        WifiMode("HtMcs0"), WifiMode("HtMcs1"), WifiMode("HtMcs2"), WifiMode("HtMcs3"),
        WifiMode("HtMcs4"), WifiMode("HtMcs5"), WifiMode("HtMcs6"), WifiMode("HtMcs7")
    };

    std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};

    for (const auto& errorModelName : errorModels) {
        for (const auto& mode : modes) {
            std::string filename = "ht_" + mode.GetName() + "_" + errorModelName + ".dat";
            std::ofstream output(filename);
            output << "# SNR FrameSuccessRate" << std::endl;

            for (double snr : snrValues) {
                Ptr<ErrorRateModel> errorModel;
                if (errorModelName == "NistErrorRateModel") {
                    errorModel = CreateObject<NistErrorRateModel>();
                } else if (errorModelName == "YansErrorRateModel") {
                    errorModel = CreateObject<YansErrorRateModel>();
                } else if (errorModelName == "TableBasedErrorRateModel") {
                      errorModel = CreateObject<TableBasedErrorRateModel>();
                } else {
                    std::cerr << "Unknown error model: " << errorModelName << std::endl;
                    return 1;
                }

                double ber = WifiMode::GetBer(mode, snr);
                double per = 1 - pow((1 - ber), (8 * packetSize));

                errorModel->SetBer(ber);

                bool success = (Simulator::Now().GetSeconds() >= 0 && Simulator::Now().GetSeconds() <= 1.0 && per <= 1.0);

                output << snr << " " << (1 - per) << std::endl;
            }
            output.close();

             Gnuplot plot(filename);
             plot.SetTitle("Frame Success Rate vs. SNR (" + mode.GetName() + ", " + errorModelName + ")");
             plot.SetLegend("SNR", "Frame Success Rate");
             plot.AddDataset(filename, "lines");
             plot.GenerateOutput();
        }
    }

    return 0;
}