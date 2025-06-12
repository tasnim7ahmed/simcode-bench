#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer routers;
    routers.Create(3); // R1, R2, R3

    NodeContainer csmaNodes;
    csmaNodes.Create(3); // LAN nodes connected to R2

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer routerR1R2 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer routerR2R3 = p2p.Install(routers.Get(1), routers.Get(2));

    NetDeviceContainer lanDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(routerR1R2);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(routerR2R3);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesLAN = csma.Assign(lanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfacesR2R3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(routers.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(csmaNodes.Get(0), TcpSocketFactory::GetTypeId());

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

    MyApp::~MyApp() { m_socket = nullptr; }

    void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate) {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_dataRate = dataRate;
    }

    void MyApp::StartApplication() {
        m_running = true;
        m_packetsSent = 0;
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

        if (++m_packetsSent < m_nPackets) {
            m_sendEvent = Simulator::Schedule(m_dataRate.CalculateBytesTxTime(m_packetSize), &MyApp::SendPacket, this);
        }
    }

    Ptr<MyApp> app = CreateObject<MyApp>();
    Address serverAddress(InetSocketAddress(interfacesR2R3.GetAddress(1), sinkPort));
    app->Setup(ns3TcpSocket, serverAddress, 1040, 1000, DataRate("1Mbps"));
    csmaNodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));
    p2p.EnablePcapAll("network-simulation");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream outFile("results.csv");
    outFile << "FlowID,Throughput(Mbps),PacketLossRatio,TotalPackets,TotalBytes,TotalDelay(s)\n";

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::stringstream protoStream;
        protoStream << (uint32_t)t.protocol;
        std::string proto = protoStream.str();

        if (t.sourcePort == sinkPort || t.destinationPort == sinkPort) {
            double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            double packetLossRatio = static_cast<double>(it->second.lostPackets) / (it->second.txPackets ? it->second.txPackets : 1);
            outFile << it->first << ","
                    << throughput << ","
                    << packetLossRatio << ","
                    << it->second.txPackets << ","
                    << it->second.rxBytes << ","
                    << it->second.delaySum.GetSeconds() << "\n";
        }
    }

    outFile.close();
    Simulator::Destroy();
    return 0;
}