#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for 4 vehicles
    NodeContainer nodes;
    nodes.Create(4);

    // Setup WiFi in ad-hoc mode
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Setup mobility models (constant velocity along a straight road)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign constant velocities to each node
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetAttribute("Velocity", Vector3DValue(Vector(20.0, 0.0, 0.0))); // m/s
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP applications for vehicle-to-vehicle communication
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        if (i != 1) { // Send packets to node 1
            Address destAddr = InetSocketAddress(interfaces.GetAddress(1), port);
            onOffHelper onoff("ns3::UdpSocketFactory", destAddr);
            apps.Add(onoff.Install(nodes.Get(i)));
        }
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Install packet sink on node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));

    // Enable NetAnim visualization
    AnimationInterface anim("vehicular_network.xml");
    anim.EnablePacketMetadata(true);

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}