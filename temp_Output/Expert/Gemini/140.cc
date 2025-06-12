#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/packet.h"

using namespace ns3;

static void RxCallback(Ptr<const Packet> packet, const Address &from) {
    Time now = Simulator::Now();
    uint32_t rxSize = packet->GetSize();
    std::cout << now.GetSeconds() << "s: Received " << rxSize << " bytes from " << from << std::endl;
}

static void TxCallback(Ptr<Socket> socket, uint32_t pktSize) {
    Time now = Simulator::Now();
    std::cout << now.GetSeconds() << "s: Sent " << pktSize << " bytes from " << socket->GetNode()->GetObject<Node>()->GetId() << std::endl;
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;

    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), tid);
    InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(1), port);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&RxCallback));
    recvSocket->Listen();

    Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), tid);
    sendSocket->SetConnectByConfigure(true);
    InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);

    sendSocket->Connect(remote);

    Simulator::ScheduleWithContext(sendSocket->GetNode()->GetId(), Seconds(1.0), [sendSocket]() {
        uint32_t pktSize = 1024;
        Ptr<Packet> packet = Create<Packet>(pktSize);
        sendSocket->Send(packet);
        Simulator::ScheduleWithContext(sendSocket->GetNode()->GetId(), Seconds(1.0), [sendSocket, pktSize]() {
            Ptr<Packet> packet2 = Create<Packet>(pktSize);
            sendSocket->Send(packet2);
        });
    });

    sendSocket->SetSendCallback(MakeCallback(&TxCallback));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}