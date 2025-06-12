#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Mesh2x2GridExample");

void
PacketTx(Ptr<const Packet> packet)
{
    static uint32_t txPackets = 0;
    txPackets++;
    Simulator::Schedule(Seconds(15.0), [](){
        std::cout << "Total Packets Sent: " << txPackets << std::endl;
    });
}

void
PacketRx(Ptr<const Packet> packet, const Address &)
{
    static uint32_t rxPackets = 0;
    rxPackets++;
    Simulator::Schedule(Seconds(15.0), [](){
        std::cout << "Total Packets Received: " << rxPackets << std::endl;
    });
}

int
main(int argc, char *argv[])
{
    uint32_t nNodes = 4;
    double gridStep = 50.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(nNodes);

    // Set mobility (2x2 grid)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridStep),
                                 "DeltaY", DoubleValue(gridStep),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Set up Wi-Fi mesh
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    // Internet stack and IP addresses
    InternetStackHelper internetStack;
    internetStack.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP Application setup
    uint16_t port = 5000;

    // Setup a UDP server on node 3
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(meshNodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // UDP clients - all nodes except node 3 send packets to node 3
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds(0.1);

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        if (i == 3)
            continue;
        UdpClientHelper client(interfaces.GetAddress(3), port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = client.Install(meshNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(15.0));
    }

    // Statistics and tracing
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&PacketTx));
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketRx));

    wifiPhy.EnablePcapAll("mesh2x2-grid");

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
    }

    Simulator::Destroy();
    return 0;
}