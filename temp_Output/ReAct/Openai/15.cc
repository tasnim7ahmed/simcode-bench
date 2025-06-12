#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoMcsThroughput");

// Structure to hold user-configurable parameters
struct SimulationConfig
{
    std::string trafficType = "UDP";      // "UDP" or "TCP"
    double startDistance = 1.0;           // meters
    double endDistance = 50.0;            // meters
    double stepDistance = 5.0;            // meters
    double simulationTime = 5.0;          // seconds
    double frequency = 5.0;               // GHz (2.4 or 5.0)
    uint32_t channelWidth = 20;           // MHz: 20 or 40 or 80
    bool shortGuardInterval = true;       // true or false
    bool useGreenfield = false;           // true = Greenfield, false = Mixed format
    uint32_t maxMimoStreams = 4;          // up to 4
    uint32_t payloadSize = 1472;          // bytes
    uint16_t port = 9;                    // port number for application
} g_cfg;

// Helper for translation between frequency value and YansWifiPhy standard enumeration
WifiPhyStandard GetPhyStandard(double frequency)
{
    if (std::abs(frequency - 2.4) < 1e-2)
        return WIFI_PHY_STANDARD_80211n_2_4GHZ;
    else
        return WIFI_PHY_STANDARD_80211n_5GHZ;
}

// Returns: vector of supported MCS index for specified number of streams (1..4)
std::vector<uint8_t> GetMcsSetForStreams(uint32_t nStreams)
{
    // 802.11n supports MCS 0..7 for 1 stream, 8..15 for 2 streams, 16..23 for 3 streams, 24..31 for 4 streams
    std::vector<uint8_t> indices;
    uint8_t base = (nStreams - 1) * 8;
    for (uint8_t i = 0; i < 8; ++i)
        indices.push_back(base + i);
    return indices;
}

struct Result
{
    double distance;
    double throughputMbps;
};

std::string GetCurveLabel(uint32_t nStreams, uint8_t mcs)
{
    std::ostringstream oss;
    oss << nStreams << "x" << "MCS" << mcs;
    return oss.str();
}

struct PerCurveData
{
    std::string label;
    std::vector<double> x; // distances
    std::vector<double> y; // throughputs
};

void PrintUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [--trafficType=TCP|UDP] [--band=2.4|5.0] "
              << "[--channelWidth=20|40|80] [--sgi=0|1] [--greenfield=0|1] [--distanceStart=1.0] "
              << "[--distanceEnd=50.0] [--distanceStep=5.0] [--simTime=5.0] [--payloadSize=1472] "
              << "[--maxStreams=4]" << std::endl;
}

void ParseArgs(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.AddValue("trafficType", "Set traffic type: UDP or TCP", g_cfg.trafficType);
    cmd.AddValue("band", "Set frequency band (2.4 or 5.0)", g_cfg.frequency);
    cmd.AddValue("channelWidth", "Wi-Fi channel width (MHz)", g_cfg.channelWidth);
    cmd.AddValue("sgi", "Short Guard Interval (1=true, 0=false)", g_cfg.shortGuardInterval);
    cmd.AddValue("greenfield", "HT Greenfield preamble (1=true, 0=false)", g_cfg.useGreenfield);
    cmd.AddValue("distanceStart", "Start distance in meters", g_cfg.startDistance);
    cmd.AddValue("distanceEnd", "End distance in meters", g_cfg.endDistance);
    cmd.AddValue("distanceStep", "Distance step in meters", g_cfg.stepDistance);
    cmd.AddValue("simTime", "Simulation duration in seconds", g_cfg.simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", g_cfg.payloadSize);
    cmd.AddValue("maxStreams", "Maximum number of spatial streams (MIMO)", g_cfg.maxMimoStreams);
    cmd.Parse(argc, argv);
    ToUpper(g_cfg.trafficType);
    if (g_cfg.maxMimoStreams < 1) g_cfg.maxMimoStreams = 1;
    if (g_cfg.maxMimoStreams > 4) g_cfg.maxMimoStreams = 4;
}

void ToUpper(std::string& str)
{
    for (char& c : str)
        c = ::toupper(c);
}

Ptr<Application> InstallUdpServer(Ptr<Node> node, uint16_t port)
{
    UdpServerHelper srv (port);
    ApplicationContainer apps = srv.Install(node);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(g_cfg.simulationTime + 1.0));
    return apps.Get(0);
}

Ptr<Application> InstallTcpServer(Ptr<Node> node, uint16_t port)
{
    Address bindAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sink ("ns3::TcpSocketFactory", bindAddr);
    ApplicationContainer apps = sink.Install(node);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(g_cfg.simulationTime + 1.0));
    return apps.Get(0);
}

Ptr<Application> InstallUdpClient(Ptr<Node> src, Ipv4Address dstAddr, uint16_t port, uint32_t payloadSize)
{
    UdpClientHelper client (dstAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(0));
    client.SetAttribute("Interval", TimeValue (Seconds(0.0001))); // nearly saturated
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer apps = client.Install(src);
    apps.Start(Seconds(1.0)); // avoid startup transients
    apps.Stop(Seconds(g_cfg.simulationTime));
    return apps.Get(0);
}

Ptr<Application> InstallTcpClient(Ptr<Node> src, Ipv4Address dstAddr, uint16_t port, uint32_t payloadSize)
{
    BulkSendHelper bulk ("ns3::TcpSocketFactory", InetSocketAddress(dstAddr, port));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));
    bulk.SetAttribute("SendSize", UintegerValue(payloadSize));
    ApplicationContainer apps = bulk.Install(src);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(g_cfg.simulationTime));
    return apps.Get(0);
}

void ConfigureHtDevice(Ptr<NetDevice> dev, uint8_t mcs, uint32_t nStreams)
{
    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
    Ptr<HtWifiMac> mac = DynamicCast<HtWifiMac>(wifiDev->GetMac());
    mac->SetAttribute("QosSupported", BooleanValue(true));
    Ptr<HtConfiguration> htConf = mac->GetHtConfiguration();
    htConf->SetAttribute("SupportedMcsSet", UintegerValue((1 << (mcs+1)) - 1)); // crude: enable up to mcs
    htConf->SetAttribute("McsStreams", UintegerValue(nStreams));
}

// Returns achieved throughput (Mbps)
double RunOneSimulation(double distance, uint32_t nStreams, uint8_t mcs, PerCurveData& curve, uint32_t curveIdx)
{
    NodeContainer nodes;
    nodes.Create(2);

    // Install Wi-Fi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    phy.Set ("ChannelWidth", UintegerValue (g_cfg.channelWidth));
    phy.Set ("ShortGuardEnabled", BooleanValue (g_cfg.shortGuardInterval));
    phy.Set ("Greenfield", BooleanValue (g_cfg.useGreenfield));
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(GetPhyStandard(g_cfg.frequency));

    HtWifiMacHelper mac;
    Ssid ssid = Ssid ("wifi-mimo-mcs");

    // Access Point
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "QosSupported", BooleanValue(true));

    NetDeviceContainer apDev;
    apDev = wifi.Install (phy, mac, nodes.Get (0));

    // Station
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false),
                 "QosSupported", BooleanValue(true));

    NetDeviceContainer staDev;
    staDev = wifi.Install (phy, mac, nodes.Get (1));

    // Set number of Rx/Tx antennas and MIMO streams
    phy.Set ("Antennas", UintegerValue (nStreams));
    phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (nStreams));
    phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (nStreams));

    // Assign spatial streams and mcs for both
    Ptr<WifiNetDevice> apWifiDev = DynamicCast<WifiNetDevice>(apDev.Get (0));
    Ptr<WifiNetDevice> staWifiDev = DynamicCast<WifiNetDevice>(staDev.Get (0));

    Ptr<HtWifiMac> apMac = DynamicCast<HtWifiMac>(apWifiDev->GetMac());
    apMac->GetHtConfiguration()->SetAttribute("SupportedMcsSet", UintegerValue((1u << (mcs + 1)) - 1));
    apMac->GetHtConfiguration()->SetAttribute("McsStreams", UintegerValue(nStreams));

    Ptr<HtWifiMac> staMac = DynamicCast<HtWifiMac>(staWifiDev->GetMac());
    staMac->GetHtConfiguration()->SetAttribute("SupportedMcsSet", UintegerValue((1u << (mcs + 1)) - 1));
    staMac->GetHtConfiguration()->SetAttribute("McsStreams", UintegerValue(nStreams));

    // Set MCS as default TXVector
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phys/TxSpatialStreams", UintegerValue(nStreams));
    Config::Set("/NodeList/*/DeviceList/*/Mac/Mcs", UintegerValue(mcs));
    Config::Set("/NodeList/*/DeviceList/*/Mac/ShortGuardInterval", BooleanValue (g_cfg.shortGuardInterval));

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 1.0));
    posAlloc->Add(Vector(distance, 0.0, 1.0));
    mobility.SetPositionAllocator (posAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper addr;
    addr.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = addr.Assign (NetDeviceContainer(apDev, staDev));

    Ptr<Application> srcApp, dstApp;
    if (g_cfg.trafficType == "UDP") {
        dstApp = InstallUdpServer(nodes.Get(0), g_cfg.port);
        srcApp = InstallUdpClient(nodes.Get(1), ifaces.GetAddress(0), g_cfg.port, g_cfg.payloadSize);
    } else {
        dstApp = InstallTcpServer(nodes.Get(0), g_cfg.port);
        srcApp = InstallTcpClient(nodes.Get(1), ifaces.GetAddress(0), g_cfg.port, g_cfg.payloadSize);
    }

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop (Seconds (g_cfg.simulationTime + 1.0));
    Simulator::Run ();

    // Measure throughput of the only Flow
    double throughput = 0.0;
    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (const auto& f : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (f.first);
        // traffic: src=STA, dst=AP
        if (t.destinationAddress == ifaces.GetAddress(0))
        {
            double rxBytes = f.second.rxBytes;
            double timeSecs = g_cfg.simulationTime - 1.0; // discount app ramp-up
            throughput = (rxBytes * 8.0) / (timeSecs * 1e6); // Mbps
            break;
        }
    }

    Simulator::Destroy();
    return throughput;
}

int main(int argc, char* argv[])
{
    ParseArgs(argc, argv);

    std::vector<PerCurveData> allResults;
    for (uint32_t nStreams = 1; nStreams <= g_cfg.maxMimoStreams; ++nStreams)
    {
        std::vector<uint8_t> mcsSet = GetMcsSetForStreams(nStreams);
        for (uint8_t i = 0; i < mcsSet.size(); ++i) {
            uint8_t mcs = mcsSet[i];
            PerCurveData curve;
            curve.label = GetCurveLabel(nStreams, mcs);
            for (double dist = g_cfg.startDistance; dist <= g_cfg.endDistance + 1e-3; dist += g_cfg.stepDistance)
            {
                NS_LOG_UNCOND("Testing nStreams=" << nStreams << " MCS=" << (uint32_t)mcs << " distance=" << dist);
                double tp = RunOneSimulation(dist, nStreams, mcs, curve, allResults.size());
                //NS_LOG_UNCOND("  throughput=" << tp << " Mbps");
                curve.x.push_back(dist);
                curve.y.push_back(tp);
            }
            allResults.push_back(curve);
        }
    }

    // Gnuplot output
    std::string dataFilename = "wifi-ht-mimo-throughput.plt";
    std::ofstream datafile(dataFilename);

    for (const auto& curve : allResults)
    {
        datafile << "# " << curve.label << std::endl;
        for (size_t i=0; i < curve.x.size(); ++i)
            datafile << curve.x[i] << "\t" << curve.y[i] << std::endl;
        datafile << "\n\n";
    }
    datafile.close();

    Gnuplot plot("wifi-ht-mimo-throughput.png");
    plot.SetTitle("802.11n MIMO Throughput vs Distance");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");
    plot.AppendExtra("set xtics nomirror font \",9\"");
    plot.AppendExtra("set ytics nomirror font \",9\"");
    plot.AppendExtra("set grid");

    for (const auto& curve : allResults)
    {
        std::ostringstream cmd;
        cmd << "plot '" << dataFilename << "' index " << (&curve - &allResults[0])
            << " using 1:2 with lines title '" << curve.label << "'";
        Gnuplot2dDataset ds(curve.label);
        ds.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        for (size_t i=0; i < curve.x.size(); ++i)
            ds.Add(curve.x[i], curve.y[i]);
        plot.AddDataset(ds);
    }

    std::ofstream gp("wifi-ht-mimo.gp");
    plot.GenerateOutput(gp);
    gp.close();

    std::cout << "Simulation complete. Data written to " << dataFilename
              << " and Gnuplot script to wifi-ht-mimo.gp.\n"
              << "To plot: gnuplot wifi-ht-mimo.gp" << std::endl;
    return 0;
}