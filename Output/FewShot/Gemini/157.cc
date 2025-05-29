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

std::ofstream csvFile;

void PacketReceived(std::string context, Ptr<const Packet> packet, const Address& address) {
    Ipv4Address destinationIp;
    if (address.Is<InetSocketAddress>()) {
        destinationIp = InetSocketAddress::ConvertFrom(address).GetIpv4();
    }

    Time arrivalTime = Simulator::Now();
    uint32_t packetSize = packet->GetSize();

    std::string sourceNode = context.substr(13, 1);
    std::string destinationNode = std::to_string(destinationIp.Get()).substr(8, 1);

    csvFile << Simulator::Now().GetSeconds() << ","
            << sourceNode << ","
            << destinationNode << ","
            << packetSize << ","
            << arrivalTime.GetSeconds() << std::endl;
}


int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    csvFile.open("ring_topology_data.csv");
    csvFile << "Time,SourceNode,DestinationNode,PacketSize,ArrivalTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (int i = 0; i < 4; ++i) {
        NetDeviceContainer linkDevices = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get((i + 1) % 4)));
        devices.Add(linkDevices.Get(0));
        devices.Add(linkDevices.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    for (int i = 0; i < 4; ++i) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get((i + 1) % 4));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));
    }

    for (int i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(((i + 1) % 4) * 2), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Config::Connect("/NodeList/" + std::to_string(i+1) + "/ApplicationList/0/UdpEchoServer/Rx", MakeCallback(&PacketReceived));

    }

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    csvFile.close();

    Simulator::Destroy();

    return 0;
}