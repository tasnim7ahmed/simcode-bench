#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Install the WAVE (802.11p) protocol
    WifiHelper wifi;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(wifiPhy, waveMac, nodes);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> cvmm0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    cvmm0->SetVelocity(Vector(20, 0, 0));
    Ptr<ConstantVelocityMobilityModel> cvmm1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    cvmm1->SetVelocity(Vector(15, 0, 0));
    Ptr<ConstantVelocityMobilityModel> cvmm2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    cvmm2->SetVelocity(Vector(25, 0, 0));

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP Echo Server on node 2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();

    return 0;
}