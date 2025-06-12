#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer centralNode;
    centralNode.Create(1);
    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    // Create node containers for point-to-point links
    NodeContainer starNodes[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        starNodes[i] = NodeContainer(centralNode.Get(0), peripheralNodes.Get(i));
    }

    // Create and configure point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i] = pointToPoint.Install(starNodes[i]);
    }

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Install UDP Echo Server on central node
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Clients on peripheral nodes
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(0), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = echoClient.Install(peripheralNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i*0.1));
        clientApp.Stop(Seconds(10.0));
        clientApps.Add(clientApp);
    }

    // Enable pcap tracing
    for (uint32_t i = 0; i < 4; ++i)
    {
        pointToPoint.EnablePcapAll("star-topology-p2p-" + std::to_string(i));
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Collect and log FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.destinationPort != echoPort)
            continue; // Only echo flows

        double pdr = 0;
        if (it->second.txPackets > 0)
            pdr = (double) it->second.rxPackets / it->second.txPackets;

        double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps

        double avgDelay = 0;
        if (it->second.rxPackets > 0)
            avgDelay = it->second.delaySum.GetSeconds() / it->second.rxPackets;

        std::cout << "Flow ID: " << it->first << std::endl;
        std::cout << "  Src Addr: " << t.sourceAddress << " --> Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << pdr << std::endl;
        std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
        std::cout << "  Avg Delay: " << avgDelay << " s" << std::endl;
        std::cout << "=============================" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}