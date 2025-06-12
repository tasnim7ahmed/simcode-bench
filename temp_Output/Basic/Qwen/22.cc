#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-net-device.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsssErrorRateValidation");

void
CalculateFrameSuccessRate(Ptr<ErrorRateModel> errorModel, WifiMode mode, Gnuplot2dDataset dataset)
{
    double snrStart = 0.0;
    double snrEnd = 20.0;
    double snrStep = 1.0;
    uint32_t frameSize = 1500;

    for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
    {
        double successRate = 0.0;
        double total = 1000;
        for (uint32_t i = 0; i < total; ++i)
        {
            Ptr<Packet> packet = Create<Packet>(frameSize);
            double snr = std::pow(10.0, snrDb / 10.0);
            WifiTxVector txVector;
            txVector.SetMode(mode);
            double ps = errorModel->GetChunkSuccessRate(packet, snr, txVector);
            if (ps > 0 && (double)rand() / RAND_MAX < ps)
            {
                successRate += 1;
            }
        }
        successRate /= total;
        dataset.Add(snrDb, successRate);
    }
}

int main(int argc, char *argv[])
{
    RngSeedManager::SetSeed(12345);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    WifiHelper wifi;
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint32_t frameSize = 1500;
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("1Mbps"));
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(frameSize));
    Config::SetDefault("ns3::OnOffApplication::OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    Config::SetDefault("ns3::OnOffApplication::OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    apps = sink.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(11.0));

    Simulator::Run();

    Gnuplot gnuplot("dsss_error_rate.eps");
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set yrange [0:1.05]");
    gnuplot.SetExtra("set key top left");

    std::vector<WifiMode> dsssModes;
    dsssModes.push_back(WifiPhy::GetDsssRate1Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate2Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate5_5Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate11Mbps());

    std::vector<std::pair<Ptr<ErrorRateModel>, std::string>> models;
    models.emplace_back(CreateObject<YansErrorRateModel>(), "Yans");
    models.emplace_back(CreateObject<NistErrorRateModel>(), "Nist");
    models.emplace_back(CreateObject<TableBasedErrorRateModel>(), "Table");

    for (const auto& modelPair : models)
    {
        Gnuplot2dDataset dataset(modelPair.second);
        dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

        for (const auto& mode : dsssModes)
        {
            CalculateFrameSuccessRate(modelPair.first, mode, dataset);
        }

        gnuplot.AddDataset(dataset);
    }

    std::ofstream plotFile("dsss_error_rate.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Destroy();

    return 0;
}