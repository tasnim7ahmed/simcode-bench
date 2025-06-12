#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer sourceNode;
    sourceNode.Create(1);

    NodeContainer routers;
    routers.Create(2);

    NodeContainer destinationRouter;
    destinationRouter.Create(1);

    NodeContainer destinationNode;
    destinationNode.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer sourceToRtr1;
    sourceToRtr1 = p2p.Install(sourceNode.Get(0), routers.Get(0));

    NetDeviceContainer sourceToRtr2;
    sourceToRtr2 = p2p.Install(sourceNode.Get(0), routers.Get(1));

    NetDeviceContainer rtr1ToDestRtr;
    rtr1ToDestRtr = p2p.Install(routers.Get(0), destinationRouter.Get(0));

    NetDeviceContainer rtr2ToDestRtr;
    rtr2ToDestRtr = p2p.Install(routers.Get(1), destinationRouter.Get(0));

    NetDeviceContainer destRtrToDestNode;
    destRtrToDestNode = p2p.Install(destinationRouter.Get(0), destinationNode.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr1Ifs = address.Assign(sourceToRtr1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr2Ifs = address.Assign(sourceToRtr2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr1DestRtrIfs = address.Assign(rtr1ToDestRtr);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr2DestRtrIfs = address.Assign(rtr2ToDestRtr);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer destRtrDestNodeIfs = address.Assign(destRtrToDestNode);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Manually add static routes for source to control path selection

    Ptr<Ipv4> ipv4Src = sourceNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingSrc = Ipv4StaticRouting::GetRouting(ipv4Src);

    Ipv4Address destIp = destRtrDestNodeIfs.GetAddress(1); // Destination node IP on its interface

    // Route via Rtr1 (source's first interface)
    routingSrc->AddHostRouteTo(destIp, rtr1DestRtrIfs.GetAddress(0), 1); // via Rtr1

    // Route via Rtr2 (source's second interface)
    routingSrc->AddHostRouteTo(destIp, rtr2DestRtrIfs.GetAddress(0), 2); // via Rtr2

    // Server application on destination node
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(destRtrDestNodeIfs.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(destinationNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client application on source node using TCP
    Ptr<Socket> tcpSocket = Socket::CreateSocket(sourceNode.Get(0), TcpSocketFactory::GetTypeId());

    class MyTcpClient : public Application {
    public:
        MyTcpClient(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
            : m_socket(socket), m_peer(address), m_packetSize(packetSize), m_dataRate(dataRate) {}

        virtual ~MyTcpClient() { m_socket = nullptr; }

    private:
        void StartApplication() override {
            m_sendEvent = Simulator::Schedule(Seconds(0.0), &MyTcpClient::SendPacket, this);
        }
        void StopApplication() override {
            if (m_sendEvent.IsRunning()) Simulator::Cancel(m_sendEvent);
            if (m_socket) m_socket->Close();
        }
        void SendPacket() {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_socket->Send(packet);
            m_nextTxTime = m_packetSize * 8.0 / m_dataRate.GetBitRate();
            m_sendEvent = Simulator::Schedule(Seconds(m_nextTxTime), &MyTcpClient::SendPacket, this);
        }

        Ptr<Socket> m_socket;
        Address m_peer;
        uint32_t m_packetSize;
        DataRate m_dataRate;
        double m_nextTxTime = 0.0;
        EventId m_sendEvent;
    };

    // First transmission: use route via Rtr1
    Address route1Address = sinkAddress;
    Config::Set("/NodeList/0/$ns3::Ipv4/L3Protocol/DefaultRouteEntry", UintegerValue(0)); // select interface 0 (Rtr1)

    Ptr<MyTcpClient> app1 = CreateObject<MyTcpClient>(tcpSocket, route1Address, 1024, DataRate("1Mbps"));
    sourceNode.Get(0)->AddApplication(app1);
    app1->SetStartTime(Seconds(1.0));
    app1->SetStopTime(Seconds(5.0));

    // Second transmission: use route via Rtr2
    Address route2Address = sinkAddress;
    Config::Set("/NodeList/0/$ns3::Ipv4/L3Protocol/DefaultRouteEntry", UintegerValue(1)); // select interface 1 (Rtr2)

    Ptr<MyTcpClient> app2 = CreateObject<MyTcpClient>(tcpSocket, route2Address, 1024, DataRate("1Mbps"));
    sourceNode.Get(0)->AddApplication(app2);
    app2->SetStartTime(Seconds(6.0));
    app2->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}