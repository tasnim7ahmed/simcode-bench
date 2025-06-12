#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANET80211pExample");

int main (int argc, char *argv[])
{
    // Enable logging for applications
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes (vehicles)
    NodeContainer nodes;
    nodes.Create (3);

    // Install mobility: ConstantVelocityMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (10.0),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (3),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install (nodes);

    // Set velocity for each vehicle
    Ptr<ConstantVelocityMobilityModel> mobility0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel> ();
    Ptr<ConstantVelocityMobilityModel> mobility1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel> ();
    Ptr<ConstantVelocityMobilityModel> mobility2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel> ();
    mobility0->SetVelocity (Vector (20.0, 0.0, 0.0)); // 20 m/s
    mobility1->SetVelocity (Vector (15.0, 0.0, 0.0)); // 15 m/s
    mobility2->SetVelocity (Vector (25.0, 0.0, 0.0)); // 25 m/s

    // Create a YansWifiChannel
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();

    // Set up 802.11p WAVE devices
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel (waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Set up UDP Echo Server on node 2 (last vehicle), port 9
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer (echoPort);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get(2));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // Set up UDP Echo Client on node 0 (first vehicle)
    UdpEchoClientHelper echoClient (interfaces.GetAddress (2), echoPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get(0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Run the simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}