#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    Ptr<BulkSendApplication> bulkSendSource = CreateObject<BulkSendApplication>();
    bulkSendSource->SetSocket(ns3TcpSocket);
    bulkSendSource->SetSendSize(1448);
    bulkSendSource->SetMaxBytes(0);
    bulkSendSource->SetRemote(sinkAddress);

    nodes.Get(0)->AddApplication(bulkSendSource);
    bulkSendSource->SetStartTime(Seconds(2.0));
    bulkSendSource->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}