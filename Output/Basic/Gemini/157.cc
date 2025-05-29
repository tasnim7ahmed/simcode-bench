#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>

using namespace ns3;

std::ofstream csvFile;

void PacketReceive(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time arrivalTime = Simulator::Now();
    Ipv4Address sourceAddress;
    Ipv4Address destinationAddress;
    uint16_t sourcePort;
    uint16_t destinationPort;

    Ptr<Socket> socket = DynamicCast<UdpSocketFactory>(SocketFactory::GetFactory())->CreateSocket();
    InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(address);

    destinationAddress = iaddr.GetIpv4();
    destinationPort = iaddr.GetPort();

    Ptr<Packet> copy = packet->Copy();
    UdpHeader udpHeader;
    copy->RemoveHeader(udpHeader);

    Ptr<Ipv4> ipv4 = DynamicCast<Ipv4>(socket->GetNode()->GetObject<Ipv4L3Protocol>()->GetObject<Ipv4>());
    sourceAddress = ipv4->GetAddress(1,0).GetLocal();

    uint32_t packetSize = packet->GetSize();
    Time transmissionTime = Time(packet->GetUid() * 0.000001);

    csvFile << sourceAddress << ","
            << destinationAddress << ","
            << packetSize << ","
            << transmissionTime.GetSeconds() << ","
            << arrivalTime.GetSeconds() << std::endl;
}


int main(int argc, char* argv[]) {

    bool enablePcap = false;
    std::string animFile = "ring.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("animFile", "File Name for Animation Output", animFile);

    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices30 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces30 = address.Assign(devices30);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpServerHelper server0(port);
    server0.SetAttribute("MaxPackets", UintegerValue(1000000));
    ApplicationContainer apps0 = server0.Install(nodes.Get(0));
    apps0.Start(Seconds(1.0));
    apps0.Stop(Seconds(10.0));

    UdpServerHelper server1(port);
    server1.SetAttribute("MaxPackets", UintegerValue(1000000));
    ApplicationContainer apps1 = server1.Install(nodes.Get(1));
    apps1.Start(Seconds(1.0));
    apps1.Stop(Seconds(10.0));

    UdpServerHelper server2(port);
    server2.SetAttribute("MaxPackets", UintegerValue(1000000));
    ApplicationContainer apps2 = server2.Install(nodes.Get(2));
    apps2.Start(Seconds(1.0));
    apps2.Stop(Seconds(10.0));

    UdpServerHelper server3(port);
    server3.SetAttribute("MaxPackets", UintegerValue(1000000));
    ApplicationContainer apps3 = server3.Install(nodes.Get(3));
    apps3.Start(Seconds(1.0));
    apps3.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(10);

    UdpClientHelper client0(interfaces12.GetAddress(1), port);
    client0.SetAttribute("MaxPackets", UintegerValue(1000000));
    client0.SetAttribute("PacketSize", UintegerValue(packetSize));
    client0.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
    clientApps0.Start(Seconds(2.0));
    clientApps0.Stop(Seconds(9.0));

    UdpClientHelper client1(interfaces23.GetAddress(1), port);
    client1.SetAttribute("MaxPackets", UintegerValue(1000000));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));
    client1.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer clientApps1 = client1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(9.0));

    UdpClientHelper client2(interfaces30.GetAddress(1), port);
    client2.SetAttribute("MaxPackets", UintegerValue(1000000));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));
    client2.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer clientApps2 = client2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(9.0));

    UdpClientHelper client3(interfaces01.GetAddress(1), port);
    client3.SetAttribute("MaxPackets", UintegerValue(1000000));
    client3.SetAttribute("PacketSize", UintegerValue(packetSize));
    client3.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer clientApps3 = client3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(9.0));

    std::string filename = "ring_data.csv";
    csvFile.open(filename);
    csvFile << "Source,Destination,PacketSize,TransmissionTime,ReceiveTime" << std::endl;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Application> app = node->GetApplication(0);
        if(app){
            Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(app);
            if(udpServer){
                Ptr<Socket> socket = udpServer->GetSocket();
                if(socket){
                    std::ostringstream oss;
                    oss << "/NodeList/" << i << "/$ns3::UdpSocketFactory/Rx";
                    Config::Connect(oss.str(), MakeCallback(&PacketReceive));
                }
            }
        }
    }
    if (enablePcap) {
        pointToPoint.EnablePcapAll("ring");
    }

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 30, 10);
    anim.SetConstantPosition(nodes.Get(2), 50, 10);
    anim.SetConstantPosition(nodes.Get(3), 70, 10);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    csvFile.close();
    Simulator::Destroy();

    return 0;
}