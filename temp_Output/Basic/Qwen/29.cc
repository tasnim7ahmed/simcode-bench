#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiRateExperiment");

class WiFiMultirateExperiment {
public:
    WiFiMultirateExperiment();
    void Run(uint32_t nodeCount, Time duration, std::string rate);
    void SetupApplications();
    void SetupMobility();
    void SetupRouting();
    void MonitorPackets();
    void CalculateThroughput();
    void SetRate(std::string rate);

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    std::vector<Ptr<OnOffApplication>> sources;
    std::vector<Ptr<PacketSink>> sinks;
    Time m_duration;
    std::string m_rate;
};

WiFiMultirateExperiment::WiFiMultirateExperiment() {
    // Constructor
}

void WiFiMultirateExperiment::SetRate(std::string rate) {
    m_rate = rate;
}

void WiFiMultirateExperiment::Run(uint32_t nodeCount, Time duration, std::string rate) {
    m_duration = duration;
    m_rate = rate;

    NS_LOG_INFO("Creating nodes.");
    nodes.Create(nodeCount);

    NS_LOG_INFO("Setting up mobility.");
    SetupMobility();

    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    AodvHelper aodv;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    NS_LOG_INFO("Setting up WiFi.");
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(m_rate), "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    NS_LOG_INFO("Setting up applications.");
    SetupApplications();

    NS_LOG_INFO("Setting up routing.");
    SetupRouting();

    NS_LOG_INFO("Enabling flow monitor.");
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    NS_LOG_INFO("Starting simulation.");
    Simulator::Stop(duration);
    Simulator::Run();

    NS_LOG_INFO("Simulation completed. Calculating throughput.");
    CalculateThroughput();
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
    }

    Simulator::Destroy();
}

void WiFiMultirateExperiment::SetupApplications() {
    uint32_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(m_duration);

        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
        sinks.push_back(sink);

        if (i > 0) {
            OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(i % nodes.GetN()), sinkPort));
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000kbps")));
            onoff.SetAttribute("PacketSize", UintegerValue(1024));

            ApplicationContainer apps = onoff.Install(nodes.Get(i));
            apps.Start(Seconds(1.0));
            apps.Stop(m_duration);

            sources.push_back(DynamicCast<OnOffApplication>(apps.Get(0)));
        }
    }
}

void WiFiMultirateExperiment::SetupMobility() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)));
    mobility.Install(nodes);
}

void WiFiMultirateExperiment::SetupRouting() {
    // AODV already set up in InstallInternetStack()
}

void WiFiMultirateExperiment::CalculateThroughput() {
    Gnuplot gnuplot("throughput.png");
    gnuplot.SetTitle("Throughput Over Time");
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("Time (s)", "Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Throughput");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    double totalBytes = 0;
    double startTime = 0;
    for (auto sink : sinks) {
        totalBytes += sink->GetTotalRx();
    }

    dataset.Add(Simulator::Now().GetSeconds(), (totalBytes * 8.0 / (Simulator::Now().GetSeconds() - startTime)) / 1e6);
    gnuplot.AddDataset(dataset);

    std::ofstream plotFile("throughput.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char *argv[]) {
    uint32_t nodeCount = 100;
    Time duration = Seconds(60);
    std::string rate = "OfdmRate54Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodeCount", "Number of nodes to create", nodeCount);
    cmd.AddValue("duration", "Duration of the simulation", duration);
    cmd.AddValue("rate", "Transmission rate (e.g., OfdmRate54Mbps)", rate);
    cmd.Parse(argc, argv);

    LogComponentEnable("WiFiMultiRateExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    WiFiMultirateExperiment experiment;
    experiment.SetRate(rate);
    experiment.Run(nodeCount, duration, rate);

    return 0;
}