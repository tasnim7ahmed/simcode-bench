#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t frameSize = 1500; // bytes
    std::vector<WifiMode> dsssModes;
    dsssMode.push_back(WifiPhy::GetDsssRate1Mbps());
    dsssMode.push_back(WifiPhy::GetDsssRate2Mbps());
    dsssMode.push_back(WifiPhy::GetDsssRate5_5Mbps());
    dsssMode.push_back(WifiPhy::GetDsssRate11Mbps());

    Gnuplot plot("frame-success-rate-vs-snr.eps");
    plot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");

    Gnuplot2dDataset dataset;

    for (auto &mode : dsssModes) {
        for (double snrDb = 0; snrDb <= 20; snrDb += 1.0) {
            double successRateYans = WifiPhy::CalculatePlcpSuccessProbability(mode, Seconds(0.001), snrDb, WIFI_PHY_ERROR_RATE_MODEL_YANS);
            double successRateNist = WifiPhy::CalculatePlcpSuccessProbability(mode, Seconds(0.001), snrDb, WIFI_PHY_ERROR_RATE_MODEL_NIST);
            double successRateTable = WifiPhy::CalculatePlcpSuccessProbability(mode, Seconds(0.001), snrDb, WIFI_PHY_ERROR_RATE_MODEL_TABLE);

            NS_ASSERT(successRateYans >= 0.0 && successRateYans <= 1.0);
            NS_ASSERT(successRateNist >= 0.0 && successRateNist <= 1.0);
            NS_ASSERT(successRateTable >= 0.0 && successRateTable <= 1.0);

            dataset.Add(snrDb, successRateYans);
            dataset.Add(snrDb, successRateNist);
            dataset.Add(snrDb, successRateTable);
        }
    }

    plot.AddDataset(dataset);

    std::ofstream plotFile("frame-success-rate-vs-snr.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}