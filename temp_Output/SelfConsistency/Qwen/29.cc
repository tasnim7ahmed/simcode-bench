#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultirateExperiment");

class WiFiMultirateExperiment {
public:
    WiFiMultirateExperiment();
    void Run(uint32_t numNodes, double simulationTime, double distance);
    void SetupApplications();
    void ReportThroughput();
    void PacketSinkRx(Ptr<const Packet> p, const Address &ad);
    void SetupMobility();

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    std::vector<Ptr<PacketSink>> sinks;
    double totalRxBytes;
    Gnuplot2dDataset throughputDataset;
};

WiFiMultirateExperiment::WiFiMultirateExperiment()
    : totalRxBytes(0.0),
      throughputDataset("Throughput Over Time")
{
    throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
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
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(nodes);
}

void WiFiMultirateExperiment::Run(uint32_t numNodes, double simulationTime, double distance) {
    NS_LOG_INFO("Creating nodes.");
    nodes.Create(numNodes);

    NS_LOG_INFO("Setting up WiFi Ad Hoc network.");
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    NS_LOG_INFO("Installing internet stack with AODV routing.");
    AodvHelper aodv;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(aodv, 0);
    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    SetupMobility();

    NS_LOG_INFO("Setting up applications.");
    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(j), port));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));

                ApplicationContainer apps = onoff.Install(nodes.Get(i));
                apps.Start(Seconds(0.1));
                apps.Stop(Seconds(simulationTime - 0.1));
            }
        }
    }

    for (uint32_t i = 0; i < numNodes; ++i) {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer app = sink.Install(nodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinks.push_back(DynamicCast<PacketSink>(app.Get(0)));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/ApplicationList/0/$ns3::PacketSink/Rx",
                                      MakeCallback(&WiFiMultirateExperiment::PacketSinkRx, this));
    }

    NS_LOG_INFO("Starting simulation.");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    ReportThroughput();
    Simulator::Destroy();
}

void WiFiMultirateExperiment::PacketSinkRx(Ptr<const Packet> p, const Address &ad) {
    totalRxBytes += p->GetSize();
}

void WiFiMultirateExperiment::ReportThroughput() {
    double throughput = totalRxBytes * 8 / 1e6; // Mbps
    NS_LOG_UNCOND("Total Throughput: " << throughput << " Mbps");
    throughputDataset.Add(Simulator::Now().GetSeconds(), throughput);
    Gnuplot gnuplot("throughput.png");
    gnuplot.SetTitle("Throughput Over Time");
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("Time (s)", "Throughput (Mbps)");
    gnuplot.AddDataset(throughputDataset);
    std::ofstream plotFile("throughput.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char *argv[]) {
    uint32_t numNodes = 100;
    double simulationTime = 10.0;
    double distance = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation.", numNodes);
    cmd.AddValue("simulationTime", "Duration of the simulation in seconds.", simulationTime);
    cmd.AddValue("distance", "Distance between nodes.", distance);
    cmd.Parse(argc, argv);

    LogComponentEnable("WiFiMultirateExperiment", LOG_LEVEL_INFO);

    WiFiMultirateExperiment experiment;
    experiment.Run(numNodes, simulationTime, distance);

    return 0;
}