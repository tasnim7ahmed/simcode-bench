#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodesR1R2;
    nodesR1R2.Create(2);

    NodeContainer nodesR2R3;
    nodesR2R3.Add(nodesR1R2.Get(1));
    nodesR2R3.Create(1);

    NodeContainer lanNodes;
    lanNodes.Create(3);
    Ptr<Node> r2 = nodesR1R2.Get(1);
    lanNodes.Add(r2);

    PointToPointHelper p2pR1R2;
    p2pR1R2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pR1R2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2;
    devicesR1R2 = p2pR1R2.Install(nodesR1R2);

    PointToPointHelper p2pR2R3;
    p2pR2R3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pR2R3.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR2R3;
    devicesR2R3 = p2pR2R3.Install(nodesR2R3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NetDeviceContainer devicesLan;
    devicesLan = csma.Install(lanNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(devicesR1R2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(devicesR2R3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesLan = address.Assign(devicesLan);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfacesR2R3.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodesR2R3.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(lanNodes.Get(0), TcpSocketFactory::GetTypeId());

    class MyApp : public Application {
    public:
        MyApp();
        virtual ~MyApp();

        void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);
    private:
        virtual void StartApplication(void);
        virtual void StopApplication(void);

        void ScheduleTx(void);
        void SendPacket(void);

        Ptr<Socket> m_socket;
        Address m_peer;
        uint32_t m_packetSize;
        DataRate m_dataRate;
        EventId m_sendEvent;
        bool m_running;
    };

    MyApp::MyApp()
        : m_socket(0),
          m_peer(),
          m_packetSize(0),
          m_dataRate(),
          m_sendEvent(),
          m_running(false) {
    }

    MyApp::~MyApp() {
        m_socket = nullptr;
    }

    void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate) {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_dataRate = dataRate;
    }

    void MyApp::StartApplication() {
        m_running = true;
        m_socket->Bind();
        m_socket->Connect(m_peer);
        SendPacket();
    }

    void MyApp::StopApplication() {
        m_running = false;

        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }

        if (m_socket) {
            m_socket->Close();
        }
    }

    void MyApp::SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);

        ScheduleTx();
    }

    void MyApp::ScheduleTx() {
        if (m_running) {
            Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
            m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
        }
    }

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1040, DataRate("1Mbps"));
    lanNodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    p2pR1R2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));
    p2pR2R3.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));
    csma.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream outputFile;
    outputFile.open("results.csv");
    outputFile << "FlowID,Source,Dest,PacketsSent,ReceivedPackets,Throughput\n";

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::stringstream protoStream;
        protoStream << t.protocol;
        std::string proto = protoStream.str();

        double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
        outputFile << it->first << ","
                   << t.sourceAddress << ":" << t.sourcePort << ","
                   << t.destinationAddress << ":" << t.destinationPort << ","
                   << it->second.txPackets << ","
                   << it->second.rxPackets << ","
                   << throughput << "\n";
    }

    outputFile.close();
    Simulator::Destroy();

    return 0;
}