#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultirateWiFiExperiment");

class MultirateWiFiExperiment {
public:
    MultirateWiFiExperiment();
    ~MultirateWiFiExperiment();

    void Run(uint32_t numNodes, uint32_t numFlows, double simulationTime, bool enableLogging);
    void SetupApplications();
    void SetupMobility();
    void CalculateThroughput();
    void MonitorPackets(Ptr<const Packet> packet, const Address &from, const Address &to);
    void SetupRouting();

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    std::vector<Ptr<Socket>> udpSockets;
    uint64_t totalRxBytes;
    double lastCalculationTime;
};

MultirateWiFiExperiment::MultirateWiFiExperiment()
    : totalRxBytes(0), lastCalculationTime(0.0)
{
}

MultirateWiFiExperiment::~MultirateWiFiExperiment()
{
}

void MultirateWiFiExperiment::Run(uint32_t numNodes, uint32_t numFlows, double simulationTime, bool enableLogging)
{
    if (enableLogging) {
        LogComponentEnable("MultirateWiFiExperiment", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("Creating nodes.");
    nodes.Create(numNodes);

    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate54Mbps"),
                                 "ControlMode", StringValue("OfdmRate24Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    devices = wifi.Install(phy, mac, nodes);

    SetupMobility();

    InternetStackHelper stack;
    AodvHelper aodv;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    SetupApplications();

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&MultirateWiFiExperiment::MonitorPackets, this));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
}

void MultirateWiFiExperiment::SetupMobility()
{
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

void MultirateWiFiExperiment::SetupApplications()
{
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
}

void MultirateWiFiExperiment::CalculateThroughput()
{
    double now = Simulator::Now().GetSeconds();
    double interval = now - lastCalculationTime;
    double throughput = (totalRxBytes * 8.0) / (interval * 1e6);
    NS_LOG_INFO("Throughput: " << throughput << " Mbps");
    totalRxBytes = 0;
    lastCalculationTime = now;
}

void MultirateWiFiExperiment::MonitorPackets(Ptr<const Packet> packet, const Address &from, const Address &to)
{
    totalRxBytes += packet->GetSize();
    if ((Simulator::Now().GetSeconds() - lastCalculationTime) >= 1.0) {
        CalculateThroughput();
    }
}

void GenerateGnuplotScript()
{
    Gnuplot plot("throughput.plt");
    plot.SetTitle("Throughput over Time");
    plot.SetXLabel("Time (s)");
    plot.SetYLabel("Throughput (Mbps)");

    GnuplotDataset dataset;
    dataset.SetTitle("Throughput");
    dataset.SetStyle(GnuplotDataset::LINES_POINTS);

    // Dummy data for illustration; actual simulation would log and parse results
    dataset.Add(0, 0);
    dataset.Add(1, 1.2);
    dataset.Add(2, 2.5);
    dataset.Add(3, 3.1);
    dataset.Add(4, 2.9);
    dataset.Add(5, 3.5);

    plot.AddDataset(dataset);
    std::ofstream plotFile("throughput.plot");
    plot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 100;
    uint32_t numFlows = 1;
    double simulationTime = 10.0;
    bool enableLogging = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the network.", numNodes);
    cmd.AddValue("numFlows", "Number of simultaneous UDP flows.", numFlows);
    cmd.AddValue("simulationTime", "Duration of the simulation in seconds.", simulationTime);
    cmd.AddValue("enableLogging", "Enable logging output.", enableLogging);
    cmd.Parse(argc, argv);

    MultirateWiFiExperiment experiment;
    experiment.Run(numNodes, numFlows, simulationTime, enableLogging);

    GenerateGnuplotScript();

    return 0;
}