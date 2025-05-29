#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshTopologyExample");

int
main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (3);

    // Setup wifi PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    // Setup mesh helper
    MeshHelper mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));

    // Install mesh devices
    NetDeviceContainer meshDevices = mesh.Install (phy, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::ListPositionAllocator",
                                  "PositionList", VectorValue (std::vector<Vector> {
                                      Vector (0.0, 0.0, 0.0),
                                      Vector (50.0, 0.0, 0.0),
                                      Vector (100.0, 0.0, 0.0)
                                  }));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    InternetStackHelper internetStack;
    internetStack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    // UDP server on node 2
    uint16_t port = 9;
    UdpServerHelper udpServer (port);
    ApplicationContainer serverApps = udpServer.Install (nodes.Get (2));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (10.0));

    // UDP client on node 0, sending to node 2
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 20;
    Time interPacketInterval = Seconds (0.5);
    UdpClientHelper udpClient (interfaces.GetAddress (2), port);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    // Additional UDP flow: node 1 -> node 0
    UdpServerHelper udpServer2 (port + 1);
    ApplicationContainer serverApps2 = udpServer2.Install (nodes.Get (0));
    serverApps2.Start (Seconds (0.5));
    serverApps2.Stop (Seconds (10.0));

    UdpClientHelper udpClient2 (interfaces.GetAddress (0), port + 1);
    udpClient2.SetAttribute ("MaxPackets", UintegerValue (15));
    udpClient2.SetAttribute ("Interval", TimeValue (Seconds (0.6)));
    udpClient2.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps2 = udpClient2.Install (nodes.Get (1));
    clientApps2.Start (Seconds (2.0));
    clientApps2.Stop (Seconds (10.0));

    // Enable PCAP tracing
    phy.EnablePcapAll ("mesh-pcap");

    // Install FlowMonitor to collect statistics
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll ();

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();

    // Statistics output
    flowmon->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
    std::cout << "Flow statistics:\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
    }

    Simulator::Destroy ();
    return 0;
}