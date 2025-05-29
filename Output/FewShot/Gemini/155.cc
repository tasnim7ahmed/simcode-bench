#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

std::ofstream outputFile;

void PacketReceived(std::string context, Ptr<const Packet> packet, const Address& address) {
    Ipv4Address sourceAddress = Ipv4Address::GetZero();
    Ipv4Address destinationAddress = Ipv4Address::GetZero();
    uint32_t sourceNodeId = 0;
    uint32_t destinationNodeId = 0;

    if (InetSocketAddress::IsMatchingType(address)) {
        InetSocketAddress inetSocketAddress = InetSocketAddress::ConvertFrom(address);
        destinationAddress = inetSocketAddress.GetIpv4();

        for (uint32_t i = 0; i < Simulator::GetContext().GetNNodes(); ++i) {
            Ptr<Node> node = Simulator::GetContext().GetNode(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            if (ipv4 != nullptr) {
                for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
                    if (ipv4->GetAddress(j, 0).GetLocal() == destinationAddress) {
                        destinationNodeId = i;
                        break;
                    }
                }
            }
        }
    }

    std::string sourceContext = context.substr(0, context.find("/"));
    sourceNodeId = std::stoi(sourceContext.substr(sourceContext.find("NodeList/") + 9));

    Ptr<Node> sourceNode = Simulator::GetContext().GetNode(sourceNodeId);
    Ptr<Ipv4> sourceIpv4 = sourceNode->GetObject<Ipv4>();
    if(sourceIpv4 != nullptr) {
        sourceAddress = sourceIpv4->GetAddress(1, 0).GetLocal();
    }

    outputFile << Simulator::Now().GetSeconds() << ","
               << sourceNodeId << ","
               << destinationNodeId << ","
               << packet->GetSize() << ","
               << Simulator::Now().GetSeconds()
               << std::endl;
}

int main(int argc, char *argv[]) {
    outputFile.open("star_topology_data.csv");
    outputFile << "Time,SourceNode,DestinationNode,PacketSize,ReceptionTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (int i = 1; i < 5; ++i) {
        NodeContainer pair;
        pair.Add(nodes.Get(0));
        pair.Add(nodes.Get(i));
        NetDeviceContainer device = pointToPoint.Install(pair);
        devices.Add(device.Get(0));
        devices.Add(device.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    for (int i = 1; i < 5; ++i) {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(i), tid);
        InetSocketAddress iaddr(interfaces.GetAddress(2*i-1), port);
        recvSink->Bind(iaddr);
        recvSink->SetRecvCallback(MakeCallback(&PacketReceived));

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2*i-1), port));
        source.SetAttribute("MaxBytes", UintegerValue(1024000));
        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    outputFile.close();

    return 0;
}