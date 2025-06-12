#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging if needed (optional)
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 7 nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-point links: n0-n2 and n1-n2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // CSMA segment: n2, n3, n4, n5
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NodeContainer csmaNodes(nodes.Get(2), nodes.Get(3), nodes.Get(4), nodes.Get(5));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Point-to-point link: n5-n6
    NetDeviceContainer dev56 = p2p.Install(nodes.Get(5), nodes.Get(6));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer intf02 = address.Assign(dev02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer intf12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer intfCsma = address.Assign(csmaDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer intf56 = address.Assign(dev56);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR/UDP flow from n0 to n6
    uint16_t port = 9;

    // Receiver on n6
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(6));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Sender on n0
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(intf56.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));
    onoff.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing for all channels
    pointToPoint.EnablePcapAll("mixed_network");
    csma.EnablePcapAll("mixed_network");

    // Trace network queues and packet receptions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed_network_queue.tr");

    for (NodeContainer::Iterator node = nodes.Begin(); node != nodes.End(); ++node) {
        for (uint32_t i = 0; i < (*node)->GetNDevices(); ++i) {
            std::ostringstream oss;
            oss << "/NodeList/" << (*node)->GetId() << "/DeviceList/" << i << "/TxQueue/Enqueue";
            Config::Connect(oss.str(), MakeBoundCallback(&Ns3TcpQueueDiscItemEnqueueTrace, stream));
        }
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}