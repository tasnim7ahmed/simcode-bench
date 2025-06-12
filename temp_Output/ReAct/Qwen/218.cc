#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double simulationTime = 50.0;
    double distance = 500.0; // meters
    double nodeSpeed = 20.0; // m/s
    double nodePause = 0.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes (vehicles) in the VANET.", numNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds).", simulationTime);
    cmd.AddValue("distance", "Maximum distance between nodes (meters).", distance);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiMac.Install(wifiPhy, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance / numNodes),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(numNodes),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
        Ptr<Node> node = (*it);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));
    }

    InternetStackHelper internet;
    dsdv::DsdvHelper dsdv;
    internet.SetRoutingHelper(dsdv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(numNodes - 1), port));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("DataRate", StringValue("256kbps"));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(nodes.Get(numNodes - 1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("vanet-dsdv.tr");
    wifiPhy.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}