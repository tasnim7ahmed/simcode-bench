#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

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

    uint16_t port = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->SetAttribute("SndBufSize", UintegerValue(16384));
    ns3TcpSocket->SetAttribute("RcvBufSize", UintegerValue(16384));
    Ptr<MyApp> clientApp = CreateObject<MyApp>();
    clientApp->SetSocket(ns3TcpSocket);
    clientApp->SetRemote(InetSocketAddress(interfaces.GetAddress(1), port));
    clientApp->SetAttribute("MaxBytes", UintegerValue(1000000));
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(10.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (5.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    Simulator::Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    }

    Simulator::Destroy();
    return 0;
}

class MyApp : public Application {
public:
    MyApp() : m_socket(0), m_peer(), m_packetSize(1024), m_nPackets(1), m_sent(0) {}
    ~MyApp() { m_socket = 0; }

    void SetSocket(Ptr<Socket> socket) { m_socket = socket; }
    void SetRemote(Address peer) { m_peer = peer; }
    void SetPacketSize(uint32_t packetSize) { m_packetSize = packetSize; }
    void SetNPackets(uint32_t nPackets) { m_nPackets = nPackets; }

    TypeId GetTypeId() const override {
        static TypeId tid = TypeId("MyApp")
            .SetParent<Application>()
            .SetGroupName("Tutorial")
            .AddConstructor<MyApp>();
        return tid;
    }

private:
    virtual void StartApplication(void) override {
        m_socket->Bind();
        m_socket->Connect(m_peer);
        m_socket->SetRecvCallback(MakeCallback(&MyApp::ReceiveData, this));
        m_socket->SetConnectCallback(
            MakeCallback(&MyApp::ConnectionSucceeded, this),
            MakeCallback(&MyApp::ConnectionFailed, this));
        m_socket->SetCloseCallback(MakeCallback(&MyApp::NormalClose, this));
        m_socket->SetSendCallback(MakeCallback(&MyApp::DataSent, this));

        Simulator::Schedule(Seconds(1.0), &MyApp::SendPacket, this);
    }

    virtual void StopApplication(void) override {
        if (m_socket != 0) {
            m_socket->Close();
        }
    }

    void SendPacket(void) {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);
    }

    void ReceiveData(Ptr<Socket> socket) {
    }

    void ConnectionSucceeded(Ptr<Socket> socket) {
        socket->SetRecvCallback(MakeCallback(&MyApp::ReceiveData, this));
    }

    void ConnectionFailed(Ptr<Socket> socket) {
        Simulator::Stop();
    }

    void NormalClose(Ptr<Socket> socket) {
    }

    void DataSent(Ptr<Socket> socket, uint32_t sent) {
        m_sent += sent;
        if(m_sent < 1000000){
           Simulator::Schedule(Seconds(0.00001), &MyApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    uint32_t m_sent;
};