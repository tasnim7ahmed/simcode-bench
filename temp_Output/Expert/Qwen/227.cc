#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UAVSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: UAVs and GCS
    NodeContainer uavNodes;
    uavNodes.Create(5);  // 5 UAVs
    NodeContainer gcsNode;
    gcsNode.Create(1);   // Ground Control Station

    NodeContainer allNodes = NodeContainer(uavNodes, gcsNode);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility model for UAVs (3D)
    MobilityHelper mobilityUAV;
    mobilityUAV.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(20.0),
                                     "DeltaY", DoubleValue(20.0),
                                     "GridWidth", UintegerValue(5),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityUAV.SetMobilityModel("ns3::RandomWalk3dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                                 "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    mobilityUAV.Install(uavNodes);

    // Mobility model for GCS (fixed)
    MobilityHelper mobilityGCS;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // GCS at ground level
    mobilityGCS.SetPositionAllocator(positionAlloc);
    mobilityGCS.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGCS.Install(gcsNode);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on GCS
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(60.0));

    // UDP Echo Clients on UAVs
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    UdpEchoClientHelper echoClient(interfaces.GetAddress(5), 9); // GCS IP is index 5
    echoClient.SetAttribute("MaxPackets", UintegerValue(59));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
        clientApps.Add(echoClient.Install(uavNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(60.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("uav_simulation");

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}