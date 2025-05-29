#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

std::ofstream outputFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time arrivalTime = Simulator::Now();
    Ipv4Address sourceAddress = Ipv4Address::ConvertFrom(address);

    uint32_t packetSize = packet->GetSize();
    Ptr<Packet> copy = packet->Copy();

    Ipv4Header ipHeader;
    copy->RemoveHeader(ipHeader);

    TcpHeader tcpHeader;
    copy->RemoveHeader(tcpHeader);
    uint16_t sourcePort = tcpHeader.GetSourcePort();
    uint16_t destinationPort = tcpHeader.GetDestinationPort();

    for (uint32_t i = 1; i <= 4; ++i) {
        if (destinationPort == 9 + i) {
            outputFile << i << ",";
            break;
        }
    }
    outputFile << "0" << ",";
    outputFile << packetSize << "," << Simulator::Now().GetSeconds() << "," << arrivalTime.GetSeconds() << std::endl;
}

void PacketTransmit(std::string context, Ptr<const Packet> packet) {
    Time transmitTime = Simulator::Now();

    uint32_t packetSize = packet->GetSize();
    Ptr<Packet> copy = packet->Copy();

    Ipv4Header ipHeader;
    copy->RemoveHeader(ipHeader);

    TcpHeader tcpHeader;
    copy->RemoveHeader(tcpHeader);
    uint16_t sourcePort = tcpHeader.GetSourcePort();
    uint16_t destinationPort = tcpHeader.GetDestinationPort();

    outputFile << "0" << ",";

    for (uint32_t i = 1; i <= 4; ++i) {
        if (destinationPort == 9 + i) {
            outputFile << i << ",";
            break;
        }
    }
    outputFile << packetSize << "," << transmitTime.GetSeconds() << ",0" << std::endl;
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    outputFile.open("star_network_data.csv");
    outputFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(0));
        linkNodes.Add(nodes.Get(i + 1));
        devices[i] = pointToPoint.Install(linkNodes);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        address.Assign(devices[i]);
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    for (uint32_t i = 1; i <= 4; ++i) {
        UdpEchoServerHelper echoServer(port + i);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces[i - 1].GetAddress(1), port + i);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    for (uint32_t i = 0; i < 5; ++i) {
        std::string context = "/NodeList/" + std::to_string(i) + "/$ns3::UdpL4Protocol/Tx";
        Config::Connect(context, MakeCallback(&PacketTransmit));

        std::string contextRx = "/NodeList/" + std::to_string(i) + "/$ns3::UdpL4Protocol/Rx";
        Config::Connect(contextRx, MakeCallback(&PacketReceive));
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    outputFile.close();

    return 0;
}