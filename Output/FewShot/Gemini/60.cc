#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/address.h"
#include "ns3/trace-helper.h"
#include "ns3/queue.h"

using namespace ns3;

uint32_t bytesTotal = 2000000;
uint32_t bytesSent = 0;

static void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    std::cout << Simulator::Now().Seconds() << " Cwnd " << newCwnd << std::endl;
}

static void QueueSizeTracer(Ptr<Queue<Packet>> queue) {
    std::cout << Simulator::Now().Seconds() << " Queue Size " << queue->GetNPackets() << std::endl;
}

static void RxDrop(Ptr<const Packet> p) {
    std::cout << Simulator::Now().Seconds() << " RxDrop " << p->GetUid() << std::endl;
}

static void EnqueueTrace(std::string context, Ptr<const Queue<Packet>> queue, Ptr<const Packet> packet) {
    std::cout << Simulator::Now().Seconds() << " Enqueue " << packet->GetUid() << " queue size " << queue->GetNPackets() << std::endl;
}

static void DequeueTrace(std::string context, Ptr<const Queue<Packet>> queue, Ptr<const Packet> packet) {
    std::cout << Simulator::Now().Seconds() << " Dequeue " << packet->GetUid() << " queue size " << queue->GetNPackets() << std::endl;
}


static void SendData(Ptr<Socket> socket, Ipv4Address destination, uint16_t port) {
    if (bytesSent < bytesTotal) {
        uint32_t toSend = std::min((uint32_t)1448, bytesTotal - bytesSent);
        Ptr<Packet> packet = Create<Packet>(toSend);
        socket->Send(packet);
        bytesSent += toSend;
        Simulator::Schedule(MilliSeconds(1), &SendData, socket, destination, port);
    } else {
        socket->Close();
    }
}

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ipv4Address serverAddress = interfaces2.GetAddress(1);

    Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    socket->Bind();
    socket->Connect(InetSocketAddress(serverAddress, sinkPort));

    Config::ConnectWithoutContext("/ChannelList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeBoundCallback(&EnqueueTrace, devices1.Get(0)->GetInstanceId()));
    Config::ConnectWithoutContext("/ChannelList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeBoundCallback(&DequeueTrace, devices1.Get(0)->GetInstanceId()));
    Config::ConnectWithoutContext("/ChannelList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeBoundCallback(&EnqueueTrace, devices2.Get(0)->GetInstanceId()));
    Config::ConnectWithoutContext("/ChannelList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeBoundCallback(&DequeueTrace, devices2.Get(0)->GetInstanceId()));

    Config::ConnectWithoutContext("/NodeList/2/$ns3::TcpL4Protocol/RxDrop", MakeCallback(&RxDrop));

    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndTracer));


    Simulator::ScheduleWithContext(socket->GetNode()->GetId(), Seconds(2.0), &SendData, socket, serverAddress, sinkPort);

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-large-transfer.tr"));
    pointToPoint.EnablePcapAll("tcp-large-transfer");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}