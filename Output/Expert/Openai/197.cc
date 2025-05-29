#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static uint32_t txPackets[3] = {0};
static uint32_t rxPackets[3] = {0};

void TxTracePtr(std::string context, Ptr<const Packet> p)
{
    if (context.find("/NodeList/0/") != std::string::npos)
        txPackets[0]++;
    else if (context.find("/NodeList/1/") != std::string::npos)
        txPackets[1]++;
    else if (context.find("/NodeList/2/") != std::string::npos)
        txPackets[2]++;
}

void RxTracePtr(std::string context, Ptr<const Packet> p, const Address &)
{
    if (context.find("/NodeList/0/") != std::string::npos)
        rxPackets[0]++;
    else if (context.find("/NodeList/1/") != std::string::npos)
        rxPackets[1]++;
    else if (context.find("/NodeList/2/") != std::string::npos)
        rxPackets[2]++;
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(i1i2.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    // Packet tracing
    Config::Connect("/NodeList/*/DeviceList/*/MacTx", MakeCallback(&TxTracePtr));
    Config::Connect("/NodeList/*/DeviceList/*/MacRx", MakeCallback(&RxTracePtr));

    // Enable pcap
    p2p.EnablePcapAll("wired-p2p", true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    std::cout << "Packets TXed: Node0=" << txPackets[0]
              << " Node1=" << txPackets[1]
              << " Node2=" << txPackets[2] << std::endl;
    std::cout << "Packets RXed: Node0=" << rxPackets[0]
              << " Node1=" << rxPackets[1]
              << " Node2=" << rxPackets[2] << std::endl;

    Simulator::Destroy();
    return 0;
}