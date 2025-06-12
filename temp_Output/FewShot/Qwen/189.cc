#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for relevant components
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes (1 server, 3 clients)
    NodeContainer nodes;
    nodes.Create(4);

    // Use CsmaHelper to connect all nodes in a shared CSMA network
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP receiver (server) on node 0
    uint16_t port = 5000;
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP senders (clients) on nodes 1, 2, and 3
    for (uint32_t i = 1; i < 4; ++i) {
        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable flow monitor to collect metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Setup NetAnim for visualization
    AnimationInterface anim("csma_simulation.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);   // Server at center
    anim.SetConstantPosition(nodes.Get(1), -40, 30); // Client 1
    anim.SetConstantPosition(nodes.Get(2), 40, 30);  // Client 2
    anim.SetConstantPosition(nodes.Get(3), 0, 60);   // Client 3

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow monitor results
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("csma_simulation.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}