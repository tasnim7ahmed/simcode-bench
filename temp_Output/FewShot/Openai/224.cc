#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvSimulation");

class PeriodicUdpSender : public Application
{
public:
    PeriodicUdpSender() {}
    virtual ~PeriodicUdpSender() {}
    void Setup(Address address, uint16_t port, Ptr<UniformRandomVariable> rng)
    {
        m_peerAddress = address;
        m_peerPort = port;
        m_rng = rng;
    }

protected:
    virtual void StartApplication() override
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        ScheduleNextTx();
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
        Simulator::Cancel(m_sendEvent);
    }

private:
    void ScheduleNextTx()
    {
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &PeriodicUdpSender::SendPacket, this);
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(512);
        m_socket->SendTo(packet, 0, InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
        ScheduleNextTx();
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    Ptr<UniformRandomVariable> m_rng;
    EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
    uint32_t nNodes = 10;
    double simTime = 20.0;
    double nodeSpeedMin = 2.0;
    double nodeSpeedMax = 10.0;
    double pauseTime = 0.5;
    double areaX = 100.0;
    double areaY = 100.0;
    uint16_t port = 4000;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of MANET nodes", nNodes);
    cmd.Parse(argc, argv);

    LogComponentEnable("ManetAodvSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(nNodes);

    // Configure Wi-Fi (ad-hoc mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(nodeSpeedMin) + "|Max=" + std::to_string(nodeSpeedMax) + "]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(pauseTime) + "]"),
                             "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[ \
                                 X=ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaX) + "], \
                                 Y=ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaY) + "]]"));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server application on every node
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(i));
        serverApp.Start(Seconds(0.5));
        serverApp.Stop(Seconds(simTime));
        serverApps.Add(serverApp);
    }

    // UDP sender: each node sends to a randomly chosen neighbor every second
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        std::vector<uint32_t> neighIdx;
        for (uint32_t j = 0; j < nNodes; ++j)
        {
            if (i != j)
                neighIdx.push_back(j);
        }
        uint32_t randomIdx = neighIdx.size() > 0 ? neighIdx[rng->GetInteger(0, neighIdx.size() - 1)] : i;
        Ipv4Address dstAddr = interfaces.GetAddress(randomIdx);

        Ptr<PeriodicUdpSender> app = CreateObject<PeriodicUdpSender>();
        app->Setup(InetSocketAddress(dstAddr, port), port, rng);
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}