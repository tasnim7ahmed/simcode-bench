#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMeshGridExample");

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // Number of mesh nodes in 2x2 grid
    uint32_t gridSize = 2;
    uint32_t nNodes = gridSize * gridSize;
    double gridStep = 30.0;

    NodeContainer meshNodes;
    meshNodes.Create(nNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper mesh;
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    // Set static positions in 2x2 grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (gridStep),
                                  "DeltaY", DoubleValue (gridStep),
                                  "GridWidth", UintegerValue (gridSize),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Install internet
    InternetStackHelper internetStack;
    internetStack.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Install UDP server on last node
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(meshNodes.Get(nNodes-1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // Install UDP client on first node, sending packets to last node
    UdpClientHelper udpClient(interfaces.GetAddress(nNodes-1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(50));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.25)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(meshNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Enable PCAP tracing on mesh devices
    wifiPhy.EnablePcapAll("wifi-mesh-grid");

    // Flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
    for (auto i = stats.begin (); i != stats.end (); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    }

    Simulator::Destroy();
    return 0;
}