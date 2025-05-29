#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;
    double simTime = 10.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devs = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devs);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    ApplicationContainer serverApp;
    {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApp = sinkHelper.Install(nodes.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime + 1));
    }

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
        onoff.SetAttribute("DataRate", StringValue("50Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + i * 0.1)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        onoff.SetAttribute("MaxBytes", UintegerValue(0));
        clientApps.Add(onoff.Install(nodes.Get(i)));
    }

    // Set up packet loss by configuring an ErrorModel on the CSMA devices (not the server)
    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
    errorModel->SetAttribute("ErrorRate", DoubleValue(0.01)); // 1% loss
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        devs.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));
    }

    // Enable NetAnim
    AnimationInterface anim("csma-animation.xml");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    // Collect and print statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalLost = 0;
    double totalDelay = 0.0;
    uint32_t totalRxPackets = 0;
    double totalThroughput = 0.0;
    uint32_t countedFlows = 0;

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == interfaces.GetAddress(0))
        {
            totalLost += flow.second.lostPackets;
            if (flow.second.rxPackets > 0)
            {
                totalDelay += flow.second.delaySum.GetSeconds();
                totalRxPackets += flow.second.rxPackets;
                ++countedFlows;
            }
            double throughput = flow.second.rxBytes * 8.0 / (simTime * 1000000.0); // Mbit/s
            totalThroughput += throughput;
        }
    }
    if (countedFlows > 0 && totalRxPackets > 0)
    {
        std::cout << "Average Packet Loss: " << double(totalLost) / countedFlows << std::endl;
        std::cout << "Average Delay (s): " << (totalDelay / totalRxPackets) << std::endl;
        std::cout << "Average Throughput (Mbit/s): " << (totalThroughput / countedFlows) << std::endl;
    }

    Simulator::Destroy();
    return 0;
}