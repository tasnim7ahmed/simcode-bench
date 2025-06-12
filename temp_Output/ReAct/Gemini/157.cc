#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet.h"
#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream outputFile;

void ReceivePacket(std::string context, Ptr<const Packet> packet, const Address& address) {
    Time arrivalTime = Simulator::Now();
    Ipv4Address sourceAddress = Ipv4Address::GetZero();
    Ipv4Address destinationAddress = Ipv4Address::GetZero();
    uint16_t packetSize = packet->GetSize();

    Ptr<NetDevice> receivingDevice = NetDevice::FindByAddress(address);
    if(receivingDevice) {
        Ptr<Node> receivingNode = receivingDevice->GetNode();

        Ptr<Ipv4> ipv4 = receivingNode->GetObject<Ipv4>();
        if(ipv4) {
            for (uint32_t i = 0; i < ipv4->GetNAddresses(); ++i) {
                Ipv4InterfaceAddress interfaceAddress = ipv4->GetAddress(i, 0);
                if(interfaceAddress.GetLocal() == Ipv4Address("127.0.0.1")) continue;
                 destinationAddress = interfaceAddress.GetLocal();
                 break;
            }
        }

    }

    Ptr<Packet> copyPacket = packet->Copy();
    if(copyPacket->PeekHeader<UdpHeader>()) {
        UdpHeader udpHeader;
        copyPacket->PeekHeader(udpHeader);
        uint16_t destPort = udpHeader.GetDestPort();
        ApplicationContainer apps = ApplicationContainer::GetApplication(destPort-9);

        if(!apps.GetN() ) return;
        Ptr<UdpClient> client = DynamicCast<UdpClient>(apps.Get(0));
        if(client){
             sourceAddress = client->GetRemoteAddress();
        }

        Time sentTime = client->GetSentTime(packet->GetUid());

        outputFile << sourceAddress << ","
                   << destinationAddress << ","
                   << packetSize << ","
                   << sentTime.GetSeconds() << ","
                   << arrivalTime.GetSeconds() << std::endl;
    }
}


int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    outputFile.open("ring_network_data.csv");
    outputFile << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices3 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices4 = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

    UdpClientHelper client1(interfaces1.GetAddress(1), 10);
    client1.SetAttribute("MaxPackets", UintegerValue(100));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = client1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));
    clientApps1.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&UdpClient::LogSentTime, DynamicCast<UdpClient>(clientApps1.Get(0))));

    UdpClientHelper client2(interfaces2.GetAddress(1), 11);
    client2.SetAttribute("MaxPackets", UintegerValue(100));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));
    clientApps2.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&UdpClient::LogSentTime, DynamicCast<UdpClient>(clientApps2.Get(0))));

    UdpClientHelper client3(interfaces3.GetAddress(1), 12);
    client3.SetAttribute("MaxPackets", UintegerValue(100));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = client3.Install(nodes.Get(2));
    clientApps3.Start(Seconds(1.0));
    clientApps3.Stop(Seconds(10.0));
    clientApps3.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&UdpClient::LogSentTime, DynamicCast<UdpClient>(clientApps3.Get(0))));

    UdpClientHelper client4(interfaces4.GetAddress(1), 13);
    client4.SetAttribute("MaxPackets", UintegerValue(100));
    client4.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client4.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps4 = client4.Install(nodes.Get(3));
    clientApps4.Start(Seconds(1.0));
    clientApps4.Stop(Seconds(10.0));
    clientApps4.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&UdpClient::LogSentTime, DynamicCast<UdpClient>(clientApps4.Get(0))));

    UdpServerHelper server1(10);
    ApplicationContainer serverApps1 = server1.Install(nodes.Get(1));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(11.0));

    UdpServerHelper server2(11);
    ApplicationContainer serverApps2 = server2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(11.0));

    UdpServerHelper server3(12);
    ApplicationContainer serverApps3 = server3.Install(nodes.Get(3));
    serverApps3.Start(Seconds(0.0));
    serverApps3.Stop(Seconds(11.0));

    UdpServerHelper server4(13);
    ApplicationContainer serverApps4 = server4.Install(nodes.Get(0));
    serverApps4.Start(Seconds(0.0));
    serverApps4.Stop(Seconds(11.0));

    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&ReceivePacket));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    outputFile.close();

    Simulator::Destroy();
    return 0;
}