#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvUdpSimulation");

const uint32_t numNodes = 10;
const double simTime = 20.0;
const double pktInterval = 1.0;
const double nodeSpeedMin = 2.0;
const double nodeSpeedMax = 5.0;
const double pauseTime = 0.5;
const double areaX = 100.0;
const double areaY = 100.0;
const uint16_t port = 9000;
const uint32_t pktSize = 512;

Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();

void SendPacket(Ptr<Socket> socket, Ipv4InterfaceContainer interfaces, uint32_t nodeId, NodeContainer nodes)
{
    std::vector<uint32_t> neighbors;
    Ptr<MobilityModel> mob1 = nodes.Get(nodeId)->GetObject<MobilityModel>();
    Vector pos1 = mob1->GetPosition();
    double maxRange = 40.0;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        if (i == nodeId) continue;
        Ptr<MobilityModel> mob2 = nodes.Get(i)->GetObject<MobilityModel>();
        if ((mob2->GetPosition() - pos1).GetLength() <= maxRange)
            neighbors.push_back(i);
    }
    if (!neighbors.empty())
    {
        uint32_t idx = neighbors[uv->GetInteger(0, neighbors.size() - 1)];
        InetSocketAddress dest = InetSocketAddress(interfaces.GetAddress(idx), port);
        Ptr<Packet> pkt = Create<Packet>(pktSize);
        socket->SendTo(pkt, 0, dest);
    }
    Simulator::Schedule(Seconds(pktInterval), &SendPacket, socket, interfaces, nodeId, nodes);
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(10.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(5),
        "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(i), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        recvSink->Bind(local);
        recvSink->SetRecvCallback(MakeCallback([](Ptr<Socket> socket) {
            while (Ptr<Packet> packet = socket->Recv())
            {
            }
        }));

        Ptr<Socket> source = Socket::CreateSocket(nodes.Get(i), tid);
        Simulator::Schedule(Seconds(uv->GetValue(0,pktInterval)), &SendPacket, source, interfaces, i, nodes);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}