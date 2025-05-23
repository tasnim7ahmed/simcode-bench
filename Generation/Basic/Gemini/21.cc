#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-helper.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiClearChannelExperiment");

std::map<double, uint32_t> g_rssToPacketsReceived;
uint32_t g_packetsReceived = 0;

void PacketReceived(Ptr<const Packet> packet, const Address &srcAddress)
{
    g_packetsReceived++;
}

class Experiment
{
public:
    Experiment(std::string dataRateMode, double rss);
    void Setup();
    void Run();
    void Teardown();

private:
    std::string m_dataRateMode;
    double m_rss;
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    Ptr<OnOffApplication> m_onoffApp;
    Ptr<PacketSink> m_packetSink;
    ApplicationContainer m_apps;

    double m_txPower;
    double m_loss;

    void CalculatePathLoss();
};

Experiment::Experiment(std::string dataRateMode, double rss)
    : m_dataRateMode(dataRateMode), m_rss(rss)
{
    m_txPower = 10.0;
    CalculatePathLoss();
}

void Experiment::CalculatePathLoss()
{
    m_loss = m_txPower - m_rss;
    if (m_loss < 0) {
        m_loss = 0;
        NS_LOG_WARN("Calculated path loss for target RSS " << m_rss << " dBm and TX power " << m_txPower << " dBm resulted in a non-positive value. Clamping to 0 dB.");
    }
}

void Experiment::Setup()
{
    g_packetsReceived = 0;

    m_nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::ConstantPropagationLossModel",
                                    "Loss", DoubleValue(m_loss));
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPower", DoubleValue(m_txPower));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue("wifi-b-exp"));

    m_devices = wifi.Install(phy, mac, m_nodes);

    Config::Set("/NodeList/*/DeviceList/*/Phy/SupportedRates", StringValue(m_dataRateMode));
    Config::Set("/NodeList/*/DeviceList/*/Phy/PreambleType", StringValue("Short"));

    InternetStackHelper stack;
    stack.Install(m_nodes);

    m_interfaces = stack.Assign(m_devices);

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(m_interfaces.GetAddress(1), port));

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    m_packetSink = DynamicCast<PacketSink>(packetSinkHelper.Install(m_nodes.Get(1)).Get(0));
    m_packetSink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceived));

    OnOffHelper onoffHelper("ns3::UdpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute("DataRate", StringValue("10Mbps"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    m_onoffApp = DynamicCast<OnOffApplication>(onoffHelper.Install(m_nodes.Get(0)).Get(0));
    m_onoffApp->SetStartTime(Seconds(1.0));
    m_onoffApp->SetStopTime(Seconds(5.0));
}

void Experiment::Run()
{
    Simulator::Stop(Seconds(6.0));
    Simulator::Run();
}

void Experiment::Teardown()
{
    Simulator::Destroy();
    g_rssToPacketsReceived[m_rss] = g_packetsReceived;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiClearChannelExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    std::vector<std::string> dataRateModes = {
        "DsssRate1Mbps",
        "DsssRate2Mbps",
        "DsssRate5_5Mbps",
        "DsssRate11Mbps"
    };

    std::vector<double> rssValues;
    for (double rss = -30.0; rss >= -90.0; rss -= 5.0) {
        rssValues.push_back(rss);
    }
    std::sort(rssValues.begin(), rssValues.end(), std::greater<double>());

    for (const auto &mode : dataRateModes)
    {
        std::cout << "Testing Data Rate: " << mode << std::endl;
        g_rssToPacketsReceived.clear();

        for (double rss : rssValues)
        {
            std::cout << "  Testing RSS: " << rss << " dBm" << std::endl;
            Experiment exp(mode, rss);
            exp.Setup();
            exp.Run();
            exp.Teardown();
        }

        std::string plotFileName = "wifi-clear-channel-rss-" + mode;
        std::string plotTitle = "802.11b Clear Channel Performance (" + mode + ")";
        std::string dataFileName = plotFileName + ".dat";

        Gnuplot gnuplot(plotFileName + ".eps", Gnuplot::EPS);
        gnuplot.SetTitle(plotTitle);
        gnuplot.SetXLabel("Received Signal Strength (dBm)");
        gnuplot.SetYLabel("Packets Received");
        gnuplot.AppendExtra("set grid");
        gnuplot.AppendExtra("set key bottom right");

        GnuplotDataset dataset;
        dataset.SetTitle(mode);
        dataset.SetStyle(GnuplotDataset::LINES_POINTS);
        dataset.SetExtra("lw 2 pt 7 ps 1.5");

        for (const auto &pair : g_rssToPacketsReceived)
        {
            dataset.Add((double)pair.first, (double)pair.second);
        }

        gnuplot.AddDataset(dataset);
        gnuplot.GenerateOutput(std::cout);
        std::ofstream ofs(dataFileName.c_str());
        dataset.Output(ofs);
        ofs.close();
    }

    return 0;
}