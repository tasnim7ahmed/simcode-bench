#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpExample");

void
PacketLossTracer(std::string context, Ptr<const Packet> packet)
{
    // Simple tracer for demonstration; user can expand as needed
    NS_LOG_UNCOND("Packet loss detected: " << context);
}

int
main(int argc, char *argv[])
{
    // Command line parameters
    uint32_t nNodes = 4;
    uint32_t serverNodeId = 0;   // Node 0 is the central server
    double simTime = 10.0;       // seconds
    std::string dataRate = "100Mbps";
    std::string delayTime = "2ms";
    uint16_t port = 50000;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(dataRate));
    csma.SetChannelAttribute("Delay", StringValue(delayTime));

    // Connect nodes to CSMA
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install packet loss tracer (simulate by enabling receive error model on server device)
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.01)); // 1% packet loss
    devices.Get(serverNodeId)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacRxDrop", MakeCallback(&PacketLossTracer));

    // Create TCP server on node 0
    uint32_t serverIndex = serverNodeId; // node 0
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(serverIndex), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(serverIndex));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime + 1));

    // Install bulk send applications on other nodes (clients)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        if (i == serverIndex)
            continue;
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", serverAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        clientApps.Add(bulkSend.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime + 1));

    // Configure NetAnim
    AnimationInterface anim("csma-tcp-netanim.xml");
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), 10 + 30 * i, 50); // Space the nodes out
    }
    anim.UpdateNodeDescription(serverIndex, "CentralServer");

    // Enable flow monitor for throughput/delay/loss metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable logging if desired
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(simTime + 2));
    Simulator::Run();

    // Print flow monitor stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "--------------------------------------" << std::endl;
    std::cout << "Flow statistics:" << std::endl;
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Throughput: "
                  << ((flow.second.rxBytes * 8.0) / (simTime * 1000000.0))
                  << " Mbps\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: "
                      << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets)
                      << " s\n";
        }
        std::cout << "--------------------------------------" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}