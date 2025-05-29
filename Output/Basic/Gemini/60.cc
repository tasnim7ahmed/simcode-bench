#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/queue.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

uint32_t totalBytesSent = 0;
uint32_t bytesToSend = 2000000;
Ptr<Socket> clientSocket;

void CongestionWindowTracer(std::string key, uint32_t oldValue, uint32_t newValue) {
    NS_LOG_UNCOND("Congestion Window: " << Simulator::Now().AsDouble() << " " << newValue);
}

void QueueSizeTracer(std::string context, uint32_t oldValue, uint32_t newValue) {
    NS_LOG_UNCOND("Queue Size: " << Simulator::Now().AsDouble() << " " << newValue);
}

void PacketReceived(Ptr<const Packet> packet, const Address& address) {
    NS_LOG_UNCOND("Packet Received: " << Simulator::Now().AsDouble() << " " << packet->GetSize());
}

void SendData() {
    uint32_t remainingBytes = bytesToSend - totalBytesSent;
    uint32_t sendSize = std::min(uint32_t(1448), remainingBytes);

    Ptr<Packet> packet = Create<Packet>(sendSize);
    clientSocket->Send(packet);
    totalBytesSent += sendSize;

    if (totalBytesSent < bytesToSend) {
        Simulator::Schedule(Seconds(0.001), &SendData);
    } else {
        clientSocket->Close();
    }
}

void ConnectionEstablished(Ptr<Socket> socket) {
    NS_LOG_UNCOND("Connection Established!");
    Simulator::Schedule(Seconds(1.0), &SendData);
}

void SocketError(Ptr<Socket> socket) {
    NS_LOG_UNCOND("Socket Error!");
}

void SocketCleanup(Ptr<Socket> socket) {
    NS_LOG_UNCOND("Socket Cleanup!");
}

int main(int argc, char* argv[]) {
    LogComponent::Enable("TcpLargeTransfer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    clientSocket->Bind();
    clientSocket->Connect(sinkAddress);

    clientSocket->SetConnectCallback(&ConnectionEstablished);
    clientSocket->SetCloseCallback(&SocketCleanup);
    clientSocket->SetErrorCallback(&SocketError);

    Config::ConnectWithoutContext("/ChannelList/0/Queue/Size", MakeCallback(&QueueSizeTracer));
    Config::ConnectWithoutContext("/ChannelList/1/Queue/Size", MakeCallback(&QueueSizeTracer));

    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CongestionWindowTracer));

    Simulator::Schedule(Seconds(0.1), [&]() {
        Config::ConnectWithoutContext("/NodeList/2/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&PacketReceived));
    });

    pointToPoint.EnablePcapAll("tcp-large-transfer");

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-large-transfer.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}