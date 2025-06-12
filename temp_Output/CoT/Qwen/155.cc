#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

std::ofstream g_csvFile;

void
LogPacketTransmission(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    double txTime = Simulator::Now().GetSeconds();
    Ipv4Address srcAddr = socket->GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    Ipv4Address dstAddr = header.GetDestinationAddress();

    uint32_t srcNodeId = socket->GetNode()->GetId();
    uint32_t dstNodeId = 0;
    for (uint32_t i = 1; i <= 4; ++i)
    {
        if (srcAddr == Names::Find<Node>("Node" + std::to_string(i))->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal())
        {
            dstNodeId = i;
            break;
        }
    }

    g_csvFile << srcNodeId << ","
              << dstNodeId << ","
              << packet->GetSize() << ","
              << txTime << ","
              << "" // Reception time will be logged separately
              << std::endl;
}

void
LogPacketReception(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
    double rxTime = Simulator::Now().GetSeconds();
    Ipv4Address dstAddr = socket->GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    Ipv4Address srcAddr = header.GetSourceAddress();

    uint32_t dstNodeId = socket->GetNode()->GetId();
    uint32_t srcNodeId = 0;
    for (uint32_t i = 1; i <= 4; ++i)
    {
        if (dstAddr == Names::Find<Node>("Node" + std::to_string(i))->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal())
        {
            srcNodeId = i;
            break;
        }
    }

    // Update the CSV entry with reception time (this approach appends a new line for simplicity)
    g_csvFile << srcNodeId << ","
              << dstNodeId << ","
              << packet->GetSize() << ","
              << "" << "," // Transmission time already logged
              << rxTime << std::endl;
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    g_csvFile.open("star_topology_tcp_dataset.csv");
    g_csvFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

    NodeContainer centralNode;
    centralNode.Create(1);
    Names::Add("Node0", centralNode.Get(0));

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        Names::Add("Node" + std::to_string(i + 1), peripheralNodes.Get(i));
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer centralDevices;
    NetDeviceContainer peripheralDevices[4];

    for (uint32_t i = 0; i < 4; ++i)
    {
        NetDeviceContainer devices = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
        centralDevices.Add(devices.Get(0));
        peripheralDevices[i].Add(devices.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer centralInterfaces;
    Ipv4InterfaceContainer peripheralInterfaces[4];

    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(centralDevices.Get(i), peripheralDevices[i].Get(0)));
        centralInterfaces.Add(interfaces.Get(0));
        peripheralInterfaces[i].Add(interfaces.Get(1));
    }

    uint16_t port = 5000;

    for (uint32_t i = 0; i < 4; ++i)
    {
        Address sinkAddress(InetSocketAddress(peripheralInterfaces[i].GetAddress(0), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = packetSinkHelper.Install(peripheralNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        onoff.SetAttribute("Remote", AddressValue(sinkAddress));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = onoff.Install(centralNode.Get(0));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(9.0));
    }

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                    MakeCallback([](Ptr<const Packet> packet, const Address&) {
                        // We ignore this because we want to track TCP segments at the socket level
                    }),
                    nullptr);

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/$ns3::TcpSocketBase/Tx",
                    MakeBoundCallback(&LogPacketTransmission),
                    nullptr);

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/$ns3::TcpSocketBase/Rx",
                    MakeBoundCallback(&LogPacketReception),
                    nullptr);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();

    return 0;
}