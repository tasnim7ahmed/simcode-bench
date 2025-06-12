#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

class MyApp : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("MyApp")
            .SetParent<Application>()
            .AddConstructor<MyApp>()
            .AddAttribute("DataRate", "The data rate",
                          DataRateValue(DataRate("10Mbps")),
                          MakeDataRateAccessor(&MyApp::m_dataRate),
                          MakeDataRateChecker())
            .AddAttribute("Remote", "The remote address of the server",
                          AddressValue(),
                          MakeAddressAccessor(&MyApp::m_peer),
                          MakeAddressChecker());
    }

    MyApp();
    virtual ~MyApp();

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Address m_peer;
    DataRate m_dataRate;
    uint32_t m_packetSize;
    uint32_t m_totalBytes;
    uint32_t m_bytesSent;
    EventId m_sendEvent;
    bool m_running;
};

MyApp::MyApp()
    : m_peer(), m_dataRate(0), m_packetSize(1024),
      m_totalBytes(2000000), m_bytesSent(0), m_running(false)
{
}

void MyApp::StartApplication(void) {
    m_running = true;
    m_bytesSent = 0;
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    if (m_socket->Connect(m_peer) == -1) {
        NS_FATAL_ERROR("Failed to connect socket");
    }
    m_socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&MyApp::CwndChange, this));
    ScheduleTx();
}

void MyApp::StopApplication(void) {
    m_running = false;

    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket) {
        m_socket->Close();
    }
}

void MyApp::ScheduleTx(void) {
    if (m_running && m_bytesSent < m_totalBytes) {
        Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}

void MyApp::SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    int actual = m_socket->Send(packet);
    if (actual > 0) {
        m_bytesSent += actual;
    }
    ScheduleTx();
}

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    MyAppHelper app;
    app.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    app.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces12.GetAddress(1), 9)));
    ApplicationContainer clientApp = app.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr");
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::TcpSocketBase/CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));

    p2p.EnablePcapAll("tcp-large-transfer");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}