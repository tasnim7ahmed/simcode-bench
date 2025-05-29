#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

std::ofstream outputFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time arrivalTime = Simulator::Now();
    Ipv4Header ipHeader;
    packet->PeekHeader(ipHeader);
    uint32_t sourceNode = ipHeader.GetSource().Get();
    uint32_t destinationNode = ipHeader.GetDestination().Get();
    uint32_t packetSize = packet->GetSize();

    outputFile << sourceNode << ","
               << destinationNode << ","
               << packetSize << ","
               << packet->GetUid() << ","
               << Simulator::Now().GetSeconds() << ","
               << arrivalTime.GetSeconds() << std::endl;

}

int main(int argc, char* argv[]) {

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    outputFile.open("star_topology_data.csv");
    outputFile << "Source,Destination,PacketSize,UID,SendTime,ReceiveTime" << std::endl;

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

    UdpEchoServerHelper echoServer2(9);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(5), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(0));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer3(9);
    ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(3));
    serverApps3.Start(Seconds(1.0));
    serverApps3.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient3(interfaces.GetAddress(7), 9);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient3.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(0));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer4(9);
    ApplicationContainer serverApps4 = echoServer4.Install(nodes.Get(4));
    serverApps4.Start(Seconds(1.0));
    serverApps4.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient4(interfaces.GetAddress(9), 9);
    echoClient4.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient4.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient4.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps4 = echoClient4.Install(nodes.Get(0));
    clientApps4.Start(Seconds(2.0));
    clientApps4.Stop(Seconds(10.0));


    for (uint32_t i = 1; i < 10; i+=2)
    {
         std::stringstream ss;
         ss << "/NodeList/" << i << "/$ns3::UdpSocketAggregator/SocketList/0/Rx";
         Config::Connect(ss.str(), MakeCallback(&PacketReceive));
    }


    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    outputFile.close();

    return 0;
}