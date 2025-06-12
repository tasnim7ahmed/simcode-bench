#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpSimulation");

void
PacketLossCallback(std::string context, Ptr<const Packet> packet)
{
    static uint32_t lostPackets = 0;
    lostPackets++;
    NS_LOG_UNCOND("Packet lost at " << Simulator::Now().GetSeconds() << "s, total lost: " << lostPackets);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Number of CSMA nodes: 3 clients + 1 server
    uint32_t nClients = 3;
    NodeContainer nodes;
    nodes.Create(nClients + 1); // Last node is server

    // Set up CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Enable error model for loss
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.01)); // 1% loss
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        devices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install PacketSink on server (node nClients)
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(nClients), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(nClients));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Install BulkSend Apps on clients
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nClients; ++i)
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        bulkSend.SetAttribute("SendSize", UintegerValue(1024));
        ApplicationContainer app = bulkSend.Install(nodes.Get(i));
        app.Start(Seconds(1.0 + i)); // staggered start
        app.Stop(Seconds(20.0));
        clientApps.Add(app);
    }

    // Throughput and delay measurement using FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Trace packet loss
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacRxDrop", MakeCallback(&PacketLossCallback));

    // NetAnim setup
    AnimationInterface anim("csma-tcp-netanim.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), 20 + 70*i, 50);
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // FlowMonitor statistics
    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    uint64_t totalRxPackets = 0;
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == port)
        {
            double timeSpan = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
            double throughput = (flow.second.rxBytes * 8.0) / timeSpan / 1024 / 1024; // Mbps
            totalThroughput += throughput;
            totalDelay += flow.second.delaySum.GetSeconds();
            totalRxPackets += flow.second.rxPackets;
        }
    }

    NS_LOG_UNCOND("Total throughput: " << totalThroughput << " Mbps");
    NS_LOG_UNCOND("Average delay: " << (totalRxPackets ? totalDelay / totalRxPackets : 0) << " s");
    NS_LOG_UNCOND("Total received packets: " << totalRxPackets);

    Simulator::Destroy();
    return 0;
}