#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessGridSimulation");

int main(int argc, char *argv[]) {
    uint32_t gridRows = 5;
    uint32_t gridCols = 5;
    double gridDistance = 100.0; // meters
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool verbose = false;
    bool pcapTracing = false;
    bool asciiTracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("gridRows", "Number of rows in the grid", gridRows);
    cmd.AddValue("gridCols", "Number of columns in the grid", gridCols);
    cmd.AddValue("gridDistance", "Distance between nodes in meters", gridDistance);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Source node index", sourceNode);
    cmd.AddValue("sinkNode", "Sink node index", sinkNode);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
    cmd.AddValue("ascii", "Enable ASCII tracing", asciiTracing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(gridRows * gridCols);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxPowerStart", DoubleValue(16));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16));
    wifiPhy.Set("TxGain", DoubleValue(0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(gridDistance),
                                  "DeltaY", DoubleValue(gridDistance),
                                  "GridWidth", UintegerValue(gridCols),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);
    stack.SetRoutingHelper(list);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpEchoServer");
    ObjectFactory serverFactory;
    serverFactory.SetTypeId(tid);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        if (i == sinkNode) {
            serverApps.Add(serverFactory.Create<Application>(nodes.Get(i)));
        }
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(interfaces.GetAddress(sinkNode), 9);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(nodes.Get(sourceNode));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    if (pcapTracing) {
        wifiPhy.EnablePcapAll("wireless_grid_simulation");
    }

    if (asciiTracing) {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wireless_grid_simulation.tr"));
    }

    AnimationInterface anim("wireless_grid_simulation.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}