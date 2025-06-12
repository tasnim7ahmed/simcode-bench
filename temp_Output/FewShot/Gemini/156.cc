#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream outputFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time now = Simulator::Now();
    Ipv4Address sourceAddress;
    Ipv4Address destinationAddress;
    uint16_t sourcePort, destinationPort;

    Ptr<Packet> copy = packet->Copy();
    TcpHeader tcpHeader;
    copy->PeekHeader(tcpHeader);
    sourcePort = tcpHeader.GetSourcePort();
    destinationPort = tcpHeader.GetDestinationPort();

    Ptr<Ipv4> ipv4 = copy->FindFirstMatchingProtocol<Ipv4>();
    if (ipv4 != nullptr) {
        Ipv4Header ipHeader;
        copy->PeekHeader(ipHeader);
        sourceAddress = ipHeader.GetSource();
        destinationAddress = ipHeader.GetDestination();
    }

    outputFile << sourceAddress << ","
               << destinationAddress << ","
               << packet->GetSize() << ","
               << now.GetSeconds() << std::endl;
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    outputFile.open("star_topology_data.csv");
    outputFile << "Source Address,Destination Address,Packet Size,Reception Time (s)" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (int i = 1; i < 5; ++i) {
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
    for (int i = 1; i < 5; ++i) {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2*i-1), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2*i-1), port));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(102400));
        ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(10.0));

        std::string context = "/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketList/0/Rx";
        Config::Connect(context, MakeCallback(&PacketReceive));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    outputFile.close();
    Simulator::Destroy();

    return 0;
}