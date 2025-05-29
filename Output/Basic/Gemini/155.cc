#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

using namespace ns3;

std::ofstream csvFile;

void PacketSinkRx(std::string context, Ptr<const Packet> p, const Address& addr) {
    static uint64_t lastRxTime = 0;
    uint64_t currentTime = Simulator::Now().GetNanoSeconds();
    double rxTime = Simulator::Now().GetSeconds();

    Ipv4Header ipHeader;
    p->PeekHeader(ipHeader);
    uint32_t sourceAddress = ipHeader.GetSource().Get();
    uint32_t destinationAddress = ipHeader.GetDestination().Get();
    uint32_t packetSize = p->GetSize();

    TypeId tid = TypeId::LookupByName ("ns3::TcpHeader");
    TcpHeader tcpHeader;
    if (p->PeekHeader (tcpHeader))
    {
      //std::cout << "Source Port: " << tcpHeader.GetSourcePort () << std::endl;
      //std::cout << "Destination Port: " << tcpHeader.GetDestinationPort () << std::endl;
    }

    std::stringstream ss;
    ss << sourceAddress;
    std::string sourceAddressStr = ss.str();

    std::stringstream ss2;
    ss2 << destinationAddress;
    std::string destinationAddressStr = ss2.str();

    csvFile << sourceAddressStr << "," << destinationAddressStr << "," << packetSize << "," << lastRxTime / 1e9 << "," << rxTime << std::endl;
    lastRxTime = currentTime;

}


int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    csvFile.open("star_topology_data.csv");
    csvFile << "Source,Destination,PacketSize,TxTime,RxTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 1; i < 5; ++i) {
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(0));
        linkNodes.Add(nodes.Get(i));
        devices[i - 1] = p2p.Install(linkNodes);
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        address.Assign(devices[i]);
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    for(int i = 1; i < 5; i++){
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(interfaces[i-1].GetAddress(1), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(i));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        Ptr<Node> appSource = nodes.Get(0);
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces[i-1].GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer sourceApps = source.Install(appSource);
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(10.0));

        Config::Connect("/NodeList/" + std::to_string(i) + "/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&PacketSinkRx));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    csvFile.close();
    Simulator::Destroy();

    return 0;
}