#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

std::ofstream outputFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time arrivalTime = Simulator::Now();
    Ipv4Address sourceAddress = Ipv4Address::GetZero();
    Ipv4Address destinationAddress = Ipv4Address::GetZero();
    uint16_t sourcePort = 0;
    uint16_t destinationPort = 0;

    Ptr<Ipv4Header> ipv4Header = Create<Ipv4Header>();
    if (packet->PeekHeader(ipv4Header)) {
        sourceAddress = ipv4Header->GetSource();
        destinationAddress = ipv4Header->GetDestination();

        Ptr<TcpHeader> tcpHeader = Create<TcpHeader>();
        if (packet->PeekHeader(tcpHeader)) {
            sourcePort = tcpHeader->GetSourcePort();
            destinationPort = tcpHeader->GetDestinationPort();
        }

        outputFile << sourceAddress << "," << destinationAddress << "," << packet->GetSize() << "," << Simulator::Now().GetSeconds() << "," << arrivalTime.GetSeconds() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    outputFile.open("star_network_data.csv");
    outputFile << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices3 = p2p.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer devices4 = p2p.Install(nodes.Get(0), nodes.Get(4));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(interfaces1.GetAddress(1), port));
    source1.SetAttribute("SendSize", UintegerValue(1024));
    source1.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps1 = source1.Install(nodes.Get(0));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(10.0));

    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(interfaces2.GetAddress(1), port));
    source2.SetAttribute("SendSize", UintegerValue(1024));
    source2.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps2 = source2.Install(nodes.Get(0));
    sourceApps2.Start(Seconds(1.0));
    sourceApps2.Stop(Seconds(10.0));

    BulkSendHelper source3("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(1), port));
    source3.SetAttribute("SendSize", UintegerValue(1024));
    source3.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps3 = source3.Install(nodes.Get(0));
    sourceApps3.Start(Seconds(1.0));
    sourceApps3.Stop(Seconds(10.0));

    BulkSendHelper source4("ns3::TcpSocketFactory", InetSocketAddress(interfaces4.GetAddress(1), port));
    source4.SetAttribute("SendSize", UintegerValue(1024));
    source4.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps4 = source4.Install(nodes.Get(0));
    sourceApps4.Start(Seconds(1.0));
    sourceApps4.Stop(Seconds(10.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps1 = sink1.Install(nodes.Get(1));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(11.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(2));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(11.0));

    PacketSinkHelper sink3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps3 = sink3.Install(nodes.Get(3));
    sinkApps3.Start(Seconds(0.0));
    sinkApps3.Stop(Seconds(11.0));

    PacketSinkHelper sink4("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps4 = sink4.Install(nodes.Get(4));
    sinkApps4.Start(Seconds(0.0));
    sinkApps4.Stop(Seconds(11.0));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketReceive));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    outputFile.close();
    Simulator::Destroy();
    return 0;
}