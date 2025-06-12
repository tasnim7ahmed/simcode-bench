#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1048576, 1000, DataRate("1Mbps"));
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-point-to-point.tr"));
    pointToPoint.EnablePcapAll("tcp-point-to-point");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    uint64_t totalBytesRx = DynamicCast<PacketSink>(sinkApps.Get(0))->GetTotalRx();
    NS_LOG_UNCOND("Total Bytes Received: " << totalBytesRx);

    return 0;
}

class MyApp : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("MyApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<MyApp>();
        return tid;
    }

    MyApp() : m_socket(nullptr), m_peer(), m_packetSize(0), m_nPackets(0), m_dataRate(0) {}

    virtual ~MyApp() { StopApplication(); }

private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        m_socket->Bind();
        m_socket->Connect(m_peer);
        SendPacket();
    }

    virtual void StopApplication(void) {
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void SendPacket(void) {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);

        if (--m_nPackets > 0) {
            ScheduleTx();
        }
    }

    void ScheduleTx(void) {
        if (m_socket) {
            Time tNext = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
            m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
};