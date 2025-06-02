#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocNetworkExample");

void UpdateNodePositions(NodeContainer &nodes)
{
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Ptr<MobilityModel> mobility = (*it)->GetObject<MobilityModel>();
        double x = rand->GetValue(-100, 100);
        double y = rand->GetValue(-100, 100);
        mobility->SetPosition(Vector(x, y, 0.0));
    }
    Simulator::Schedule(Seconds(5.0), &UpdateNodePositions, nodes);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("AdhocNetworkExample", LOG_LEVEL_INFO);

    int size = 10;
    int step = 50;
    int totalTime = 100;
    bool pcap = true;
    bool printRoutes = true;

    NodeContainer nodes;
    nodes.Create(size);
    CommandLine cmd(__FILE__);

    cmd.AddValue("pcap", "Write PCAP traces.", pcap);
    cmd.AddValue("printRoutes", "Print routing table dumps.", printRoutes);
    cmd.AddValue("size", "Number of nodes.", size);
    cmd.AddValue("time", "Simulation time, s.", totalTime);
    cmd.AddValue("step", "Grid step, m", step);

    cmd.Parse(argc, argv);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    phy.Set("TxGain", DoubleValue(0.0));
    phy.Set("RxGain", DoubleValue(0.0));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(nodes);

    if (pcap)
    {
        phy.EnablePcapAll(std::string("aodv"));
    }
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.2.22.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    if (printRoutes)
    {
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("scratch/routingTable_update.routes", std::ios::out);
        for (int i = 2; i <= totalTime; i += 2)
        {
            aodv.PrintRoutingTableAllAt(Seconds(i), routingStream);
            Simulator::Schedule(Seconds(i), &UpdateNodePositions, nodes); // Schedule position update
        }
    }
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps;

    for (int i = 0; i < size; i++)
    {
        serverApps.Add(server.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(totalTime));
    ApplicationContainer clientApps[size];

    for (int i = 0; i < size; i++)
    {
        UdpEchoClientHelper client(interfaces.GetAddress((i + 1) % size), port);
        client.SetAttribute("MaxPackets", UintegerValue(50));
        client.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        clientApps[i].Add(client.Install(nodes.Get(i)));
    }

    // Start and stop client applications
    for (int i = 0; i < size; i++)
    {
        clientApps[i].Start(Seconds(2.0 + 10 * i));
        clientApps[i].Stop(Seconds(totalTime));
    }

    // Define a flow monitor object
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(totalTime));

    // Animation
    AnimationInterface anim("ten_nodes_aodv.xml");

    // Run simulation
    Simulator::Run();

    // Calculate packet delivery ratio and end-to-end delay and throughput
    uint32_t packetsDelivered = 0;
    uint32_t totalPackets = 0;
    double totalDelay = 0;

    // Retrieve the flow statistics
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto &it : stats)
    {
        FlowId flowId = it.first;
        std::cout << "Flow " << flowId << std::endl;
        std::cout << " Tx Packets: " << it.second.txPackets << std::endl;
        std::cout << " Tx Bytes: " << it.second.txBytes << std::endl;
        std::cout << " Rx Packets: " << it.second.rxPackets << std::endl;
        std::cout << " Rx Bytes: " << it.second.rxBytes << std::endl;

        if (it.second.rxPackets > 0)
        {
            packetsDelivered += it.second.rxPackets;
            totalPackets += it.second.txPackets;
            totalDelay += (it.second.delaySum.ToDouble(ns3::Time::S) / it.second.rxPackets);
        }
    }

    double packetDeliveryRatio = (double)packetsDelivered / totalPackets;
    double averageEndToEndDelay = totalDelay / packetsDelivered;
    double throughput = (double)packetsDelivered * 512 * 8 / (totalTime - 1.0) / 1000000;

    // Print the results
    std::cout << "Packets Delivered Ratio: " << packetDeliveryRatio << std::endl;
    std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << std::endl;
    std::cout << "Throughput: " << throughput << std::endl;

    Simulator::Destroy();

    return 0;
}
