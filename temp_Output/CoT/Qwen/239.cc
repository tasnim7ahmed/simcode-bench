#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PNetwork");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));

    NodeContainer client;
    client.Create(1);
    NodeContainer server;
    server.Create(1);
    NodeContainer router;
    router.Create(1);

    NodeContainer clientRouter = NodeContainer(client.Get(0), router.Get(0));
    NodeContainer routerServer = NodeContainer(router.Get(0), server.Get(0));

    PointToPointHelper p2pClientRouter;
    p2pClientRouter.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pClientRouter.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pRouterServer;
    p2pRouterServer.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRouterServer.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouterDevices = p2pClientRouter.Install(clientRouter);
    NetDeviceContainer routerServerDevices = p2pRouterServer.Install(routerServer);

    InternetStackHelper stack;
    stack.Install(client);
    stack.Install(server);
    stack.Install(router);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.NewNetwork();
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(clientRouterInterfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(client.Get(0), TcpSocketFactory::GetTypeId());

    class MyApp : public Application {
    public:
        MyApp();
        virtual ~MyApp();

        void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
    private:
        virtual void StartApplication(void);
        virtual void StopApplication(void);

        void SendPacket(void);
        Ptr<Socket> m_socket;
        Address m_peer;
        uint32_t m_packetSize;
        uint32_t m_nPackets;
        DataRate m_dataRate;
        EventId m_sendEvent;
        bool m_running;
        uint32_t m_packetsSent;
    };

    MyApp::MyApp()
        : m_socket(0),
          m_peer(),
          m_packetSize(0),
          m_nPackets(0),
          m_dataRate(),
          m_sendEvent(),
          m_running(false),
          m_packetsSent(0) {}

    MyApp::~MyApp() { m_socket = 0; }

    void
    MyApp::Setup(Ptr<Socket> socket, Address peer, uint32_t packetSize, uint32_t nPackets, DataRate dataRate) {
        m_socket = socket;
        m_peer = peer;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_dataRate = dataRate;
    }

    void
    MyApp::StartApplication(void) {
        m_running = true;
        m_packetsSent = 0;
        m_socket->Bind();
        m_socket->Connect(m_peer);
        SendPacket();
    }

    void
    MyApp::StopApplication(void) {
        m_running = false;

        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }

        if (m_socket) {
            m_socket->Close();
        }
    }

    void
    MyApp::SendPacket(void) {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);

        if (++m_packetsSent < m_nPackets) {
            Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
            m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
        }
    }

    Ptr<MyApp> app = CreateObject<MyApp>();
    Address serverAddress(InetSocketAddress(routerServerInterfaces.GetAddress(1), sinkPort));
    app->Setup(ns3TcpSocket, serverAddress, 1024, 20, DataRate("1Mbps"));
    client.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}