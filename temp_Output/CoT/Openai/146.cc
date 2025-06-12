#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshWifiUdpGrid");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    uint32_t nNodes = 4;
    double distance = 30.0; // meters
    double simTime = 15.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create (nNodes);

    // Wifi configuration
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    MeshHelper mesh;
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
    mesh.SetNumberOfInterfaces (1);

    NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

    // Mobility: static grid
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (distance, 0.0, 0.0));
    positionAlloc->Add (Vector (0.0, distance, 0.0));
    positionAlloc->Add (Vector (distance, distance, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet stack
    InternetStackHelper internetStack;
    internetStack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    // UDP applications setup: node 0 sends to node 3
    uint16_t port = 9999;
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (nodes.Get (3));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    UdpClientHelper client (interfaces.GetAddress (3), port);
    client.SetAttribute ("MaxPackets", UintegerValue (10000));
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = client.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simTime));

    // PCAP tracing
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    mesh.EnablePcapAll ("mesh-grid");

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Statistics
    uint32_t totalTxPackets = 0, totalRxPackets = 0;
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
        totalTxPackets += iter->second.txPackets;
        totalRxPackets += iter->second.rxPackets;
    }

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Total transmitted packets: " << totalTxPackets << std::endl;
    std::cout << "  Total received packets:    " << totalRxPackets << std::endl;

    Simulator::Destroy ();
    return 0;
}