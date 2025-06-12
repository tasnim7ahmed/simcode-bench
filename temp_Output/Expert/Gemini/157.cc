#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    UdpClientHelper client(interfaces[0].GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(11.0));

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    UdpClientHelper client2(interfaces[1].GetAddress(1), 9);
    client2.SetAttribute("MaxPackets", UintegerValue(1000));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = client2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(11.0));

    UdpServerHelper server2(9);
    ApplicationContainer serverApps2 = server2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(11.0));

    UdpClientHelper client3(interfaces[2].GetAddress(1), 9);
    client3.SetAttribute("MaxPackets", UintegerValue(1000));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps3 = client3.Install(nodes.Get(2));
    clientApps3.Start(Seconds(1.0));
    clientApps3.Stop(Seconds(11.0));

    UdpServerHelper server3(9);
    ApplicationContainer serverApps3 = server3.Install(nodes.Get(3));
    serverApps3.Start(Seconds(1.0));
    serverApps3.Stop(Seconds(11.0));

    UdpClientHelper client4(interfaces[3].GetAddress(1), 9);
    client4.SetAttribute("MaxPackets", UintegerValue(1000));
    client4.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client4.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps4 = client4.Install(nodes.Get(3));
    clientApps4.Start(Seconds(1.0));
    clientApps4.Stop(Seconds(11.0));

    UdpServerHelper server4(9);
    ApplicationContainer serverApps4 = server4.Install(nodes.Get(0));
    serverApps4.Start(Seconds(1.0));
    serverApps4.Stop(Seconds(11.0));

    Simulator::Stop(Seconds(12.0));

    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("ring_topology.csv", std::ios::out);
    *stream->GetStream() << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

    for (uint32_t i = 0; i < 4; ++i) {
        Ptr<Application> app = serverApps.Get(0);
        if (i==1){app = serverApps2.Get(0);}
        if (i==2){app = serverApps3.Get(0);}
        if (i==3){app = serverApps4.Get(0);}

        Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(app);
        udpServer->TraceConnectWithoutContext("Rx", MakeBoundCallback(&
            [](Ptr<const Packet> packet, const Address& fromAddress, Ptr<OutputStreamWrapper> stream, uint32_t serverNodeId) {
                Ipv4Address senderAddress = InetSocketAddress::ConvertFrom(fromAddress).GetIpv4();
                uint32_t sourceNodeId = 0;
                for (uint32_t j = 0; j < 4; ++j) {
                    if (interfaces[j].GetAddress(0) == senderAddress) {
                        sourceNodeId = j;
                        break;
                    }
                }

                *stream->GetStream()
                    << sourceNodeId << ","
                    << serverNodeId << ","
                    << packet->GetSize() << ","
                    << packet->GetUid() / 1000000.0 << ","
                    << Simulator::Now().GetSeconds()
                    << std::endl;

            }, stream, i));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}