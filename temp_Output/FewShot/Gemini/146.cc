#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("MeshWifiExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Configure WiFi and Mesh
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller("dot11s");
    mesh.SetSpreadInterfaceChannels(true);

    // Disable fragmentation ( Fragmentation doesn't work well with some adhoc routing protocols)
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    QosWifiMacHelper wifiMac = QosWifiMacHelper::Default ();

    NetDeviceContainer meshDevices = mesh.Install(wifi, wifiMac, meshNodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Mobility Model - Static positions in a 2x2 grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (10.0),
                                     "DeltaY", DoubleValue (10.0),
                                     "GridWidth", UintegerValue (2),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Create UDP server on node 3
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(meshNodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // Create UDP client on node 0, sending to node 3
    UdpClientHelper client(interfaces.GetAddress(3), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(meshNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Setup PCAP tracing
    wifi.EnablePcapAll("mesh-wifi-example");

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    // Print per flow statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
      }

    Simulator::Destroy();

    return 0;
}