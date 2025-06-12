#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpDataset");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    uint32_t numNodes = 4;
    std::string csvFileName = "ring_topology_udp_dataset.csv";

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[numNodes];

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[numNodes];

    for (uint32_t i = 0; i < numNodes; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[1].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer2(9);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(1), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer3(9);
    ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(3));
    serverApps3.Start(Seconds(1.0));
    serverApps3.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient3(interfaces[3].GetAddress(1), 9);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(256));

    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(2));
    clientApps3.Start(Seconds(3.0));
    clientApps3.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer0(9);
    ApplicationContainer serverApps0 = echoServer0.Install(nodes.Get(0));
    serverApps0.Start(Seconds(1.0));
    serverApps0.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient0(interfaces[0].GetAddress(1), 9);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(2048));

    ApplicationContainer clientApps0 = echoClient0.Install(nodes.Get(3));
    clientApps0.Start(Seconds(3.5));
    clientApps0.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(csvFileName);
    *stream->GetStream() << "source_node,destination_node,packet_size,tx_time,rx_time\n";

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeBoundCallback(
        [&stream](Ptr<const Packet> packet, const Address &address, uint32_t src, uint32_t dst, double txTime) {
            *stream->GetStream() << src << "," << dst << "," << packet->GetSize() << "," << txTime << ",0\n";
        },
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5
    ));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx", MakeBoundCallback(
        [&stream](Ptr<const Packet> packet, const Address &address, uint32_t dst, uint32_t src, double rxTime) {
            *stream->GetStream() << src << "," << dst << "," << packet->GetSize() << ",0," << rxTime << "\n";
        },
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5
    ));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}