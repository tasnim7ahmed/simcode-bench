#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DCTCPSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer senders, receivers, switches;
    senders.Create(40);
    receivers.Create(40);
    switches.Create(3); // T1, T2, R1

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(receivers);
    stack.Install(switches);

    // Configure point-to-point links
    PointToPointHelper senderSwitchLink;
    senderSwitchLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    senderSwitchLink.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper switchSwitchLink;
    switchSwitchLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    switchSwitchLink.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper switchReceiverLink;
    switchReceiverLink.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    switchReceiverLink.SetChannelAttribute("Delay", StringValue("10us"));

    // Install net devices and assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");

    NetDeviceContainer senderT1Devices, T1T2Devices, T2R1Devices, R1ReceiverDevices;
    Ipv4InterfaceContainer senderT1Interfaces, T1T2Interfaces, T2R1Interfaces, R1ReceiverInterfaces;

    // Sender to T1
    for (uint32_t i = 0; i < 30; ++i) {
        NetDeviceContainer devices = senderSwitchLink.Install(senders.Get(i), switches.Get(0));
        senderT1Devices.Add(devices.Get(0));
        senderT1Devices.Add(devices.Get(1));
    }
    for (uint32_t i = 30; i < 40; ++i) {
         NetDeviceContainer devices = senderSwitchLink.Install(senders.Get(i), switches.Get(1));
         senderT1Devices.Add(devices.Get(0));
         senderT1Devices.Add(devices.Get(1));
    }
    senderT1Interfaces = address.Assign(senderT1Devices);
    address.NewNetwork();

    // T1 to T2
    NetDeviceContainer devicesT1T2 = switchSwitchLink.Install(switches.Get(0), switches.Get(1));
    T1T2Devices.Add(devicesT1T2);
    T1T2Interfaces = address.Assign(T1T2Devices);
    address.NewNetwork();

    // T2 to R1
    NetDeviceContainer devicesT2R1 = switchSwitchLink.Install(switches.Get(1), switches.Get(2));
    T2R1Devices.Add(devicesT2R1);
    T2R1Interfaces = address.Assign(T2R1Devices);
    address.NewNetwork();

    // R1 to Receiver
    for (uint32_t i = 0; i < 20; ++i) {
        NetDeviceContainer devices = switchReceiverLink.Install(switches.Get(2), receivers.Get(i));
        R1ReceiverDevices.Add(devices.Get(0));
        R1ReceiverDevices.Add(devices.Get(1));
    }
    for (uint32_t i = 20; i < 40; ++i){
         NetDeviceContainer devices = switchReceiverLink.Install(switches.Get(2), receivers.Get(i));
         R1ReceiverDevices.Add(devices.Get(0));
         R1ReceiverDevices.Add(devices.Get(1));
    }
    R1ReceiverInterfaces = address.Assign(R1ReceiverDevices);
    address.NewNetwork();

    // Configure RED queue with ECN marking
    TrafficControlHelper tch;
    QueueDiscContainer queueDiscs;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    tch.SetQueueLimits(QueueSize("2000p"));
    tch.SetRedAttribute("Marking", BooleanValue(true));
    tch.SetRedAttribute("MeanPktSize", UintegerValue(500));

    queueDiscs.Add(tch.Install(T1T2Devices.Get(1)));
    queueDiscs.Add(tch.Install(T2R1Devices.Get(1)));

    // Create TCP applications
    uint16_t port = 50000;
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(R1ReceiverInterfaces.GetAddress(1), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer senderApps[40];
    for (uint32_t i = 0; i < 40; ++i) {
        bulkSendHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(R1ReceiverInterfaces.GetAddress((i<20) ? (2*i +1): (2*i -39)), port)));
        senderApps[i] = bulkSendHelper.Install(senders.Get(i));
        senderApps[i].Start(Seconds(i * 0.025)); // Stagger start times (1 second startup window)
        senderApps[i].Stop(Seconds(5.0));
    }

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer receiverApps[40];
    for(uint32_t i=0; i<40; ++i){
        receiverApps[i] = packetSinkHelper.Install(receivers.Get(i));
        receiverApps[i].Start(Seconds(0.0));
        receiverApps[i].Stop(Seconds(5.0));
    }

    // Enable global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // AnimationInterfaceHelper anim;
    // anim.SetOutputFile("dctcp.xml");
    // Run simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();

    // Calculate throughput
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") : Throughput: " << throughput << " Mbps" << std::endl;
        totalThroughput += throughput;
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
    // Calculate fairness (Jain's fairness index)
    double sum = 0.0;
    double sumSquares = 0.0;
    int numFlows = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() > 0)
        {
            double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
            sum += throughput;
            sumSquares += throughput * throughput;
            numFlows++;
        }

    }

    double fairnessIndex = (sum * sum) / (numFlows * sumSquares);
    std::cout << "Jain's Fairness Index: " << fairnessIndex << std::endl;
    flowMonitor->SerializeToXmlFile("dctcp.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}