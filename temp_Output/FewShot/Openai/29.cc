#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class WifiMultirateExperiment
{
public:
    WifiMultirateExperiment();
    void Configure(int argc, char *argv[]);
    void Run();
    void Report();

private:
    void SetupMobility();
    void SetupWifi();
    void SetupApplications();
    void SetupTracing();
    void CalculateThroughput();
    void OnPacketReceive(Ptr<const Packet> pkt, const Address &addr);

    uint32_t m_nNodes;
    double m_simTime;
    double m_areaSize;
    uint32_t m_nFlows;
    std::string m_phyMode;
    std::string m_outputFile;

    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    Gnuplot m_gnuplot;
    std::vector<uint64_t> m_rxBytes;
    std::vector<Ptr<Application>> m_sinkApps;
    std::vector<Ptr<Application>> m_onOffApps;
    EventId m_throughputEvent;
};

WifiMultirateExperiment::WifiMultirateExperiment()
    : m_nNodes(100),
      m_simTime(60.0),
      m_areaSize(300.0),
      m_nFlows(10),
      m_phyMode("DsssRate11Mbps"),
      m_outputFile("wifi-multirate-throughput.plt"),
      m_gnuplot(m_outputFile, "Throughput vs Time")
{
}

void WifiMultirateExperiment::Configure(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", m_nNodes);
    cmd.AddValue("simTime", "Simulation time (s)", m_simTime);
    cmd.AddValue("areaSize", "Size of the area (m)", m_areaSize);
    cmd.AddValue("nFlows", "Number of simultaneous flows", m_nFlows);
    cmd.AddValue("phyMode", "WiFi PhyMode", m_phyMode);
    cmd.AddValue("outputFile", "Output file for Gnuplot", m_outputFile);
    cmd.Parse(argc, argv);

    m_rxBytes.resize(m_nFlows, 0);
}

void WifiMultirateExperiment::SetupMobility()
{
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaSize) + "]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaSize) + "]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "PositionAllocator", 
        PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(m_nodes);
}

void WifiMultirateExperiment::SetupWifi()
{
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(m_phyMode),
                                "ControlMode", StringValue(m_phyMode));
    wifiMac.SetType("ns3::AdhocWifiMac");

    m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 100);
    stack.SetRoutingHelper(list);
    stack.Install(m_nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    m_interfaces = address.Assign(m_devices);
}

void WifiMultirateExperiment::SetupApplications()
{
    double appStart = 1.0;
    double appStop = m_simTime - 1.0;
    uint16_t port = 5000;
    m_sinkApps.clear();
    m_onOffApps.clear();

    for (uint32_t i = 0; i < m_nFlows; ++i)
    {
        uint32_t src = i % m_nNodes;
        uint32_t dst = (i * 7 + 31) % m_nNodes;
        while (dst == src) // avoid self-flow
            dst = (dst + 1) % m_nNodes;

        // UDP Sink (PacketSink)
        Address sinkAddr(InetSocketAddress(m_interfaces.GetAddress(dst), port + i));
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddr);
        ApplicationContainer sinkApps = sinkHelper.Install(m_nodes.Get(dst));
        sinkApps.Start(Seconds(appStart));
        sinkApps.Stop(Seconds(appStop));
        m_sinkApps.push_back(sinkApps.Get(0));
        
        // UDP Source (OnOff)
        OnOffHelper clientHelper("ns3::UdpSocketFactory", sinkAddr);
        clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
        ApplicationContainer onOffApps = clientHelper.Install(m_nodes.Get(src));
        m_onOffApps.push_back(onOffApps.Get(0));
    }
}

void WifiMultirateExperiment::SetupTracing()
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    for (uint32_t i = 0; i < m_sinkApps.size(); ++i)
    {
        Ptr<Application> app = m_sinkApps[i];
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
        if (sink)
        {
            sink->TraceConnectWithoutContext("Rx", MakeCallback([this, i](Ptr<const Packet> pkt, const Address &addr) {
                m_rxBytes[i] += pkt->GetSize();
            }));
        }
    }
}

void WifiMultirateExperiment::CalculateThroughput()
{
    static double lastTime = 0.0;
    static std::vector<uint64_t> lastRxBytes(m_nFlows, 0);

    double now = Simulator::Now().GetSeconds();
    double interval = now - lastTime;
    if (interval == 0) return; // avoid div by zero

    double sumMbit = 0.0;
    for (uint32_t i = 0; i < m_nFlows; ++i)
    {
        uint64_t bytes = m_rxBytes[i] - lastRxBytes[i];
        double throughput = (bytes * 8.0) / (interval * 1e6); // Mbit/s
        sumMbit += throughput;
        lastRxBytes[i] = m_rxBytes[i];
    }
    double avgThroughput = sumMbit / m_nFlows;

    // Gnuplot: Record throughput over time
    m_gnuplot.AddDataset(GnuplotDataset("AverageThroughput").Add(now, avgThroughput));

    lastTime = now;
    // Schedule next throughput calculation
    if (now < m_simTime)
    {
        m_throughputEvent = Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::CalculateThroughput, this);
    }
}

void WifiMultirateExperiment::OnPacketReceive(Ptr<const Packet> pkt, const Address &addr)
{
    // This is handled in the SetupTracing method.
}

void WifiMultirateExperiment::Run()
{
    m_nodes.Create(m_nNodes);

    SetupMobility();
    SetupWifi();
    SetupApplications();
    SetupTracing();

    // Setup FlowMonitor for detailed statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::CalculateThroughput, this);

    Simulator::Stop(Seconds(m_simTime));
    Simulator::Run();

    // Flow monitor report
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto &it : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it.first);
        std::cout << "Flow " << it.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << it.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << it.second.rxPackets << "\n";
        std::cout << "  Throughput: "
                  << it.second.rxBytes * 8.0 / (m_simTime * 1e6) << " Mbit/s\n";
    }

    Report();

    Simulator::Destroy();
}

void WifiMultirateExperiment::Report()
{
    // Export gnuplot file
    std::ofstream plotFile(m_outputFile);
    m_gnuplot.GenerateOutput(plotFile);
    plotFile.close();
    std::cout << "Results written to " << m_outputFile << std::endl;
}

int main(int argc, char *argv[])
{
    WifiMultirateExperiment experiment;
    experiment.Configure(argc, argv);
    experiment.Run();

    std::cout << "Build: ./waf build\n";
    std::cout << "Run:   ./waf --run \"<your-script-name> --nNodes=100 --simTime=60\"\n";
    std::cout << "Debug: ./waf --run \"<your-script-name> --logging\"\n";
    return 0;
}