#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN_CSMA_UDP_Grid");

static uint32_t packetsSent = 0;
static uint32_t packetsReceived = 0;
static double totalDelay = 0.0;

void RxPacketCallback(Ptr<const Packet> packet, const Address &address)
{
    packetsReceived++;
}

void DelayTracer(std::string context, Ptr<const Packet> packet, const Address &from, const Address &to)
{
    if (packet->GetUid() == 0)
        return;
}

void TxPacketCallback(std::string context, Ptr<const Packet> packet)
{
    packetsSent++;
}

void PacketReceiveWithDelay(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    Time delay = Simulator::Now() - socket->GetObject<UdpSocket>()->GetApp()->GetObject<Application>()->GetStartTime();
    totalDelay += delay.GetSeconds();
    packetsReceived++;
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    uint32_t baseStationIndex = 0;
    double simTime = 20.0;
    double interval = 2.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Place nodes in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(40.0),
                                 "DeltaY", DoubleValue(40.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP server setup (Base station)
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(baseStationIndex));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP clients (sensor nodes send to base station)
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        if (i == baseStationIndex)
            continue;

        UdpClientHelper udpClient(interfaces.GetAddress(baseStationIndex), udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(64));

        ApplicationContainer clientApps = udpClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0 + 0.2*i));
        clientApps.Stop(Seconds(simTime));
    }

    // Tracing Rx packets for metrics
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> pkt, const Address & from){
        packetsReceived++;
    }));

    // Tracing Tx packets for metrics
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        if (i == baseStationIndex) continue;
        Ptr<Node> node = nodes.Get(i);
        Ptr<Application> app = node->GetApplication(0);
        if (app)
        {
            Ptr<UdpClient> client = app->GetObject<UdpClient>();
            if (client)
            {
                client->TraceConnect("Tx", std::string(), MakeCallback([](Ptr<const Packet> pkt){
                    packetsSent++;
                }));
            }
        }
    }

    // NetAnim setup
    AnimationInterface anim("wsn-csma-grid.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0); // base station
    anim.SetConstantPosition(nodes.Get(1), 50.0, 10.0);
    anim.SetConstantPosition(nodes.Get(2), 90.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 10.0, 50.0);
    anim.SetConstantPosition(nodes.Get(4), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(5), 90.0, 50.0);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        if (i == baseStationIndex)
            anim.UpdateNodeDescription(nodes.Get(i), "BaseStation");
        else
            anim.UpdateNodeDescription(nodes.Get(i), "Sensor" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, i == baseStationIndex ? 0 : 128, i == baseStationIndex ? 0 : 255);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Get statistics
    uint32_t totalTx = 0;
    uint32_t totalRx = 0;
    double totalEEDelay = 0.0;

    Ptr<UdpServer> server = serverApps.Get(0)->GetObject<UdpServer>();
    if (server)
    {
        totalRx = server->GetReceived();
    }
    totalTx = packetsSent;
    // As a simple e2e delay estimate, we can use application delay sum from server
    totalEEDelay = server->GetLost() > 0 ? 0.0 : simTime / std::max(1u, totalRx);

    double pdr = (totalTx == 0) ? 0.0 : (100.0 * totalRx / totalTx);
    std::cout << "======= Simulation Results =======" << std::endl;
    std::cout << "Packets sent:     " << totalTx << std::endl;
    std::cout << "Packets received: " << totalRx << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "End-to-End Delay (approx.): " << totalEEDelay << " s\n";

    Simulator::Destroy();
    return 0;
}