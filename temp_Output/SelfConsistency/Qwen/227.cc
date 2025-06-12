#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavAodvSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create UAV and GCS nodes
    NodeContainer uavNodes;
    uavNodes.Create(3);  // Three UAVs

    NodeContainer gcsNode;
    gcsNode.Create(1);   // One GCS

    NodeContainer allNodes = NodeContainer(uavNodes, gcsNode);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    // Create WiFi channel
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifiHelper;

    wifiHelper.SetStandard(WIFI_STANDARD_80211a);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6Mbps"),
                                       "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    wifiPhy.SetChannel(WifiChannelHelper::DefaultChannel());

    NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP communication from UAVs to GCS
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(60.0));

    UdpClientHelper client(interfaces.GetAddress(3), port); // GCS IP address
    client.SetAttribute("MaxPackets", UintegerValue(100000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(uavNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(60.0));

    // Mobility model (3D)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk3dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Value=2]"));
    mobility.Install(allNodes);

    Simulator::Stop(Seconds(60.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("uav_aodv_simulation");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}