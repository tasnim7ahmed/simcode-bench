#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream outputFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time receiveTime = Simulator::Now();
    Ipv4Address sourceAddress = Ipv4Address::GetZero();
    Ipv4Address destinationAddress = Ipv4Address::GetZero();
    uint16_t sourcePort = 0;
    uint16_t destinationPort = 0;

    if (packet->GetSize() > 0) {
        Ptr<Packet> copy = packet->Copy();
        Ipv4Header ipHeader;
        copy->RemoveHeader(ipHeader);

        sourceAddress = ipHeader.GetSource();
        destinationAddress = ipHeader.GetDestination();

        TcpHeader tcpHeader;
        copy->RemoveHeader(tcpHeader);

        sourcePort = tcpHeader.GetSourcePort();
        destinationPort = tcpHeader.GetDestinationPort();
    }

    outputFile << sourceAddress << "," << destinationAddress << "," << packet->GetSize() << "," << packet->GetUid() << "," << receiveTime.GetSeconds() << std::endl;
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    outputFile.open("star_network_data.csv");
    outputFile << "Source,Destination,PacketSize,PacketUID,ReceiveTime(s)" << std::endl;

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
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Packet::EnablePrinting();

    for (int i = 0; i < devices.GetN(); ++i) {
        devices.Get(i)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&PacketReceive));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    outputFile.close();

    Simulator::Destroy();
    return 0;
}