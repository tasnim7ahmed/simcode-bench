#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpNetAnimExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("CsmaTcpNetAnimExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // clients: 0,1,2; server: 3

    // CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install applications
    uint16_t port = 50000;
    ApplicationContainer serverApp;

    // TCP Sink (server) on node 3
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    serverApp = packetSinkHelper.Install(nodes.Get(3));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // TCP OnOffApp clients on nodes 0,1,2 sending to node 3
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 3; ++i)
    {
        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(3), port));
        client.SetAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        client.SetAttribute("MaxBytes", UintegerValue(0));
        client.SetAttribute("StartTime", TimeValue(Seconds(1.0 + i * 0.5)));
        client.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
        clientApps.Add(client.Install(nodes.Get(i)));
    }

    // Enable pcap tracing for CSMA (optional)
    csma.EnablePcapAll("csma-tcp-netanim", true);

    // NetAnim setup
    AnimationInterface anim("csma-tcp-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 30.0);
    anim.SetConstantPosition(nodes.Get(1), 30.0, 30.0);
    anim.SetConstantPosition(nodes.Get(2), 50.0, 30.0);
    anim.SetConstantPosition(nodes.Get(3), 30.0, 10.0);

    // FlowMonitor
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Calculate metrics
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    double totalThroughput = 0;
    uint32_t totalLostPackets = 0;
    double totalDelay = 0;
    uint32_t receivedFlows = 0;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationAddress == interfaces.GetAddress(3))
        {
            double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - 1.0) / 1e6;
            totalThroughput += throughput;
            totalLostPackets += iter->second.lostPackets;
            if (iter->second.rxPackets > 0)
            {
                totalDelay += iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
                receivedFlows++;
            }
        }
    }

    NS_LOG_INFO("Total throughput (Mbit/s): " << totalThroughput);
    NS_LOG_INFO("Total packet loss: " << totalLostPackets);
    NS_LOG_INFO("Average end-to-end delay (s): " << (receivedFlows ? totalDelay / receivedFlows : 0));

    Simulator::Destroy();
    return 0;
}