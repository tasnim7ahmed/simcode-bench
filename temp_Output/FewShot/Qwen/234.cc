#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/socket.h"

using namespace ns3;

class UdpRelayApplication : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpRelayApplication")
            .SetParent<Application>()
            .AddConstructor<UdpRelayApplication>();
        return tid;
    }

    UdpRelayApplication() : m_socket(nullptr) {}
    virtual ~UdpRelayApplication() {}

protected:
    void DoInitialize() override {
        Application::DoInitialize();
        if (m_socket == nullptr) {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
            m_socket->Bind(local);
        }
        m_socket->SetRecvCallback(MakeCallback(&UdpRelayApplication::HandleRead, this));
    }

    void DoDispose() override {
        m_socket->Close();
        Application::DoDispose();
    }

private:
    void StartApplication() override {}
    void StopApplication() override {}

    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            m_relaySocket.Send(packet);
        }
    }

    Ptr<Socket> m_socket;
    UdpSocketImpl m_relaySocket;
};

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: Client, Relay, and Server
    NodeContainer nodes;
    nodes.Create(3);

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link Client <-> Relay
    NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Link Relay <-> Server
    NetDeviceContainer devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // Set up UDP Echo Server on Server node (node 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the relay application on Relay node (node 1)
    UdpRelayApplication relayApp;
    nodes.Get(1)->AddApplication(&relayApp);
    relayApp.SetStartTime(Seconds(1.0));
    relayApp.SetStopTime(Seconds(10.0));

    // Set up UDP client on Client node (node 0)
    UdpEchoClientHelper echoClient(interfaces1.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}