#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavAodvSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 GCS + multiple UAVs
    NodeContainer gcsNode;
    gcsNode.Create(1);

    NodeContainer uavNodes;
    uavNodes.Create(5); // Example: 5 UAVs

    NodeContainer allNodes = NodeContainer(gcsNode, uavNodes);

    // Setup mobility for UAVs and GCS
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(allNodes);

    // Set initial positions and velocities for UAVs in 3D
    for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
        Ptr<Node> node = uavNodes.Get(i);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        pos.z = 100.0; // UAVs at 100m altitude
        mobility->SetPosition(pos);
        mobility->SetVelocity(Vector(5.0, 0.0, 0.0)); // Move along X-axis
    }

    // GCS remains stationary on ground
    {
        Ptr<Node> node = gcsNode.Get(0);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        pos.z = 0.0; // Ground level
        mobility->SetPosition(pos);
    }

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiPhy.Set("ChannelCenterFrequency", UintegerValue(5180)); // 5GHz
    wifiPhy.Set("TxPowerEnd", DoubleValue(15.0));
    wifiPhy.Set("TxPowerStart", DoubleValue(15.0));
    wifiPhy.Set("RxGain", DoubleValue(0));

    // Ad hoc MAC
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Setup AODV routing
    AodvHelper aodv;
    aodv.Set("EnableHello", BooleanValue(true));
    aodv.Set("HelloInterval", TimeValue(Seconds(1.0)));
    aodv.Set("RreqRetries", UintegerValue(2));

    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install applications: UDP echo server on GCS, clients on UAVs
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(60.0));

    for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
        UdpEchoClientHelper client(interfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(20));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(uavNodes.Get(i));
        clientApps.Start(Seconds(2.0 + i));
        clientApps.Stop(Seconds(60.0));
    }

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("uav_aodv_simulation");

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}