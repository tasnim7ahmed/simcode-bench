#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-mac.h"
#include "ns3/mobility-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsssErrorModelValidation");

int
main(int argc, char *argv[])
{
    double frameSize = 1472; // bytes
    std::string epsFileName = "frame-success-rate-vs-snr.eps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("FrameSize", "The size of the transmitted frame (bytes)", frameSize);
    cmd.Parse(argc, argv);

    Gnuplot gnuplot(epsFileName);
    gnuplot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    gnuplot.SetTerminal("postscript enhanced color");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set yrange [0:1.05]");
    gnuplot.SetExtra("set grid");

    std::vector<std::string> dsssModes{"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
    std::vector<Ptr<WifiPhy>> phys;
    std::vector<WifiMode> modes;
    std::vector<double> snrDbList;

    for (double snr = -5; snr <= 20; snr += 2)
    {
        snrDbList.push_back(snr);
    }

    for (auto &modeName : dsssModes)
    {
        WifiMode mode = WifiPhy::GetStaticWifiMode(modeName);
        modes.push_back(mode);

        Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
        Ptr<TraceSourceAccessor> accessor = MakeTraceSourceAccessor(&YansWifiChannel::GetPropagationLossModel);
        channel->SetAttribute("Frequency", UintegerValue(2400));
        channel->SetAttribute("TxPowerStart", DoubleValue(0.01));
        channel->SetAttribute("TxPowerEnd", DoubleValue(0.01));
        channel->SetAttribute("TxGain", DoubleValue(0));
        channel->SetAttribute("RxGain", DoubleValue(0));

        Ptr<WifiPhy> phy = CreateObject<WifiPhy>();
        phy->SetChannel(channel);
        phy->ConfigureStandard(WIFI_STANDARD_80211b);
        phy->SetErrorRateModel("ns3::NistErrorRateModel");
        phy->SetDevice(CreateObject<WifiNetDevice>());
        phy->SetMobilityModel(CreateObject<ConstantPositionMobilityModel>());
        phy->Initialize();
        phys.push_back(phy);
    }

    Gnuplot2dDataset dataset;
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (size_t i = 0; i < modes.size(); ++i)
    {
        WifiMode mode = modes[i];
        dataset.SetTitle(dsssModes[i]);
        for (double snrDb : snrDbList)
        {
            double successRateSum = 0;
            int numRuns = 5;
            for (int run = 0; run < numRuns; ++run)
            {
                double successRate = 1.0;
                double ber = phys[i]->GetPhyEntity()->CalculateBer(mode, 0, 0, Seconds(1), snrDb);
                if (ber > 0.0)
                {
                    successRate = pow((1 - ber), 8 * frameSize);
                }
                successRateSum += successRate;
            }
            double avgSuccessRate = successRateSum / numRuns;
            dataset.Add(snrDb, avgSuccessRate);
        }
        gnuplot.AddDataset(dataset);
    }

    std::ofstream plotFile(epsFileName.c_str());
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}