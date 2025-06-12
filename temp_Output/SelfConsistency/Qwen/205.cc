#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create 4 nodes representing vehicles
    NodeContainer vehicles;
    vehicles.Create(4);

    // Set up mobility for constant velocity along a straight road (x-axis)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Start moving the vehicles along x-axis
    for (NodeContainer::Iterator i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
        Ptr<ConstantVelocityMobilityModel> cvModel = DynamicCast<ConstantVelocityMobilityModel>(model);
        if (cvModel) {
            cvModel->SetVelocity(Vector(10.0, 0.0, 0.0)); // m/s
        }
    }

    // Setup WiFi in ad-hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfRateControl");

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(wifiDevices);

    // Set up UDP echo server on all nodes
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP echo clients on all pairs
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        for (uint32_t j = 0; j < vehicles.GetN(); ++j) {
            if (i != j) {
                UdpEchoClientHelper client(interfaces.GetAddress(j), port);
                client.SetAttribute("MaxPackets", UintegerValue(100));
                client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
                client.SetAttribute("PacketSize", UintegerValue(512));

                ApplicationContainer clientApp = client.Install(vehicles.Get(i));
                clientApp.Start(Seconds(2.0));
                clientApp.Stop(Seconds(10.0));
            }
        }
    }

    // Enable NetAnim visualization
    AnimationInterface anim("vehicular-network.xml");
    anim.EnablePacketMetadata(true);

    // Simulation setup complete
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}