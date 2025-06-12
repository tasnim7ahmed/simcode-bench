#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsssErrorRateValidation");

void
CalculateFrameSuccessRate(Ptr<WifiPhy> phy,
                          WifiMode mode,
                          double snrMin,
                          double snrMax,
                          uint32_t snrSteps,
                          Gnuplot2dDataset dataset,
                          std::string modelName)
{
    double snrStepSize = (snrMax - snrMin) / snrSteps;
    for (double snr = snrMin; snr <= snrMax; snr += snrStepSize)
    {
        double successRate = 0.0;
        uint32_t packetSize = 1024;
        uint32_t trials = 100;

        for (uint32_t i = 0; i < trials; ++i)
        {
            bool success = phy->GetErrorRateModel()->EvaluateTxSuccessProbability(phy->GetPayloadMode(mode, 0), snr, packetSize);
            if (success)
            {
                successRate += 1.0;
            }
        }

        successRate /= trials;
        dataset.Add(snr, successRate);
    }
}

int
main(int argc, char* argv[])
{
    uint32_t snrSteps = 20;
    double snrMin = 0.0;
    double snrMax = 20.0;
    uint32_t frameSize = 1024;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame to transmit.", frameSize);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = channelHelper.Create();

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_B);

    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(channel);

    WifiMacHelper macHelper;
    macHelper.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phyHelper, macHelper, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    phyHelper.EnablePcapAll("dsss_error_rate_validation");

    Gnuplot plot;
    plot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");

    GnuplotCollection collection("dsss_fsr_vs_snr.eps");
    collection.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    collection.AddPlot(plot);

    WifiMode dsssModes[] = {
        WifiMode("DsssRate1Mbps"),
        WifiMode("DsssRate2Mbps"),
        WifiMode("DsssRate5_5Mbps"),
        WifiMode("DsssRate11Mbps"),
    };
    uint32_t numDsssModes = sizeof(dsssModes) / sizeof(dsssModes[0]);

    std::vector<std::string> errorModels = {"Yans", "Nist", "Table"};

    for (uint32_t m = 0; m < numDsssModes; ++m)
    {
        for (auto model : errorModels)
        {
            std::ostringstream oss;
            oss << "DSSS " << dsssModes[m].GetDataRate() << " " << model;
            Gnuplot2dDataset dataset(oss.str());

            if (model == "Yans")
            {
                phyHelper.SetErrorRateModel("ns3::YansErrorRateModel");
            }
            else if (model == "Nist")
            {
                phyHelper.SetErrorRateModel("ns3::NistErrorRateModel");
            }
            else if (model == "Table")
            {
                phyHelper.SetErrorRateModel("ns3::TableBasedErrorRateModel");
            }

            CalculateFrameSuccessRate(StaticCast<WifiPhy>(devices.Get(0)->GetPhy()),
                                      dsssModes[m],
                                      snrMin,
                                      snrMax,
                                      snrSteps,
                                      dataset,
                                      model);

            plot.AddDataset(dataset);
        }
    }

    collection.GenerateOutput(Simulator::GetStream("dsss_fsr_vs_snr.out"));

    Simulator::Destroy();

    return 0;
}