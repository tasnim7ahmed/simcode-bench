#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2VCommunicationExample");

int main (int argc, char *argv[])
{
    // Set simulation parameters
    double simTime = 10.0; // seconds
    double distance = 50.0; // meters between vehicles

    // Enable logging
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create two vehicle nodes
    NodeContainer vehicles;
    vehicles.Create (2);

    // Set up Wi-Fi for vehicular communication using Wave (802.11p)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default ();
    WaveHelper waveHelper = WaveHelper::Default ();
    NetDeviceContainer devices = waveHelper.Install (wifiPhy, wifiMac, vehicles);

    // Set up mobility model for the vehicles
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));  // Position for vehicle 1
    positionAlloc->Add (Vector (distance, 0.0, 0.0));  // Position for vehicle 2
    mobility.SetPositionAllocator (positionAlloc);
    mobility.Install (vehicles);

    // Install the internet stack on the vehicles
    InternetStackHelper internet;
    internet.Install (vehicles);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Set up UDP server on vehicle 2 (Node 1)
    uint16_t port = 4000;
    UdpServerHelper udpServer (port);
    ApplicationContainer serverApp = udpServer.Install (vehicles.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simTime));

    // Set up UDP client on vehicle 1 (Node 0)
    UdpClientHelper udpClient (interfaces.GetAddress (1), port);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (100));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // 1 packet per second
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApp = udpClient.Install (vehicles.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simTime));

    // Set up Flow Monitor to track statistics
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll ();

    // Run the simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Print Flow Monitor statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        NS_LOG_UNCOND ("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND ("Tx Packets: " << i->second.txPackets);
        NS_LOG_UNCOND ("Rx Packets: " << i->second.rxPackets);
    }

    // Clean up
    Simulator::Destroy ();
    return 0;
}

