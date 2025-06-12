#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(4);

    // Setup mobility for constant velocity on a straight road
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Assign initial and constant velocity to each vehicle
    for (NodeContainer::Iterator i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
        Ptr<ConstantVelocityMobilityModel> cvmm = mobilityModel->GetObject<ConstantVelocityMobilityModel>();
        if (cvmm) {
            cvmm->SetVelocity(Vector(10.0, 0.0, 0.0));  // Move along X-axis at 10 m/s
        }
    }

    // Configure WiFi in ad-hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfRateControl");
    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(wifiDevices);

    // Set up UDP Echo Server on all nodes to receive packets
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        serverApps = echoServer.Install(vehicles.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(20.0));
    }

    // Set up periodic UDP Echo Clients sending to other nodes
    uint32_t packetSize = 512;
    Time interval = Seconds(1.0);
    uint32_t maxPackets = 20;

    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        for (uint32_t j = 0; j < vehicles.GetN(); ++j) {
            if (i != j) {
                UdpEchoClientHelper echoClient(interfaces.GetAddress(j), port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
                echoClient.SetAttribute("Interval", TimeValue(interval));
                echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

                ApplicationContainer clientApp = echoClient.Install(vehicles.Get(i));
                clientApp.Start(Seconds(2.0 + i));  // staggered start times
                clientApp.Stop(Seconds(20.0));
            }
        }
    }

    // Setup PCAP tracing
    wifiPhy.EnablePcapAll("vehicular_network_simulation");

    // Setup animation output
    AnimationInterface anim("vehicular_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}