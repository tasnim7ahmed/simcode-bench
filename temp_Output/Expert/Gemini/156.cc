#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

struct PacketData {
    uint32_t sourceNode;
    uint32_t destinationNode;
    uint32_t packetSize;
    Time transmissionTime;
    Time receptionTime;
};

std::vector<PacketData> packetData;

void PacketSink(Ptr<Socket> socket) {
    Address localAddress = socket->GetLocalAddress();
    uint16_t port = InetSocketAddress::ConvertFrom(localAddress).GetPort();

    socket->SetRecvCallback(MakeCallback([](Ptr<Socket> socket, Ptr<Packet> packet, const Address &from) {
        Address localAddress = socket->GetLocalAddress();
        uint16_t port = InetSocketAddress::ConvertFrom(localAddress).GetPort();
        Ipv4Address sourceAddress = InetSocketAddress::ConvertFrom(from).GetIpv4();
        uint32_t sourceNodeId = sourceAddress.Get() - Ipv4Address("10.1.1.0").Get();

        Time receptionTime = Simulator::Now();
        uint32_t packetSize = packet->GetSize();

        Ptr<Packet> copy = packet->Copy();
        Ipv4Header ipHeader;
        copy->RemoveHeader(ipHeader);

        TcpHeader tcpHeader;
        copy->RemoveHeader(tcpHeader);

        Ptr<Node> node = socket->GetNode();
        uint32_t destinationNodeId = node->GetId();

        packetData.push_back({sourceNodeId, destinationNodeId, packetSize, Time(0), receptionTime});

        return true;
    }));
}

void PacketTransmitted(Ptr<const Packet> packet, const Address &address) {
    Ipv4Address destinationAddress = InetSocketAddress::ConvertFrom(address).GetIpv4();
    uint32_t destinationNodeId = destinationAddress.Get() - Ipv4Address("10.1.1.0").Get();
    uint32_t packetSize = packet->GetSize();

    Ptr<Node> node = packet->GetNode();
    uint32_t sourceNodeId = node->GetId();

    for (auto &data : packetData) {
        if (data.sourceNode == sourceNodeId && data.destinationNode == destinationNodeId && data.packetSize == packetSize && data.receptionTime != Time(0)) {
            data.transmissionTime = Simulator::Now();
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        NetDeviceContainer link = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
        devices.Add(link.Get(0));
        devices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    ApplicationContainer sinkApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(i), TcpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(2 * i, 0), port);
        sinkSocket->Bind(local);
        sinkSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                     MakeCallback(&PacketSink));
        sinkSocket->Listen();
    }

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
        clientSocket->TraceConnectWithoutContext("Tx", MakeCallback(&PacketTransmitted));
        Ptr<MyApp> app = CreateObject<MyApp>();
        app->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
        app->SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(interfaces.GetAddress(2 * i, 0), port)));
        app->SetAttribute("RemotePort", UintegerValue(port));
        app->SetAttribute("PacketSize", UintegerValue(1024));
        app->SetAttribute("MaxBytes", UintegerValue(1000000));
        clientApps.Add(app);
        nodes.Get(0)->AddApplication(app);
    }

    clientApps.Start(Seconds(1.0));
    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    std::ofstream outputFile("simulation_data.csv");
    outputFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time\n";
    for (const auto &data : packetData) {
        outputFile << data.sourceNode << "," << data.destinationNode << "," << data.packetSize << ","
                   << data.transmissionTime.GetSeconds() << "," << data.receptionTime.GetSeconds() << "\n";
    }
    outputFile.close();

    Simulator::Destroy();
    return 0;
}