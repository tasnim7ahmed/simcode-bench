#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshTopologyExample");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (4);

    // Configure Mesh Helper
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    MeshHelper mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
    mesh.SetNumberOfInterfaces (1);

    // Install mesh devices
    NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

    // Set Mobility: static 2x2 grid, 50 meters apart
    MobilityHelper mobility;
    mobility.SetPositionAllocator (
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue (0.0),
        "MinY", DoubleValue (0.0),
        "DeltaX", DoubleValue (50.0),
        "DeltaY", DoubleValue (50.0),
        "GridWidth", UintegerValue (2),
        "LayoutType", StringValue ("RowFirst")
    );
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Install Internet Stack
    InternetStackHelper internetStack;
    internetStack.Install (nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    // Install UDP traffic: Node 0 (client) -> Node 3 (server)
    uint16_t udpPort = 9000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApp = udpServer.Install (nodes.Get (3));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (15.0));

    UdpClientHelper udpClient (interfaces.GetAddress (3), udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
    udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApp = udpClient.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (15.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll ("mesh");

    // Install FlowMonitor to collect statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (15.0));
    Simulator::Run ();

    // Print statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    uint32_t totalTxPackets = 0, totalRxPackets = 0;
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
        NS_LOG_UNCOND ("Flow ID: " << flow.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND ("  Tx Packets: " << flow.second.txPackets);
        NS_LOG_UNCOND ("  Rx Packets: " << flow.second.rxPackets);
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
    }
    NS_LOG_UNCOND ("Total Tx Packets: " << totalTxPackets);
    NS_LOG_UNCOND ("Total Rx Packets: " << totalRxPackets);

    Simulator::Destroy ();
    return 0;
}