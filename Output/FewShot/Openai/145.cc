#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three mesh nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure Wi-Fi standard and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up mesh helper
    MeshHelper mesh;
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(phy, nodes);

    // Assign static positions
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    positionAlloc->Add(Vector(2.5, 4.33, 0.0)); // equilateral triangle
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Set up UDP Echo Server on node 2
    uint16_t port = 9999;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on node 0 and node 1 targeting node 2
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Add(echoClient.Install(nodes.Get(1)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Enable PCAP tracing
    phy.EnablePcapAll("mesh-pcap");

    // Set up FlowMonitor for statistics
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::cout << "Flow statistics:" << std::endl;
    for (const auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << ", Src Addr: " << t.sourceAddress << ", Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
    }

    Simulator::Destroy();
    return 0;
}