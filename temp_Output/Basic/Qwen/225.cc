#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5); // 4 vehicles + 1 RSU

    NodeContainer rsuNode = NodeContainer(nodes.Get(0));
    NodeContainer vehicleNodes;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        vehicleNodes.Add(nodes.Get(i));
    }

    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=20.0|Max=30.0]"),
                              "Pause", StringValue("ns3::ConstantVariable[0.0]"));
    mobility.Install(vehicleNodes);

    // Assign fixed positions to RSU
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // RSU at origin
    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i) {
        double x = 100 + i * 50; // Start vehicles ahead of RSU
        positionAlloc->Add(Vector(x, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(rsuNode);
    mobility.Install(vehicleNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(rsuNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(30.0));

    UdpEchoClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(30));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i) {
        clientApps.Add(client.Install(nodes.Get(i + 1)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(30.0));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("vanet-dsdv.tr"));

    AnimationInterface anim("vanet-dsdv.xml");
    anim.SetMobilityPollInterval(Seconds(1));

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}