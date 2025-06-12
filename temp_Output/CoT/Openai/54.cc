#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

// Simulation parameters
static const double simulationTime = 10.0; // seconds
static const uint32_t packetSize = 1400;   // bytes
static const uint32_t maxPackets = 0;      // Unlimited
static const std::string dataRate = "500Mbps";
static const uint16_t port = 9;

// Traffic types
enum TrafficType
{
    UDP,
    TCP
};

// Structures for results
struct RunResult
{
    TrafficType traffic;
    bool rtsCts;
    uint32_t channelWidth;
    uint32_t mcs;
    bool shortGuard;
    double distance;
    double goodput;
};

static void ResetPosition(Ptr<Node> sta, Ptr<Node> ap, double distance)
{
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sta);
    mobility.Install(ap);
    Ptr<MobilityModel> mobSta = sta->GetObject<MobilityModel>();
    mobSta->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<MobilityModel> mobAp = ap->GetObject<MobilityModel>();
    mobAp->SetPosition(Vector(distance, 0.0, 0.0));
}

static double GetGoodput(Ptr<Application> sinkApp)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp);
    return (sink->GetTotalRx() * 8.0) / simulationTime / 1e6; // Mbps
}

int main(int argc, char *argv[])
{
    std::vector<TrafficType> trafficTypes = {UDP, TCP};
    std::vector<bool> rtsCtsOpts = {false, true};
    std::vector<uint32_t> channelWidths = {20, 40, 80, 160};
    std::vector<uint32_t> mcsIndexes = {0,1,2,3,4,5,6,7,8,9};
    std::vector<bool> sgrOpts = {false, true}; // false: long, true: short
    std::vector<double> distances = {5.0, 20.0}; // Example: can be customized

    // Command line parameters
    double distance = 10.0;
    std::string outputFile = "";
    CommandLine cmd;
    cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue("outputFile", "File to save results (CSV)", outputFile);
    cmd.Parse(argc, argv);

    distances.clear();
    distances.push_back(distance);

    // Header for results
    std::ostringstream header;
    header << "Traffic,RTSCTS,ChannelWidth,MCS,ShortGI,Distance,Goodput_Mbps";
    std::vector<RunResult> results;
    std::cout << header.str() << std::endl;

    for (auto traffic : trafficTypes)
    for (auto rtsCts : rtsCtsOpts)
    for (auto width : channelWidths)
    for (auto mcs : mcsIndexes)
    for (auto sgr : sgrOpts)
    for (auto d : distances)
    {
        // Reset NS-3 state for each experiment
        SeedManager::SetSeed(12345);
        Config::Reset();

        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create(1);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default();

        phy.SetChannel(channel.Create());
        phy.SetErrorRateModel("ns3::YansErrorRateModel");

        // Configure SpectrumWifiPhy for 802.11ac
        phy.Set ("ChannelWidth", UintegerValue (width));
        phy.Set ("Frequency", UintegerValue (5180)); // 5 GHz
        phy.Set ("GuardInterval", TimeValue (sgr ? NanoSeconds(400): NanoSeconds(800)));

        // Guard Interval setting for 802.11ac
        std::string phyMode = "VhtMcs" + std::to_string(mcs);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211ac);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(phyMode),
                                     "ControlMode", StringValue("VhtMcs0"),
                                     "RtsCtsThreshold", UintegerValue(rtsCts ? 0 : 999999));

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211ac");

        // Configure MAC for STA
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

        // Configure MAC for AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Mobility
        ResetPosition(wifiStaNode.Get(0), wifiApNode.Get(0), d);

        // Internet stack
        InternetStackHelper stack;
        stack.Install(wifiStaNode);
        stack.Install(wifiApNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staIf = address.Assign(staDevice);
        Ipv4InterfaceContainer apIf = address.Assign(apDevice);

        // Application setup
        ApplicationContainer apptx, apprx;
        double appStart = 1.0, appStop = simulationTime;

        if (traffic == UDP)
        {
            UdpServerHelper udpServer(port);
            apprx = udpServer.Install(wifiApNode.Get(0));
            apprx.Start(Seconds(0.0));
            apprx.Stop(Seconds(simulationTime+1));

            UdpClientHelper udpClient(apIf.GetAddress(0), port);
            udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
            udpClient.SetAttribute("Interval", TimeValue(Seconds((double)packetSize*8.0/stod(dataRate.substr(0, dataRate.find("M")))/1e6)));
            udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
            apptx = udpClient.Install(wifiStaNode.Get(0));
            apptx.Start(Seconds(appStart));
            apptx.Stop(Seconds(appStop));
        }
        else
        {
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            apprx = sinkHelper.Install(wifiApNode.Get(0));
            apprx.Start(Seconds(0.0));
            apprx.Stop(Seconds(simulationTime+1));

            OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
            onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
            onoff.SetAttribute("MaxBytes", UintegerValue(0));
            onoff.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
            apptx = onoff.Install(wifiStaNode.Get(0));
        }

        PhyMode phyModeObj;
        phyModeObj = WifiMode(phyMode.c_str());

        // Enable Tracing (if needed)
        // Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsCts ? 0 : 999999));
        // phy.EnablePcapAll("80211ac-goodput");

        Simulator::Stop(Seconds(simulationTime+1));
        Simulator::Run();

        double goodput = 0;
        if (traffic == UDP)
        {
            Ptr<UdpServer> udpServerApp = DynamicCast<UdpServer>(apprx.Get(0));
            goodput = (udpServerApp->GetReceived() * packetSize * 8.0 / simulationTime) / 1e6;
        }
        else
        {
            Ptr<PacketSink> tcpSink = DynamicCast<PacketSink>(apprx.Get(0));
            goodput = (tcpSink->GetTotalRx() *8.0 / simulationTime) / 1e6;
        }

        // Print or save results
        std::ostringstream oss;
        oss << (traffic == UDP ? "UDP" : "TCP") << ","
            << (rtsCts ? "Y" : "N") << ","
            << width << ","
            << mcs << ","
            << (sgr ? "Short" : "Long") << ","
            << std::fixed << std::setprecision(1) << d << ","
            << std::fixed << std::setprecision(3) << goodput;
        std::cout << oss.str() << std::endl;

        if (!outputFile.empty())
        {
            RunResult r;
            r.traffic = traffic;
            r.rtsCts = rtsCts;
            r.channelWidth = width;
            r.mcs = mcs;
            r.shortGuard = sgr;
            r.distance = d;
            r.goodput = goodput;
            results.push_back(r);
        }

        Simulator::Destroy();
    }

    if (!outputFile.empty())
    {
        std::ofstream ofs(outputFile.c_str());
        ofs << header.str() << std::endl;
        for (const auto& r : results)
        {
            ofs << (r.traffic == UDP ? "UDP" : "TCP") << ","
                << (r.rtsCts ? "Y" : "N") << ","
                << r.channelWidth << ","
                << r.mcs << ","
                << (r.shortGuard ? "Short" : "Long") << ","
                << std::fixed << std::setprecision(1) << r.distance << ","
                << std::fixed << std::setprecision(3) << r.goodput
                << std::endl;
        }
        ofs.close();
    }

    return 0;
}